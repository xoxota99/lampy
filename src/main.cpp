// Sketch to accompany "Sipping Power With NeoPixels" guide.  Designed for
// Adafruit Circuit Playground but could be adapted to other projects.

#include <SPI.h>
#include <Adafruit_DotStarMatrix.h>

// GLOBAL VARIABLES --------------------------------------------------------

// This bizarre construct isn't Arduino code in the conventional sense.
// It exploits features of GCC's preprocessor to generate a PROGMEM
// table (in flash memory) holding an 8-bit unsigned sine wave (0-255).
const int _SBASE_ = __COUNTER__ + 1; // Index of 1st __COUNTER__ ref below
#define _S1_ (sin((__COUNTER__ - _SBASE_) / 128.0 * M_PI) + 1.0) * 127.5 + 0.5,
#define _S2_ _S1_ _S1_ _S1_ _S1_ _S1_ _S1_ _S1_ _S1_ // Expands to 8 items
#define _S3_ _S2_ _S2_ _S2_ _S2_ _S2_ _S2_ _S2_ _S2_ // Expands to 64 items
const uint8_t PROGMEM sineTable[] = { _S3_ _S3_ _S3_ _S3_ }; // 256 items

// Similar to above, but for an 8-bit gamma-correction table.
#define _GAMMA_ 2.6
const int _GBASE_ = __COUNTER__ + 1; // Index of 1st __COUNTER__ ref below
#define _G1_ pow((__COUNTER__ - _GBASE_) / 255.0, _GAMMA_) * 255.0 + 0.5,
#define _G2_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ // Expands to 8 items
#define _G3_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ // Expands to 64 items
const uint8_t PROGMEM gammaTable[] = { _G3_ _G3_ _G3_ _G3_ }; // 256 items

// These are used in estimating (very approximately) the current draw of
// the board and NeoPixels.  BASECURRENT is the MINIMUM current (in mA)
// used by the entire system (microcontroller board plus NeoPixels) --
// keep in mind that even when "off," NeoPixels use a tiny amount of
// current (a bit under 1 milliamp each).  LEDCURRENT is the maximum
// additional current PER PRIMARY COLOR of ONE NeoPixel -- total current
// for an RGB NeoPixel could be up to 3X this.  The '3535' NeoPixels on
// Circuit Playground are smaller and use less current than the more
// common '5050' type used in NeoPixel strips and shapes.
#define BASECURRENT 10
#define LEDCURRENT  13.3 // Try using 18 for '5050' NeoPixels
#define BRIGHTNESS  16
uint8_t  frame = 0;    // Frame count, results displayed every 256 frames
long int sum   = 0;    // Cumulative current, for calculating average
uint8_t *pixelPtr;     // -> NeoPixel color data

void mode_off();
void mode_white_max();
void mode_white_half_duty();
void mode_white_half_perceptual();
void mode_primaries();
void mode_colorwheel();
void mode_colorwheel_gamma();
void mode_half();
void mode_sparkle();
void mode_marquee();
void mode_sine();
void mode_sine_gamma();
void mode_sine_half_gamma();

// This array lists each of the display/animation drawing functions
// (which appear later in this code) in the order they're selected with
// the right button.  Some functions appear repeatedly...for example,
// we return to "mode_off" at several points in the sequence.
void (*renderFunc[])(void) {
  mode_off, // Starts here, with LEDs off
  mode_white_max, mode_white_half_duty , mode_white_half_perceptual,
  mode_primaries, mode_colorwheel, mode_colorwheel_gamma,
  mode_half, mode_sparkle,
  mode_marquee, mode_sine, mode_sine_gamma, mode_sine_half_gamma
};
const int N_MODES = (sizeof(renderFunc) / sizeof(renderFunc[0]));
uint8_t mode = 0; // Index of current mode in table
long int lastChangeTime;
#define MODE_CHANGE_TIME_MS 2500    //milliseconds between mode changes.

//Hardware SPI constructor, for tiled matrices.
Adafruit_DotStarMatrix matrix = Adafruit_DotStarMatrix(
(uint8_t)8, (uint8_t)8, 6, 1,
DS_TILE_TOP   + DS_TILE_LEFT   + DS_TILE_ROWS      + DS_TILE_PROGRESSIVE +
DS_MATRIX_TOP + DS_MATRIX_LEFT + DS_MATRIX_COLUMNS + DS_MATRIX_PROGRESSIVE,
DOTSTAR_GRB);

// Adafruit_DotStarMatrix matrix = Adafruit_DotStarMatrix(
// 8, 8,
// DS_MATRIX_TOP     + DS_MATRIX_LEFT +
// DS_MATRIX_COLUMNS + DS_MATRIX_PROGRESSIVE,
// DOTSTAR_GRB);
// SETUP FUNCTION -- RUNS ONCE AT PROGRAM START ----------------------------

