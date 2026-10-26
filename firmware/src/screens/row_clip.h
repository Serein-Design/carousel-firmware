// SPDX-License-Identifier: AGPL-3.0-only
// Draws into the frame between two rows only: what pushes in and out of a
// slot (CLOCK's readouts, SOCIAL's counters) stays inside it.
#pragma once

#include <Adafruit_GFX.h>

#include "display.h"

class RowClip : public Adafruit_GFX {
 public:
  RowClip(int16_t top, int16_t bottom)
      : Adafruit_GFX(display->width(), display->height()), top_(top), bottom_(bottom) {}
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (y >= top_ && y <= bottom_) display->drawPixel(x, y, color);
  }

 private:
  int16_t top_, bottom_;
};
