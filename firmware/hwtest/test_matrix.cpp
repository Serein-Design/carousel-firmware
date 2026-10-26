// SPDX-License-Identifier: AGPL-3.0-only
/*
 * HUB75 64x32 (P3) LED panel test on an ESP32 DevKit
 * Library: ESP32-HUB75-MatrixPanel-DMA (mrcodetastic)
 *   Library manager -> "ESP32 HUB75 LED MATRIX PANEL DMA Display"
 *   Dependency: Adafruit GFX Library
 *
 * Board to select: "ESP32 Dev Module"
 *
 * POWER WARNING:
 *   The panel must be powered from an external 5V supply (4A minimum).
 *   NEVER power it from the ESP32's 5V/VIN pin or from USB.
 *   The 5V supply ground and the ESP32 ground must be tied together.
 */

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// ---------------------------------------------------------------------------
// Pinout (same as the KiCad schematic, NeoCat mapping)
// ---------------------------------------------------------------------------
#define R1_PIN  25
#define G1_PIN  26
#define B1_PIN  27
#define R2_PIN  21
#define G2_PIN  22
#define B2_PIN  23

#define A_PIN   12
#define B_PIN   16
#define C_PIN   17
#define D_PIN   18
#define E_PIN   -1   // -1 for a 64x32 panel (1/16 scan). Set a GPIO for a 64x64.

#define CLK_PIN 15
#define LAT_PIN 32
#define OE_PIN  33

// ---------------------------------------------------------------------------
// Panel geometry
// ---------------------------------------------------------------------------
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

// Working brightness, 0-255. 255 = max (~4A on a full white screen).
#define BRIGHTNESS  255

MatrixPanel_I2S_DMA *display = nullptr;

// Colours precomputed in setup()
uint16_t BLACK, WHITE, RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Hue 0..255 -> RGB565, for the gradients and the rainbow
uint16_t colorWheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) {
    return display->color565(255 - pos * 3, 0, pos * 3);
  } else if (pos < 170) {
    pos -= 85;
    return display->color565(0, pos * 3, 255 - pos * 3);
  } else {
    pos -= 170;
    return display->color565(pos * 3, 255 - pos * 3, 0);
  }
}