void setup() {
  matrix.begin();
  matrix.setBrightness(BRIGHTNESS); // NeoPixels at full brightness
  pixelPtr = matrix.getPixels();

  Serial.begin(19200);

// mode_off();
// matrix.show();
// delay(1000);

  // mode=6;   //colorwheel_gamma

}

// LOOP FUNCTION -- RUNS OVER AND OVER FOREVER -----------------------------

void loop() {

  uint32_t t = millis();

  //for now, just change modes every ten seconds or something.
  if((t-lastChangeTime) >= MODE_CHANGE_TIME_MS){
    mode = (mode+1) % N_MODES;
    lastChangeTime=t;
  }

  (*renderFunc[mode])();          // Render one frame in current mode
  matrix.show();                  // and update the NeoPixels to show it

  // Accumulate total brightness value for all NeoPixels (assumes RGB).
  for(int i=0; i<matrix.numPixels() * 3; i++) {
    sum += pixelPtr[i];
  }
  if(!++frame) { // Every 256th frame, estimate & print current
    long int mA=BASECURRENT + (sum * (LEDCURRENT * BRIGHTNESS / 256) + 32640) / 65280;
    Serial.print(mA);
    Serial.println(" mA");
    sum = 0; // Reset pixel accumulator
  }
}

// RENDERING FUNCTIONS FOR EACH DISPLAY/ANIMATION MODE ---------------------

// All NeoPixels off
void mode_off() {
  matrix.clear();
}

// All NeoPixels on at max: white (R+G+B) at 100% duty cycle
void mode_white_max() {
  for(int i=0; i<matrix.numPixels(); i++) {
    matrix.setPixelColor(i, 0xFFFFFF);
  }
}

// All NeoPixels on at 50% duty cycle white.  Numerically speaking,
// this is half power, but perceptually it appears brighter than 50%.
void mode_white_half_duty() {
  for(int i=0; i<matrix.numPixels(); i++) {
    matrix.setPixelColor(i, 0x7F7F7F);
  }
}

// All NeoPixels on at 50% perceptial brightness, using gamma table lookup.
// Though it visually appears to be about half brightness, numerically the
// duty cycle is much less, a bit under 20% -- meaning "half brightness"
// can actually be using 1/5 the power!
void mode_white_half_perceptual() {
  uint32_t c = pgm_read_byte(&gammaTable[127]) * 0x010101;
  for(int i=0; i<matrix.numPixels(); i++) {
    matrix.setPixelColor(i, c);
  }
}

// Cycle through primary colors (red, green, blue), full brightness.
// Because only a single primary color within each NeoPixel is turned on
// at any given time, this uses 1/3 the power of the "white max" mode.
void mode_primaries() {
  uint32_t c;
  for(int i=0; i<matrix.numPixels(); i++) {
    // This animation (and many of the rest) pretend spatially that there's
    // 12 equally-spaced NeoPixels, though in reality there's only 10 with
    // gaps at the USB and battery connectors.
    uint8_t j = i + (i > 4);     // Mind the gap
    j = ((millis() / 100) + j) % 12;
    if(j < 4)      c = 0xFF0000; // Bed
    else if(j < 8) c = 0x00FF00; // Green
    else           c = 0x0000FF; // Blue
    matrix.setPixelColor(i, c);
  }
}

// HSV (hue-saturation-value) to RGB function used for the next two modes.
uint32_t hsv2rgb(int32_t h, uint8_t s, uint8_t v, boolean gc=false) {
  uint8_t n, r, g, b;

  // Hue circle = 0 to 1530 (NOT 1536!)
  h %= 1530;           // -1529 to +1529
  if(h < 0) h += 1530; //     0 to +1529
  n  = h % 255;        // Angle within sextant; 0 to 254 (NOT 255!)
  switch(h / 255) {    // Sextant number; 0 to 5
   case 0 : r = 255    ; g =   n    ; b =   0    ; break; // R to Y
   case 1 : r = 254 - n; g = 255    ; b =   0    ; break; // Y to G
   case 2 : r =   0    ; g = 255    ; b =   n    ; break; // G to C
   case 3 : r =   0    ; g = 254 - n; b = 255    ; break; // C to B
   case 4 : r =   n    ; g =   0    ; b = 255    ; break; // B to M
   default: r = 255    ; g =   0    ; b = 254 - n; break; // M to R
  }

  uint32_t v1 =   1 + v; // 1 to 256; allows >>8 instead of /255
  uint16_t s1 =   1 + s; // 1 to 256; same reason
  uint8_t  s2 = 255 - s; // 255 to 0

  r = ((((r * s1) >> 8) + s2) * v1) >> 8;
  g = ((((g * s1) >> 8) + s2) * v1) >> 8;
  b = ((((b * s1) >> 8) + s2) * v1) >> 8;
  if(gc) { // Gamma correct?
    r = pgm_read_byte(&gammaTable[r]);
    g = pgm_read_byte(&gammaTable[g]);
    b = pgm_read_byte(&gammaTable[b]);
  }
  return ((uint32_t)r << 16) | ((uint16_t)g << 8) | b;
}

