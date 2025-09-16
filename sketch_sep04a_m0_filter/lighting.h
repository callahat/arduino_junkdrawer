// pixel strip / light show config
// reminder, mic is using PIN 2
#define LED_PIN    0  // NeoPixel LED strand is connected to GPIO #0 / D0
#define N_PIXELS  8  // Number of pixels you are using
#define TOP N_PIXELS / 4 // effectively number of pixels per band
//#define LIGHT_NOISE 20 
#define LSAMPLES 32    //rotating buffer size, power of 2 makes for easy average

int filteredSamples[4][LSAMPLES];

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
  int rem;
  int itop = i * TOP;
  //int lvl = map(y, 0, 1023, 0, TOP*16-1)*31>>5; // might need to dampen elsewhere rather than use map to achieve this
  int lvl = map(abs(512-y), 0, 512, 0, TOP*16+64)>>2; // might need to dampen elsewhere rather than use map to achieve this
  Serial.print("top: ");
  Serial.print(TOP);
  Serial.print(" lvl: ");
  Serial.println(y);
  Serial.println(lvl);
  for(int ix=0; ix < TOP; ix++){
    rem = lvl & 15; // remainder
    lvl >>= 4;      // divide by 16 
    
    //if(lvl > ix) {
      strip.setPixelColor(itop + ix, rem * r, rem * g, rem * b);
    //} else {
    //  strip.setPixelColor(itop + ix, 0, 0, 0);
    //}
  }
}

// power readings from the four bands we are filtering into
void update_light_strip(int32_t lowest_band, int32_t mid_band, int32_t high_band, int32_t highest_band)
{
  set_band_color(0, lowest_band, 10, 0, 0);
  set_band_color(1, mid_band, 0, 10, 0);
  set_band_color(2, high_band, 0, 0, 10);
  set_band_color(3, highest_band, 10, 0, 10);
  strip.show();
}
