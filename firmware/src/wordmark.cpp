// SPDX-License-Identifier: AGPL-3.0-only
#include "wordmark.h"

#include "boot_logic.h"
#include "palette.h"

uint8_t wordmarkLevel(const Wordmark &m, int16_t x, int16_t y) {
  const uint8_t b = m.levels[y * ((m.w + 1) / 2) + x / 2];
  return x % 2 ? b & 0x0F : b >> 4;
}

const Wordmark &bootWordFor(const Adafruit_GFX &g) {
  return g.width() >= WORDMARK_WIDE.w ? WORDMARK_WIDE : WORDMARK_TALL;
}

void drawBootWord(Adafruit_GFX &g, int16_t x, int16_t y, uint8_t light) {
  if (!light) return;
  const Wordmark &m = bootWordFor(g);
  uint16_t shade[WORDMARK_LEVELS];   // once a frame, not once a pixel
  for (uint8_t v = 0; v < WORDMARK_LEVELS; v++)
    shade[v] = bootFaded(palette().text, v * light / (WORDMARK_LEVELS - 1));
  const int16_t left = x - m.w / 2;
  for (int16_t row = 0; row < m.h; row++)
    for (int16_t col = 0; col < m.w; col++) {
      const uint8_t v = wordmarkLevel(m, col, row);
      if (v) g.drawPixel(left + col, y + row, shade[v]);
    }
}
