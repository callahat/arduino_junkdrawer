/*
--------------------------------------------------------------------------
Copyright 2025 callahat

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
--------------------------------------------------------------------------

Serial Light Organ
==================

*/

// This MUST match the baud rate in the filter file
//#define SERIAL_BAUD 250000
#define SERIAL_BAUD 115200

// Debugging serial prints
// Plot sample, processed sample, and tops/lows for the given band
//#define DEBUGGING_PLOT_LIGHT 2
// Echo what comes across Serial1
#define DEBUGGING_SERIAL1_READ
//#define DEBUGGING_CURRENT_SAMPLE

// pixel strip / light show config
#define LED_PIN    0  // NeoPixel LED strand is connected to GPIO #0 / D0
//#define N_PIXELS  8  // Number of pixels you are using
#define N_PIXELS  100  // Pebble strand
#define TOP N_PIXELS / 4 // effectively number of pixels per band, number of pixels / 4 (since there are 4 bands)

/* Averaging the last X samples to produce a smoother graph of LED intensities,
 * LSAMPLES closer to zero can have more rapid bright/off switching strobe effect.
 * Averaging also has a side effect of dampening, so the dampening shifters should be reduced.
 * This should not be too high; the lights only update ~260-270 times a second with this on,
 * and it also seems to add a bit of a lag to the light.
 */
//#define USE_AVERAGE_SAMPLING // Comment this out to disable averaging the samples to get a smoother graph
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

int currentSample[4] = {512, 512 ,512, 512};
int tops[4] = {MIN_LVL, MIN_LVL, MIN_LVL, MIN_LVL};
int lows[4] = {MAX_LVL, MAX_LVL, MAX_LVL, MAX_LVL}; 

#include <Adafruit_NeoPixel.h>
// 8 neopixel strip setup
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
// Pebble strand setup
// Adafruit_NeoPixel strip= Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_BGR + NEO_KHZ800); // pebble coloring

#include <Adafruit_DotStar.h>
// Init DotStar module
Adafruit_DotStar star = Adafruit_DotStar(1, 7, 8, DOTSTAR_BGR);

char serialBuffer[100];
size_t l;

// byte iSample, iBuff;

#define MONITOR_SERIAL_READS 1
#ifdef MONITOR_SERIAL_READS
int processedSerialCount = 0;
bool processedSerialCountLightOn = false;
#endif

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial1.begin(SERIAL_BAUD);

  star.begin();
  star.setBrightness(50);
  star.setPixelColor(0, 0, 0, 0);
  star.show();

  strip.begin();
  strip.clear();
  strip.show();
}

void loop() {
  readSerialData();
  parseSerialData();
  updateLightStrip();
}

void readSerialData() {
  if(Serial1.available()) {
    l = Serial1.readBytesUntil('\n', serialBuffer, 100);
    serialBuffer[l+1] = 0;
    
    #ifdef DEBUGGING_SERIAL1_READ
    Serial.print("Read:");
    Serial.println(serialBuffer);
    #endif
    
    #ifdef MONITOR_SERIAL_READS
    processedSerialCount++;
    if(processedSerialCount > 100) {
      Serial.println("ON");
      processedSerialCount = 0;
      processedSerialCountLightOn = true;
      star.setPixelColor(0, 0, 0, 255);
      star.show();
    } else if(processedSerialCountLightOn && processedSerialCount > 5) {
      Serial.println("OFF");
      processedSerialCountLightOn = false;
      star.setPixelColor(0, 0, 0, 0);
      star.show();
    }
    #endif
  }
}

void parseSerialData() {
  byte iSample = 0;
  byte iBuff = 0;

  int tmpSample;

  while(serialBuffer[iBuff] != 0 && iSample < 4 && iBuff < 100) {
    tmpSample = 0;
    
    while(serialBuffer[iBuff] != ',' && serialBuffer[iBuff] != 0) {
      if(serialBuffer[iBuff] >= '0' and serialBuffer[iBuff] <= '9') {
        tmpSample *= 10;
        tmpSample += serialBuffer[iBuff] - '0';
      }

      iBuff++;
    }
    iBuff++;

    if(tmpSample > 0 && tmpSample < 1023) { 
      currentSample[iSample] = tmpSample;
    }
    iSample++;
  }
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
  if(i == DEBUGGING_PLOT_LIGHT){
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
    if(lvl > 0) { rem = PIXEL_REMAINDER; }

    strip.setPixelColor(itop + ix, rem * r, rem * g, rem * b);
  }
}

// power readings from the four bands we are filtering into
// each power value should range from 0-1024, with "quiet" being around 512.
void updateLightStrip()
{
  currentMillis = millis();
  
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
  #ifdef DEBUGGING_CURRENT_SAMPLE
  Serial.print("current_sample:");
  Serial.print(currentSample[0]);
  Serial.print(",");
  Serial.print(currentSample[1]);
  Serial.print(",");
  Serial.print(currentSample[2]);
  Serial.print(",");
  Serial.print(currentSample[3]);
  Serial.println();
  #endif

  set_band_color(0, currentSample[0], 10, 0, 0);  // low
  set_band_color(1, currentSample[1], 0, 10, 0);  // mid
  set_band_color(2, currentSample[2], 0, 0, 10);  // high
  set_band_color(3, currentSample[3], 10, 0, 10); // highest
  strip.show();
}
