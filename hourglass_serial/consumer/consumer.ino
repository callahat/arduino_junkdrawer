
#define LED_PIN    0  // NeoPixel LED strand is connected to GPIO #0 / D0
#define N_PIXELS  8  // Number of pixels you are using
#define TOP N_PIXELS / 4 // effectively number of pixels per band, number of pixels / 4 (since there are 4 bands)

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

char state[100] = "hello";

int pixelBuff[N_PIXELS];

void setup() {
  // put your setup code here, to run once:
  //Serial.begin(115200);
  Serial1.begin(9600);
  //  Serial.println("setting up");
  strip.begin();  // initialize the strip
  strip.clear();  // Initialize all pixels to 'off'
  strip.show();   // make sure it is visible

  strip.setPixelColor(0, 50, 0, 0);
  strip.show();
  
  strip.setPixelColor(0, 0, 50, 0);
  strip.show();
  //  Serial.println("setting up done");
}

int flip = 1;

size_t l;

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial1.available()) {
    Serial.println("reading bytes");
    l = Serial1.readBytesUntil('\n', state, 100);
    state[l+1] = 0;
    Serial.println(state);
    flip *= -1;
    colorPixels();
    strip.show();
  }
}

int tmpPixel, i;

void colorPixels() {
   for( i = 0; i < l; i++ ) {
     tmpPixel = state[i] - '0';
     
     strip.setPixelColor(i, ((tmpPixel & 4) / 4) * 50, ((tmpPixel & 2) / 2) * 50, (tmpPixel & 1) * 50);
   }
}
