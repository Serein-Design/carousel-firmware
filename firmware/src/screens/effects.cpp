// SPDX-License-Identifier: AGPL-3.0-only
// Classic demo effects: bouncing ball, Matrix rain, scrolling text, rotating
// rays and spectrum bars. The ambiances are screens/ambiances.cpp.
#include "display.h"
#include "settings.h"
#include "screens/screens.h"

// --- 1. Bouncing ball ------------------------------------------------------
void screenBall(uint32_t f, bool init) {
  static float x, y, dx, dy;
  static uint8_t hue;
  const int r = 3;

  if (init) { x = W() / 2.0f; y = H() / 2.0f; dx = 0.85f; dy = 0.55f; hue = 0; }

  display->clearScreen();
  display->drawRect(0, 0, W(), H(), display->color565(18, 18, 18));
  display->fillCircle((int)x, (int)y, r, colorWheel(hue++));

  x += dx; y += dy;
  if (x <= r)         { x = r;         dx = -dx; }
  if (x >= W() - 1 - r) { x = W() - 1 - r; dx = -dx; }
  if (y <= r)         { y = r;         dy = -dy; }
  if (y >= H() - 1 - r) { y = H() - 1 - r; dy = -dy; }
}

// --- 2. Matrix-style rain --------------------------------------------------
void screenMatrix(uint32_t f, bool init) {
  static float pos[MAX_DIM];
  static float speed[MAX_DIM];
  const int TRAIL = 9;

  if (init) {
    for (int i = 0; i < MAX_DIM; i++) {
      pos[i]   = -(float)random(0, H() * 2);
      speed[i] = random(30, 110) / 100.0f;
    }
  }

  display->clearScreen();
  for (int x = 0; x < W(); x++) {
    for (int t = 0; t < TRAIL; t++) {
      int y = (int)pos[x] - t;
      if (y < 0 || y >= H()) continue;
      if (t == 0) display->drawPixel(x, y, WHITE);
      else {
        uint8_t v = 220 - t * 24;
        display->drawPixel(x, y, display->color565(0, v, v / 5));
      }
    }
    pos[x] += speed[x];
    if (pos[x] - TRAIL > H()) {
      pos[x]   = -(float)random(0, H());
      speed[x] = random(30, 110) / 100.0f;
    }
  }
}

// Plasma, fire, stars and life: screens/ambiances.cpp. Scrolling text: screens/text.cpp

// --- 7. Rotating rays ------------------------------------------------------
void screenRays(uint32_t f, bool init) {
  int cx = W() / 2, cy = H() / 2;
  int R  = (W() > H() ? W() : H());

  display->clearScreen();
  for (int i = 0; i < 8; i++) {
    float a = f * 0.035f + i * (PI / 4.0f);
    display->drawLine(cx, cy,
                      cx + (int)(cosf(a) * R),
                      cy + (int)(sinf(a) * R),
                      colorWheel((uint8_t)(i * 32 + f)));
  }
  display->fillCircle(cx, cy, 2, WHITE);
}

// --- 8. Spectrum-style bars ------------------------------------------------
void screenBars(uint32_t f, bool init) {
  static int height[MAX_DIM], target[MAX_DIM];
  const int BAR_W = 4;
  int count = W() / BAR_W;

  if (init) {
    for (int i = 0; i < count; i++) { height[i] = 1; target[i] = random(2, H()); }
  }

  display->clearScreen();
  for (int i = 0; i < count; i++) {
    if ((f + i * 3) % 7 == 0) target[i] = random(2, H());
    if      (height[i] < target[i]) height[i]++;
    else if (height[i] > target[i]) height[i]--;

    for (int y = 0; y < height[i]; y++) {
      // green at the bottom, red at the top
      uint8_t frac = (uint8_t)(255UL * y / (H() - 1));
      display->drawFastHLine(i * BAR_W, H() - 1 - y, BAR_W - 1,
                             display->color565(frac, 255 - frac, 30));
    }
  }
}
