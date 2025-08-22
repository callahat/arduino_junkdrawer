/********************************************************************************************************
---------------------------------------------------------------------------------------------------------
NeoPixel Information for initializing the strip, below
  60ma/pixel for current load
  Parameter 1 = number of pixels in strip
  Parameter 2 = pin number (most are valid)
  Parameter 3 = pixel type flags, add together as needed:
    NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
    NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
    NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
    NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)

TJC - a simple "hourglass" that drops a color toward one end, changing the color of the last
  pixel. when the pixel is complete, it continues adding the colors to the next pixel
  and so forth. when all pixels are complete, it goes in reverse.
  the grain of sand is represented by a change in the active pixel either one delta
  up or down.

**********************************************************************************************************/

#include <Adafruit_NeoPixel.h>


#define DELAY_TIME (250)   // a pre-processor macro in ms
#define NUM_PIXELS (8)      // how many neo pixels we have
#define NEO_PIXEL_PIN (1)   // the data pin for the NeoPixels
#define COMPLETED_PIXEL (7) // one million has reach RGB at 100
#define CLEARED_PIXEL (0)   // back to black

int pixelBuff[NUM_PIXELS];

// Instatiate the NeoPixel from the ibrary
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);

int completedPixel = COMPLETED_PIXEL;
int nextPixel = 0;
int delta = 1;
int i;
   
void setup() {
  strip.begin();  // initialize the strip
  strip.clear();  // Initialize all pixels to 'off'
  strip.show();   // make sure it is visible

//  allOnPixelBuff();
//  colorPixels();
//  strip.show();
//  delay(3000);
  
  setupPixelBuff();
  
  Serial.begin(9600);
}

void loop() {
  activate();

  // delay for the purposes of debouncing the switch
  delay(DELAY_TIME);
}

// the adjustStarts() function will move the starting point of the for loop to imitiate a cycling effect
void activate() {
  pixelBuff[nextPixel] += delta;

  // undo the last coloring unless it just started
  if(nextPixel - 1 >= 0) {
    pixelBuff[nextPixel - 1] -= delta;
  }

  nextPixel += 1;

  if(nextPixel >= NUM_PIXELS) { 
    Serial.println("reached the end of strip, return next pixel to start");
    nextPixel = 0;
  }
  
  if((delta > 0 && pixelBuff[nextPixel] >= completedPixel) ||
      (delta < 0 && pixelBuff[nextPixel] <= completedPixel)) {
    Serial.println("reached a completed pixel, return next pixel to start");
    pixelBuff[nextPixel] = completedPixel;
    nextPixel = 0;
  }

  if((delta > 0 && pixelBuff[nextPixel] >= completedPixel) ||
     (delta < 0 && pixelBuff[nextPixel] <= completedPixel)) {
    delta *= -1;
    if(delta > 0) {
      completedPixel = COMPLETED_PIXEL;
    } else {
      completedPixel = CLEARED_PIXEL;
    }
  }

  Serial.print("Next Pixel: ");
  Serial.println(nextPixel);
  Serial.print("delta: ");
  Serial.println(delta);
  Serial.print("completed pixel: ");
  Serial.println(completedPixel);
  Serial.println("Coloring pixels");
  colorPixels();


  // debug dot
//  strip.setPixelColor(nextPixel, 40,40,40);
//  strip.show();
//  delay(50);
//  strip.setPixelColor(nextPixel, 0,0,0);

  strip.show();
}

void setupPixelBuff() {
  for (i = 0; i < NUM_PIXELS; i++){
    pixelBuff[i] = 0; // off
  }
}

//void allOnPixelBuff() {
//  //for (int i = 0; i < NUM_PIXELS; i++){
//  //  pixelBuff[i] = 7; // on
//  //}
//  pixelBuff[0] = 7;
//  pixelBuff[1] = 5;
//  pixelBuff[2] = 6;
//  pixelBuff[3] = 0;
//  pixelBuff[4] = 7; //white?
//  pixelBuff[5] = 3;
//  pixelBuff[6] = 3;
//  pixelBuff[7] = 3;
//}

void colorPixels() {
    // color the pixels, probably don't need to paint them all as we know the last one to change
  // so not much should be different. this can be cleaned up.
  for( i = 0; i < NUM_PIXELS; i++ ) {
    // all RGB are capped at 100
    Serial.print(i);
    Serial.print(" ");
    Serial.print(((pixelBuff[i] & 4) >> 2) * 5);
    Serial.print(" ");    
    Serial.print(((pixelBuff[i] & 2) >> 1) * 5);
    Serial.print(" ");
    Serial.print((pixelBuff[i] & 1) * 5);
    Serial.println(" ");
    strip.setPixelColor(i, ((pixelBuff[i] & 4) / 4) * 50, ((pixelBuff[i] & 2) / 2) * 50, (pixelBuff[i] & 1) * 50);
  }
}
