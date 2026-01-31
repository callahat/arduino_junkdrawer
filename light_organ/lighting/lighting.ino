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
#define SERIAL_BAUD 19200

// Debugging defines ----------------------------------------------------
// Plot sample, processed sample, and tops/lows for the given band
//#define DEBUG_PLOT_LIGHT 2
// Echo what comes across Serial1
//#define DEBUG_SERIAL1_READ
//#define PLOTTER 1
// blink the dotstar for every 800 bytes read
//#define MONITOR_SERIAL_READS 1

// pixel strip / light show config
#define LED_PIN    0  // NeoPixel LED strand is connected to GPIO #0 / D0
//#define N_PIXELS  8  // Number of pixels you are using, 8 for bar/stick
#define N_PIXELS  100  // Pebble strand (~30m) has 100 pixels
#define TOP N_PIXELS / 4 // effectively number of pixels per band, number of pixels / 4 (since there are 4 bands)

/* Averaging the last X samples to produce a smoother graph of LED intensities,
 * LSAMPLES closer to zero can have more rapid bright/off switching strobe effect.
 * Averaging also has a side effect of dampening, so the dampening shifters should be reduced.
 * This should not be too high; the lights only update ~260-270 times a second with this on,
 * and it also seems to add a bit of a lag to the light.
 */
//#define USE_AVERAGE_SAMPLING // Comment this out to disable averaging the samples to get a smoother graph
#define LSAMPLES 8    //rotating buffer size, power of 2 makes for easy average
#define LSAMPLE_SHIFT_DIVIDER 2    // 2^n = LSAMPLES
#define LSAMPLES_MASK LSAMPLES - 1 // A quick way to enforce a rolling incremented index

// Effectively use these to throw out low amplitude "noise"
// Should be adjusted lower when there are more pixels (need to check this, might need more dampening?) and
//   lower when there are more samples averaged
// Setting the LOW dampener higher will cut out queiter noises from lighting up the band 
#define DAMPEN_SHIFTER 4

// How many levels of brightness per pixel. Each band is divided into pixels with levels of brightness.
// Should be a power of two for faster division via bit shifting
#define LVLS_PER_PIXEL 16
#define PIXEL_REMAINDER LVLS_PER_PIXEL - 1

#define MAX_LVL 512
#define MIN_LVL 0
// Don't let the relative high/low decay to the point where even very quiet is filling up the whole light band
#define MIN_LVL_DIFF 128
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
// neopixel setup
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
// Pebble strand setup
//Adafruit_NeoPixel strip= Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_BGR + NEO_KHZ800); // pebble coloring

#include <Adafruit_DotStar.h>
// Init DotStar module
Adafruit_DotStar star = Adafruit_DotStar(1, 7, 8, DOTSTAR_BGR);

#ifdef MONITOR_SERIAL_READS
int processedSerialCount = 0;
bool processedSerialCountLightOn = false;
#endif

void setup() {
  Serial.begin(9600);
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
  readByte();
  processBytePair();
  updateLightStrip();
}


#define FIRST_POSITION_BIT  1 << 7   // the bit indicating if the data byte is the first or second of the pair
#define SECOND_POSITION_BIT 0 << 7
#define LOW_BAND_BITS       0 << 5   // the two bits indicating what band the data byte is for
#define MID_BAND_BITS       1 << 5
#define HIGH_BAND_BITS      2 << 5
#define HIGHEST_BAND_BITS   3 << 5
#define BAND_MASK HIGHEST_BAND_BITS  // Mask for the bits indicating the band
#define FIVE_BIT_MASK 31             // Mask for the power level segment for the data byte

#define BYTE_BUFFER_LEN 64
#define BYTE_BUFFER_MASK BYTE_BUFFER_LEN - 1

uint8_t byteBuffer[BYTE_BUFFER_LEN];
uint8_t byteBufferWritePos = 0;
uint8_t byteBufferToProcess = 0;
uint8_t byteBufferFresh = 0;

void readByte() {
  while(Serial1.available() > 0) {
    byteBuffer[byteBufferWritePos] = Serial1.read();

    #ifdef MONITOR_SERIAL_READS
    processedSerialCount++;
    if(processedSerialCount > 800) {
      processedSerialCount = 0;
      processedSerialCountLightOn = true;
      star.setPixelColor(0, 0, 0, 255);
      star.show();
    } else if(processedSerialCountLightOn && processedSerialCount > 5) {
      processedSerialCountLightOn = false;
      star.setPixelColor(0, 0, 0, 0);
      star.show();
    }
    #endif

    byteBufferFresh++;
    byteBufferWritePos++;
    byteBufferWritePos &= BYTE_BUFFER_MASK;
  }
}

