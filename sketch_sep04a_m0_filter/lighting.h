#define PLOTTER_LIGHT 1

// pixel strip / light show config
// reminder, mic is using PIN 2
#define LED_PIN    0  // NeoPixel LED strand is connected to GPIO #0 / D0
#define N_PIXELS  8  // Number of pixels you are using
#define TOP N_PIXELS / 4 // effectively number of pixels per band
//#define LIGHT_NOISE 20 
#define LSAMPLES 32    //rotating buffer size, power of 2 makes for easy average

#define LVLS_PER_PIXEL 16
#define PIXEL_REMAINDER LVLS_PER_PIXEL - 1
#define PIXEL_SHIFT_DIVIDER 3

#define MAX_LVL 512
#define MIN_LVL 0
#define MIN_LVL_DIFF 32

int filteredSamples[4][LSAMPLES];



int tops[4] = {MIN_LVL, MIN_LVL, MIN_LVL, MIN_LVL};
int lows[4] = {MAX_LVL, MAX_LVL, MAX_LVL, MAX_LVL}; 

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup_strip()
{
  strip.begin();
  strip.clear();
  strip.show();
}

// i - 0 - 3 - being the frequency range represented on the pixel strip
// y - being the intensity
// r,b,g being the base colors
void set_band_color(byte i, int32_t y, uint8_t r, uint8_t g, uint8_t b)
{
  int lowAdjust = 0;
  int rem;
  int itop = i * TOP; // starting pixel for this band

  //int lvl = map(abs(512-y), 0, 512, 0, TOP*16+64)>>2; // might need to dampen elsewhere rather than use map to achieve this
  int absy = abs(512-y);

  // attempt 
  if(absy > tops[i]) {
    tops[i] = absy;
  } 
  if (absy < lows[i]) {
    lows[i] = absy;
  }

  // ensure top is greater than low by MIN_LVL_DIFF, and ensure
  // top is not out of bounds since if its too close it is set as
  // low + MIN_LVL_DIFF
  if(tops[i] - MIN_LVL_DIFF < lows[i]  ) {
    tops[i] = lows[i] + MIN_LVL_DIFF;
    lowAdjust = max(tops[i], MAX_LVL) - 512; 
  }
  lows[i] -= lowAdjust;

  // instead of 0 and 512 for the range of the map, use the lows and the tops to
  // allow more of the band to be filled on relative loud samples (and have it be blank on relative
  // quiet samples). 
  int lvl = map(absy>>2, lows[i], tops[i]>>2, 0, TOP*LVLS_PER_PIXEL+64); // might need to dampen elsewhere rather than use map to achieve this
  
  //Serial.print("top: ");
  //Serial.print(TOP);
  //Serial.print(" lvl: ");
  //Serial.println(y);
  //Serial.println(lvl);
  
  #ifdef PLOTTER_LIGHT
  if(i == 0){
  Serial.print(0);
  Serial.print("\t");
  Serial.print(512);
  Serial.print("\t");
  Serial.print(lows[i]);
  Serial.print("\t");
  Serial.print(tops[i]);
  Serial.print("\t");
  Serial.println(absy);
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
  set_band_color(0, lowest_band, 10, 0, 0);
  set_band_color(1, mid_band, 0, 10, 0);
  set_band_color(2, high_band, 0, 0, 10);
  set_band_color(3, highest_band, 10, 0, 10);
  strip.show();
}