// Rotating color wheel, using 'raw' RGB values (no gamma correction).
// Average current use is about 1/2 of the max-all-white case.
void mode_colorwheel() {
  uint32_t t = millis();
  for(int i=0; i<matrix.numPixels(); i++) {
    uint8_t j = i + (i > 4);
    matrix.setPixelColor(i,
      hsv2rgb(t + j * 1530 / 12, 255, 255, false));
  }
}

// Color wheel using gamma-corrected values.  Current use is slightly less
// than the 'raw' case, but not tremendously so, as only 1/3 of pixels at
// any time are in transition cases (else 100% on or off).
void mode_colorwheel_gamma() {
  uint32_t t = millis();
  for(int i=0; i<matrix.numPixels(); i++) {
    uint8_t j = i + (i > 4);
    matrix.setPixelColor(i,
      hsv2rgb(t + j * 1530 / 12, 255, 255, true));
  }
}

// Cycle with half the pixels on, half off at any given time.
// Simple idea.  Half the pixels means half the power use.
void mode_half() {
  uint32_t t = millis() / 4;
  for(int i=0; i<matrix.numPixels(); i++) {
    uint8_t j = t + i * 256 / matrix.numPixels();
    j = (j >> 7) * 255;
    matrix.setPixelColor(i, j * 0x010000);
  }
}

// Random sparkles.  Randomly turns on ONE pixel at a time.  This demonstrates
// minimal power use while still doing something "catchy."  And because it's
// a primary color, power use is even minimal-er (see 'primaries' above).
void mode_sparkle() {
  static int randomPixel = 0;
  if(!(frame & 0x02)) {              // Every 2 frames...
    matrix.clear(); // Clear pixels
    int r;
    do {
      r = random(matrix.numPixels());                // Pick a new random pixel
    } while(r == randomPixel);       // but not the same as last time
    uint32_t c = random(0xFFFFFF);
    randomPixel = r;                 // Save new random pixel index
    matrix.setPixelColor(randomPixel, c);
  }
}

// Simple on-or-off "marquee" animation w/ about 50% of pixels lit at once.
// Not much different than the 'half' animation, but provides a conceptual
// transition into the examples that follow.
void mode_marquee() {
  uint32_t t = millis() / 4;
  for(int i=0; i<matrix.numPixels(); i++) {
    uint8_t j = i + (i > 4);
    uint8_t k = (t + j * 256 / 12) & 0xFF;
    k = ((k >> 6) & 1) * 255;
    matrix.setPixelColor(i, k * 0x000100L);
  }
}

// Sine wave marquee, no gamma correction.  Avg. overall duty cycle is 50%,
// and combined with being a primary color, uses about 1/6 the max current.
void mode_sine() {
  uint32_t t = millis() / 4;
  for(int i=0; i<matrix.numPixels(); i++) {
    uint8_t j = i + (i > 4);
    uint8_t k = pgm_read_byte(&sineTable[(t + j * 512 / 12) & 0xFF]);
    matrix.setPixelColor(i, k * 0x000100L);
  }
}

// Sine wave with gamma correction.  Because nearly all the pixels have
// "in-between" values (not 100% on or off), there's considerable power
// savings to gamma correction, in addition to looking more "correct."
void mode_sine_gamma() {
  uint32_t t = millis() / 4;
  for(int i=0; i<matrix.numPixels(); i++) {
    uint8_t j = i + (i > 4);
    uint8_t k = pgm_read_byte(&sineTable[(t + j * 512 / 12) & 0xFF]);
    k = pgm_read_byte(&gammaTable[k]);
    matrix.setPixelColor(i, k * 0x000100L);
  }
}

// Perceptually half-brightness gamma-corrected sine wave.  Sometimes you
// don't need things going to peak brightness all the time.  Combined with
// gamma and primary color use, it's super effective!
void mode_sine_half_gamma() {
  uint32_t t = millis() / 4;
  for(int i=0; i<matrix.numPixels(); i++) {
    uint8_t j = i + (i > 4);
    uint8_t k = pgm_read_byte(&sineTable[(t + j * 512 / 12) & 0xFF]) / 2;
    k = pgm_read_byte(&gammaTable[k]);
    matrix.setPixelColor(i, k * 0x000100L);
  }
}
