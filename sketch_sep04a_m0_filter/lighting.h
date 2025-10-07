// plotter debugging
// #define DEBUGGING_PLOT_LIGHT 1
#define DEBUGGING_LIGHT_UPDATES // serial how many light updates per second occur

// pixel strip / light show config
// reminder, mic is using PIN 2
#define LED_PIN    0  // NeoPixel LED strand is connected to GPIO #0 / D0
#define N_PIXELS  8  // Number of pixels you are using
#define TOP N_PIXELS / 4 // effectively number of pixels per band, number of pixels / 4 (since there are 4 bands)

// Averaging the last X samples to produce a smoother graph of LED intensities,
// LSAMPLES closer to zero can have more rapid bright/off switching strobe effect.
// Averaging also has a side effect of dampening, so the dampening shifters should be reduced.
// This should not be too high; the lights only update ~260-270 times a second with this on,
// and it also seems to add a bit of a lag to the light.
#define USE_AVERAGE_SAMPLING // Comment this out to disable averaging the samples to get a smoother graph
#define LSAMPLES 8    //rotating buffer size, power of 2 makes for easy average
#define LSAMPLE_SHIFT_DIVIDER 3    // 2^n = LSAMPLES
#define LSAMPLES_MASK LSAMPLES - 1 // A quick way to enforce a rolling incremented index

// Effectively use these to throw out low amplitude "noise"
// Should be adjusted lower when there are more pixels (need to check this, might need more dampening?) and
//   lower when there are more samples averaged
// Setting the LOW dampener higher will cut out queiter noises from lighting up the band 
#define LOWS_DAMPEN_SHIFTER 4
// Setting the TOP Dampener higher will skew the band in the more "light up" direction
#define TOPS_DAMPEN_SHIFTER 4

// How many levels of brightness per pixel. Each band is divided into pixels with levels of brightness.
// Should be a power of two for faster division via bit shifting
#define LVLS_PER_PIXEL 16
#define PIXEL_REMAINDER LVLS_PER_PIXEL - 1
#define PIXEL_SHIFT_DIVIDER 3 // another power of two for a quick division

#define MAX_LVL 512
#define MIN_LVL 0
// Don't let the relative high/low decay to the point where even very quiet is filling up the whole light band
#define MIN_LVL_DIFF 64
// Decay and rate for the local high levels; allows the MAX to slide down so that
// for quieter sounds the full band can still light up all the way for a relative peak
#define DECAY_MS 500
#define DECAY_LEVEL 10

unsigned long decayMillis = 0, currentMillis;

// Tracking the past samples to average
#if defined USE_AVERAGE_SAMPLING
int filteredSamples[4][LSAMPLES];
int filteredSampleSum[4] = {0, 0, 0, 0};
int lSampleIndex = 0; // last sample index
#endif

int tops[4] = {MIN_LVL, MIN_LVL, MIN_LVL, MIN_LVL};
int lows[4] = {MAX_LVL, MAX_LVL, MAX_LVL, MAX_LVL}; 

#ifdef DEBUGGING_LIGHT_UPDATES
unsigned long debugLightUpdateMillis;
int updateLightStripCalls;
#endif

#include <Adafruit_NeoPixel.h>
// 8 neopixel strip setup
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
// Pebble strand setup
// Adafruit_NeoPixel strip= Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_BGR + NEO_KHZ800); // pebble coloring

void setup_strip()
{
  strip.begin();
  strip.clear();
  strip.show();
}

int processSample(byte i, int absy) {
#if defined USE_AVERAGE_SAMPLING
  filteredSampleSum[i] = filteredSampleSum[i] - filteredSamples[i][lSampleIndex] + absy;
  filteredSamples[i][lSampleIndex] = absy;

  return(filteredSampleSum[i] >> LSAMPLE_SHIFT_DIVIDER);
#else
  return(absy);
#endif
}

// i - 0 - 3 - being the frequency range represented on the pixel strip
// y - being the intensity
// r,b,g being the base colors
void set_band_color(byte i, int32_t y, uint8_t r, uint8_t g, uint8_t b)
{
  int lowAdjust = 0;
  int rem;
  int itop = i * TOP; // starting pixel for this band

  int absy = abs(MAX_LVL-y);

  int sample = processSample(i, absy);

  // attempt 
  if(sample > tops[i]) {
    tops[i] = sample;
  } 
  if (sample < lows[i]) {
    lows[i] = sample;
  }
  
  // ensure top is greater than low by MIN_LVL_DIFF, and ensure
  // top is not out of bounds since if its too close it is set as
  // low + MIN_LVL_DIFF
  if(tops[i] - MIN_LVL_DIFF < lows[i]) {
    tops[i] = lows[i] + MIN_LVL_DIFF;
    lowAdjust = max(tops[i], MAX_LVL) - MAX_LVL; 
  }
  lows[i] -= lowAdjust;
  lows[i] = max(lows[i], 0);

  // instead of 0 and 512 for the range of the map, use the lows and the tops to
  // allow more of the band to be filled on relative loud samples (and have it be blank on relative
  // quiet samples). 
  int lvl = map(sample>>LOWS_DAMPEN_SHIFTER, lows[i]>>LOWS_DAMPEN_SHIFTER, tops[i]>>TOPS_DAMPEN_SHIFTER, 0, TOP*LVLS_PER_PIXEL);
  
  #ifdef DEBUGGING_PLOT_LIGHT
  if(i == 2){
  Serial.print(0);
  Serial.print("\t");
  Serial.print(512);
  Serial.print("\t");
  Serial.print(lows[i]);
  Serial.print("\t");
  Serial.print(tops[i]);
  Serial.print("\t");
  Serial.print(absy);
  Serial.print("\t");
  Serial.println(sample);
  }
  #endif
  
  for(int ix=0; ix < TOP; ix++){
    rem = lvl & PIXEL_REMAINDER; // remainder
    lvl >>= PIXEL_SHIFT_DIVIDER;      // divide by 16 

    strip.setPixelColor(itop + ix, rem * r, rem * g, rem * b);
  }
}

// power readings from the four bands we are filtering into
// each power value should range from 0-1024, with "quiet" being around 512.
void update_light_strip(int32_t lowest_band, int32_t mid_band, int32_t high_band, int32_t highest_band)
{
  currentMillis = millis();
  
  #ifdef DEBUGGING_LIGHT_UPDATES
  updateLightStripCalls++;
  if(currentMillis - 1000 > debugLightUpdateMillis) {
    Serial.println(updateLightStripCalls);
    updateLightStripCalls = 0;
    debugLightUpdateMillis = currentMillis;
  }
  #endif

  if(currentMillis - decayMillis > DECAY_MS) {
    decayMillis = currentMillis;
    for(byte ix=0; ix<4; ix++){
      tops[ix] -= DECAY_LEVEL;
      lows[ix] += 1;
    }
  }

  #ifdef USE_AVERAGE_SAMPLING
  lSampleIndex += 1;
  lSampleIndex &= LSAMPLES_MASK;
  #endif

  set_band_color(0, lowest_band, 10, 0, 0);
  set_band_color(1, mid_band, 0, 10, 0);
  set_band_color(2, high_band, 0, 0, 10);
  set_band_color(3, highest_band, 10, 0, 10);
  strip.show();
}
