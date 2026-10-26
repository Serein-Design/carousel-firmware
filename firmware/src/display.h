// SPDX-License-Identifier: AGPL-3.0-only
// HUB75 panel: initialisation, the off-screen frame and its push, shared
// colours and drawing helpers, and the panel timing settings tunable from the
// serial console.
#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "board.h"

// The frame every screen and overlay draws into, off screen: displayPush()
// then sends the panel only the pixels that changed (frame_logic.h says why).
// It answers what screens used to call on the panel itself, and carries the
// rotation, so the panel stays unrotated and the frame maps onto it 1:1.
class Frame : public GFXcanvas16 {
 public:
  Frame() : GFXcanvas16(PANEL_RES_X, PANEL_RES_Y, false) {}
  bool allocate();   // false when its 4 KB do not fit
  static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return MatrixPanel_I2S_DMA::color565(r, g, b);
  }
  void clearScreen() { fillScreen(0); }
  // GFXcanvas16 writes these without checking for a buffer. Without one (the
  // panel did not start), every draw stays a no-op, as the panel's did.
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
};

extern Frame *display;
extern uint16_t BLACK, WHITE;

// Current logical dimensions: they swap with the rotation.
static inline int W() { return display->width();  }
static inline int H() { return display->height(); }

extern uint8_t sinTab[256];   // 0..255, one full period

uint16_t colorWheel(uint8_t pos);

// The first call of setup(): OE off and zeros latched by hand, so the panel
// shows nothing until the DMA drives it (panel_blank_logic.h says why).
void displayHoldDark();

// Creates and starts the panel, then the frame and the copy of what the panel
// shows. False if any of their buffers could not be allocated.
bool displayBegin();

// Sends the panel the pixels of the frame that changed since the last push:
// once per frame, after everything is drawn.
void displayPush();

// What the panel shows, RGB565, unrotated, PANEL_RES_X x PANEL_RES_Y, row
// after row; nullptr before displayBegin(). loop() writes it on the other
// core: a reader may join two consecutive frames, never a wrong screen.
const uint16_t *displayShown();
uint8_t displayRotation();   // the frame's rotation, 0-3 (portrait: 1 or 3)

// 0..255, the panel's own brightness (the frame is unchanged)
void displayBrightness(uint8_t level);

// Serial console: one key changes a timing setting and reboots the board.
void printPanelTuning();
void serviceSerialConsole();

// "#ff8800" or "ff8800" -> RGB565; white when empty or unreadable
uint16_t parseColor(const char *hex);
