#include <Arduino.h>
#include <Adafruit_DotStar.h>   //https://github.com/adafruit/Adafruit_DotStar
#include <SPI.h>

#define PIXELS_PER_PANEL 64
#define PANEL_COUNT 6
const int NUMPIXELS = PIXELS_PER_PANEL * PANEL_COUNT;

// Constructor for hardware SPI -- must connect to MOSI, SCK pins
Adafruit_DotStar strip = Adafruit_DotStar(NUMPIXELS, DOTSTAR_BRG);

void setup() {
  //power indicator
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);

  strip.begin(); // Initialize pins for output
  strip.show();  // Turn all LEDs off ASAP
}

// Runs 10 LEDs at a time along strip, cycling through red, green and blue.
// This requires about 40 mA for all the 'on' pixels + 1 mA per 'off' pixel.
// in a cube of 384 LEDs, this is about 16 amps. Doble check your batteries
// C-Rating, kids!

int      head  = 0, tail = -10; // Index of first 'on' and 'off' pixels
uint32_t color = 0xFF0000;      // 'On' color (starts red)

void loop() {

  strip.setPixelColor(head, color); // 'On' pixel at head
  strip.setPixelColor(tail, 0);     // 'Off' pixel at tail
  strip.show();                     // Refresh strip
  delay(20);                        // Pause 20 milliseconds (~50 FPS)

  if(++head >= NUMPIXELS) {         // Increment head index.  Off end of strip?
    head = 0;                       //  Yes, reset head index to start
    if((color >>= 8) == 0)          //  Next color (R->G->B) ... past blue now?
      color = 0xFF0000;             //   Yes, reset to red
  }
  if(++tail >= NUMPIXELS) tail = 0; // Increment, reset tail index
}