void heading(const char *txt) {
  Serial.print("[TEST] ");
  Serial.println(txt);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// 1. Full screen per colour: checks that all 3 R/G/B channels work
//    and that both half-panels (rows 0-15 and 16-31) light up.
void testSolidColors() {
  heading("Solid colours");
  const uint16_t colors[] = {RED, GREEN, BLUE, WHITE};
  const char *names[] = {"RED", "GREEN", "BLUE", "WHITE"};

  for (int i = 0; i < 4; i++) {
    Serial.print("  -> ");
    Serial.println(names[i]);
    display->fillScreen(colors[i]);
    delay(1200);
  }
  display->clearScreen();
  delay(300);
}

// 2. Vertical bars: checks the colour order and that R/G/B are not
//    swapped (wiring reversed on the connector).
void testBars() {
  heading("Colour bars");
  const uint16_t bars[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, BLACK};
  int width = PANEL_RES_X / 8;

  for (int i = 0; i < 8; i++) {
    display->fillRect(i * width, 0, width, PANEL_RES_Y, bars[i]);
  }
  delay(2500);
  display->clearScreen();
  delay(300);
}

// 3. Grid + frame: checks the geometry. If rows are shifted or
//    doubled, it is a scan (A/B/C/D) or clkphase problem.
void testGeometry() {
  heading("Grid and frame");
  display->clearScreen();

  // Outer frame: must run exactly around the panel edge
  display->drawRect(0, 0, PANEL_RES_X, PANEL_RES_Y, WHITE);

  // Grid every 8 px
  for (int x = 8; x < PANEL_RES_X; x += 8) {
    display->drawLine(x, 0, x, PANEL_RES_Y - 1, display->color565(0, 40, 0));
  }
  for (int y = 8; y < PANEL_RES_Y; y += 8) {
    display->drawLine(0, y, PANEL_RES_X - 1, y, display->color565(0, 40, 0));
  }

  // Diagonals: must cross at the exact centre
  display->drawLine(0, 0, PANEL_RES_X - 1, PANEL_RES_Y - 1, RED);
  display->drawLine(PANEL_RES_X - 1, 0, 0, PANEL_RES_Y - 1, RED);

  // One pixel in each corner, each a different colour: shows the orientation
  display->drawPixel(0, 0, YELLOW);                              // top left
  display->drawPixel(PANEL_RES_X - 1, 0, CYAN);                  // top right
  display->drawPixel(0, PANEL_RES_Y - 1, MAGENTA);               // bottom left
  display->drawPixel(PANEL_RES_X - 1, PANEL_RES_Y - 1, GREEN);   // bottom right

  delay(3000);
  display->clearScreen();
  delay(300);
}

// 4. Horizontal gradient: checks the colour depth / the gamma.
void testGradient() {
  heading("Rainbow gradient");
  for (int x = 0; x < PANEL_RES_X; x++) {
    uint16_t c = colorWheel(map(x, 0, PANEL_RES_X - 1, 0, 255));
    display->drawLine(x, 0, x, PANEL_RES_Y - 1, c);
  }
  delay(2500);
  display->clearScreen();
  delay(300);
}

// 5. Text: checks how readable the panel really is.
void testText() {
  heading("Text");
  display->clearScreen();
  display->setTextWrap(false);
  display->setTextSize(1);

  display->setCursor(2, 2);
  display->setTextColor(WHITE);
  display->print("ESP32");

  display->setCursor(2, 12);
  display->setTextColor(GREEN);
  display->print("64x32 OK");

  display->setCursor(2, 22);
  display->setTextColor(CYAN);
  display->print("HUB75 P3");

  delay(3000);
  display->clearScreen();
  delay(300);
}

// 6. Scrolling text: checks that there is no flicker / tearing.
void testScrolling() {
  heading("Scrolling text");
  const char *msg = "CAROUSEL LED PANEL TEST 64x32 - ESP32 - ";
  int textWidth = strlen(msg) * 6;   // 5x7 font + 1 px spacing

  display->setTextSize(1);
  display->setTextWrap(false);
  display->setTextColor(YELLOW);

  for (int offset = PANEL_RES_X; offset > -textWidth; offset--) {
    display->clearScreen();
    display->setCursor(offset, 12);
    display->print(msg);
    delay(25);
  }
  display->clearScreen();
  delay(300);
}

// 7. Line-by-line sweep: spots a dead LED or a cut column.
//    Deliberately slow, so you can watch the panel meanwhile.
void testSweep() {
  heading("Line-by-line sweep");
  display->clearScreen();
  for (int y = 0; y < PANEL_RES_Y; y++) {
    display->drawLine(0, y, PANEL_RES_X - 1, y, WHITE);
    delay(40);
    display->drawLine(0, y, PANEL_RES_X - 1, y, BLACK);
  }
  for (int x = 0; x < PANEL_RES_X; x++) {
    display->drawLine(x, 0, x, PANEL_RES_Y - 1, WHITE);
    delay(20);
    display->drawLine(x, 0, x, PANEL_RES_Y - 1, BLACK);
  }
  delay(300);
}

// 8. Brightness ramp: checks OE driving and power supply stability.
//    If the ESP32 reboots at high brightness -> the 5V supply is too weak.
void testBrightness() {
  heading("Brightness ramp");
  display->fillScreen(WHITE);
  for (int b = 10; b <= 255; b += 5) {
    display->setBrightness8(b);
    delay(20);
  }
  for (int b = 255; b >= 10; b -= 5) {
    display->setBrightness8(b);
    delay(20);
  }
  display->setBrightness8(BRIGHTNESS);
  display->clearScreen();
  delay(300);
}

// 9. Animation: bouncing ball, checks the refresh rate.
void testAnimation(uint32_t durationMs) {
  heading("Animation (bouncing ball)");
  float x = 10, y = 10, dx = 0.9, dy = 0.6;
  const int radius = 3;
  uint8_t hue = 0;
  uint32_t start = millis();

  while (millis() - start < durationMs) {
    display->clearScreen();
    display->drawRect(0, 0, PANEL_RES_X, PANEL_RES_Y, display->color565(20, 20, 20));
    display->fillCircle((int)x, (int)y, radius, colorWheel(hue++));

    x += dx;
    y += dy;
    if (x <= radius || x >= PANEL_RES_X - 1 - radius) dx = -dx;
    if (y <= radius || y >= PANEL_RES_Y - 1 - radius) dy = -dy;

    delay(10);
  }
  display->clearScreen();
  delay(300);
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== HUB75 64x32 panel test ===");

  HUB75_I2S_CFG::i2s_pins pins = {
    R1_PIN, G1_PIN, B1_PIN,
    R2_PIN, G2_PIN, B2_PIN,
    A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
    LAT_PIN, OE_PIN, CLK_PIN
  };

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);

  // If the image is shifted by one pixel horizontally, set this to true.
  mxconfig.clkphase = false;

  // Uncomment to match the panel's driver chip (ICN2038S, FM6126A, MBI5124...).
  // An FM6126A panel stays black without this line.
  // mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  display = new MatrixPanel_I2S_DMA(mxconfig);

  if (!display->begin()) {
    Serial.println("ERROR: DMA allocation failed");
    while (true) delay(1000);
  }

  display->setBrightness8(BRIGHTNESS);
  display->clearScreen();

  BLACK   = display->color565(0, 0, 0);
  WHITE   = display->color565(255, 255, 255);
  RED     = display->color565(255, 0, 0);
  GREEN   = display->color565(0, 255, 0);
  BLUE    = display->color565(0, 0, 255);
  YELLOW  = display->color565(255, 255, 0);
  CYAN    = display->color565(0, 255, 255);
  MAGENTA = display->color565(255, 0, 255);

  Serial.println("Init OK, starting the tests.");
}

void loop() {
  testSolidColors();
  testBars();
  testGeometry();
  testGradient();
  testText();
  testScrolling();
  testSweep();
  testBrightness();
  testAnimation(8000);

  Serial.println("--- Cycle done, starting over ---\n");
}
