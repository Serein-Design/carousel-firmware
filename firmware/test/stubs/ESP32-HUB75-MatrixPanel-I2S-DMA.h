// SPDX-License-Identifier: AGPL-3.0-only
// Stand-in for the HUB75 panel driver: its colour packing, which display.h's
// Frame answers for the screens. No panel on the PC.
#pragma once

#include <Adafruit_GFX.h>

class MatrixPanel_I2S_DMA {
 public:
  static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
};
