
#define DELAY_TIME (250)   // a pre-processor macro in ms
#define NUM_PIXELS (8)      // how many neo pixels we have
#define NEO_PIXEL_PIN (2)   // the data pin for the NeoPixels
#define COMPLETED_PIXEL (7) // one million has reach RGB at 100
#define CLEARED_PIXEL (0)   // back to black

int pixelBuff[NUM_PIXELS];

int completedPixel = COMPLETED_PIXEL;
int nextPixel = 0;
int delta = 1;
int i;

void setup() {
  // put your setup code here, to run once:
 // Serial.begin(115200);
  Serial1.begin(9600);

  
  setupPixelBuff();
  
}



void setupPixelBuff() {
  for (i = 0; i < NUM_PIXELS; i++){
    pixelBuff[i] = 0; // off
  }
}

void loop() {

  activate();

  // delay for the purposes of debouncing the switch
  delay(DELAY_TIME);
}

long longColor;

void activate() {
  pixelBuff[nextPixel] += delta;

  // undo the last coloring unless it just started
  if(nextPixel - 1 >= 0) {
    pixelBuff[nextPixel - 1] -= delta;
  }

  nextPixel += 1;

  if(nextPixel >= NUM_PIXELS) { 
//    Serial.println("reached the end of strip, return next pixel to start");
    nextPixel = 0;
  }
  
  if((delta > 0 && pixelBuff[nextPixel] >= completedPixel) ||
      (delta < 0 && pixelBuff[nextPixel] <= completedPixel)) {
//    Serial.println("reached a completed pixel, return next pixel to start");
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

/*  Serial.print("Next Pixel: ");
  Serial.println(nextPixel);
  Serial.print("delta: ");
  Serial.println(delta);
  Serial.print("completed pixel: ");
  Serial.println(completedPixel);
  Serial.println("Coloring pixels");*/
    for( i = 0; i < NUM_PIXELS; i++ ) {
      Serial.print(pixelBuff[i]);
      Serial1.print(pixelBuff[i]);
    }
  Serial.println();
  Serial1.println();
}