void processBytePair() {
  byte iBandNum;
  uint16_t tmpSample;

  if(byteBufferFresh > 1) {
    if((byteBuffer[byteBufferToProcess] & FIRST_POSITION_BIT) == FIRST_POSITION_BIT) {
      #ifdef DEBUG_SERIAL1_READ
        Serial.println("Found first byte");
        Serial.println("Adjacent bytes:");
        Serial.println(256 | byteBuffer[byteBufferToProcess], BIN);
        Serial.println(256 | byteBuffer[byteBufferToProcess + 1], BIN);
      #endif
      if((byteBuffer[byteBufferToProcess + 1] & FIRST_POSITION_BIT) == 0) {
        #ifdef DEBUG_SERIAL1_READ
          Serial.println("got a first, second byte in order");
        #endif
        iBandNum = byteBuffer[byteBufferToProcess] & BAND_MASK;
        
        // A first and second byte have been found in order
        if(iBandNum == (byteBuffer[byteBufferToProcess + 1] & BAND_MASK)) {
          // Its a match, these two bytes most likely correlate to the upper and lower 5 bits of the band
          iBandNum = iBandNum >> 5;
          #ifdef DEBUG_SERIAL1_READ
            Serial.print("bytes are for same band: ");
            Serial.println(iBandNum);
          #endif

          tmpSample = ((byteBuffer[byteBufferToProcess] & FIVE_BIT_MASK) << 5) |
            (byteBuffer[byteBufferToProcess + 1] & FIVE_BIT_MASK);

          #ifdef DEBUG_SERIAL1_READ
            Serial.println("calcualted sample: ");
            Serial.println(256 | (byteBuffer[byteBufferToProcess]), BIN);
            Serial.println(256 | (byteBuffer[byteBufferToProcess + 1]), BIN);
            Serial.println(tmpSample);
          #endif
          if(tmpSample > 0 && tmpSample < 1023) {
            currentSample[iBandNum] = tmpSample;
          }
        }

        byteBufferFresh -= 2;
        byteBufferToProcess += 2;
      }
    } else {
      // a second byte was encountered, advance the next to process in the buffer by one
      byteBufferFresh--;
      byteBufferToProcess++;
    }
    // ensure the working index stays within the array bounds
    byteBufferToProcess &= BYTE_BUFFER_MASK;
  }

  #ifdef PLOTTER
  Serial.print(currentSample[3]+512);
  Serial.print("\t");
  Serial.print(currentSample[2]+384);
  Serial.print("\t");
  Serial.print(currentSample[1]+256);
  Serial.print("\t");
  Serial.print(currentSample[0]+128);
  Serial.print("\t384\t1128");
  Serial.println();
  #endif
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
  
  #ifdef DEBUG_PLOT_LIGHT
  if(i == DEBUG_PLOT_LIGHT){
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

  colorBand2(i, sample, r, g, b);
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

  set_band_color(0, currentSample[0], 10, 0, 0);  // low
  set_band_color(1, currentSample[1], 0, 10, 0);  // mid
  set_band_color(2, currentSample[2], 0, 0, 10);  // high
  set_band_color(3, currentSample[3], 5, 0, 5); // highest
  strip.show();
}

// Methods for coloring or mapping the bands
int lvl, rem;

// Basic linear mapping where the bands are divided into 4 sections,
// the higher the lvl the more pixels are activated.
void colorBand(int i, int sample, uint8_t r, uint8_t g, uint8_t b) {
  // instead of 0 and 512 for the range of the map, use the lows and the tops to
  // allow more of the band to be filled on relative loud samples (and have it be blank on relative
  // quiet samples).
  lvl = map(sample>>DAMPEN_SHIFTER, lows[i]>>DAMPEN_SHIFTER, tops[i]>>DAMPEN_SHIFTER, 0, TOP*LVLS_PER_PIXEL);

  int itop = i * TOP; // starting pixel for this band
  for(int ix=0; ix < TOP; ix++){
    calcRemAndLvl();

    strip.setPixelColor(itop + ix, rem * r, rem * g, rem * b);
  }
}

// Low Mid High Highest
uint8_t bandLevels[4] = {4, 4, 3, 3};
uint8_t pixelsPerLevel[4][4] = {
    {4, 4, 2, 4},
    {3, 4, 2, 4},
    {4, 2, 4, 0},
    {4, 2, 4, 0}
  };
uint8_t pixelMapping[4][4][6] = {
  { // low
    {40, 44, 46, 42, 0, 0}, // level 1
    {26, 27, 34, 35, 0, 0}, // level 2
    {10, 22,  0 , 0, 0, 0}, // level 3
    { 9, 11, 21, 23, 0, 0}  // level 4
  },
  { // mid
    {41, 43, 45,  0, 0, 0}, // level 1
    {30, 31, 38, 39, 0, 0}, // level 2
    { 4, 16,  0,  0, 0, 0}, // level 3
    { 3,  5, 15, 17, 0, 0}  // level 4
  },
  { // high
    {24, 25, 32, 33, 0, 0}, // level 1
    { 1, 13,  0,  0, 0, 0}, // level 2
    { 0,  2, 12, 14, 0, 0}  // level 3
  },
  { // highest
    {28, 29, 36, 37, 0, 0}, // level 1
    { 7, 19,  0,  0, 0, 0}, // level 2
    { 6,  8, 18, 20, 0, 0}  // level 2
  }
};
int bLevels, lPixels;
void colorBand2(int i, int sample, uint8_t r, uint8_t g, uint8_t b) {
  // instead of 0 and 512 for the range of the map, use the lows and the tops to
  // allow more of the band to be filled on relative loud samples (and have it be blank on relative
  // quiet samples).
  lvl = map(sample>>DAMPEN_SHIFTER, lows[i]>>DAMPEN_SHIFTER, tops[i]>>DAMPEN_SHIFTER, 0, bandLevels[i]*LVLS_PER_PIXEL);

  //bLevels = bandLevels[i];
  for(int iLevel=0; iLevel < bandLevels[i]; iLevel++){
    calcRemAndLvl();

   // lPixels = pixelsPerLevel[i][iLevel];
    for(int iPixel=0; iPixel < pixelsPerLevel[i][iLevel]; iPixel++){
    //for(int iPixel=0; iPixel < 3; iPixel++){
      strip.setPixelColor(pixelMapping[i][iLevel][iPixel], rem * r, rem * g, rem * b);
    }
  }
}

void calcRemAndLvl() {
  if(lvl <= 0) {
    rem = 0;
  } else {
    if(lvl > LVLS_PER_PIXEL) {
      rem = LVLS_PER_PIXEL - 1;
    } else {
      rem = lvl;
    }
    lvl -= LVLS_PER_PIXEL;
  }
}
