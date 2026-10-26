// SPDX-License-Identifier: AGPL-3.0-only
#include "frame_logic.h"

uint16_t framePush(const uint16_t *frame, uint16_t *shown, int16_t width, int16_t height,
                   PixelWrite write) {
  uint16_t written = 0;
  for (int16_t y = 0; y < height; y++) {
    for (int16_t x = 0; x < width; x++) {
      const int i = y * width + x;
      if (frame[i] == shown[i]) continue;
      write(x, y, frame[i]);
      shown[i] = frame[i];
      written++;
    }
  }
  return written;
}
