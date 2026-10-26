// SPDX-License-Identifier: AGPL-3.0-only
#include "preview_logic.h"

#include <string.h>

ReadVerdict readGate(bool hostOk, bool crossSite, bool cookieOk, bool locked, bool available) {
  if (!hostOk || crossSite) return READ_FORBIDDEN;
  if (!cookieOk) return READ_UNAUTHORIZED;
  if (locked || !available) return READ_UNAVAILABLE;
  return READ_OK;
}

static void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

// The unrotated buffer's index of the pixel at (x, y) of the rotated frame
static int canvasIndex(uint8_t rotation, int x, int y) {
  int bx = x, by = y;
  switch (rotation & 3) {
    case 1: bx = PREVIEW_W - 1 - y; by = x; break;
    case 2: bx = PREVIEW_W - 1 - x; by = PREVIEW_H - 1 - y; break;
    case 3: bx = y; by = PREVIEW_H - 1 - x; break;
  }
  return bx + by * PREVIEW_W;
}

void previewBmp(const uint16_t *shown, uint8_t rotation, bool black, uint8_t *out) {
  const bool upright = rotation & 1;
  const int w = upright ? PREVIEW_H : PREVIEW_W, h = upright ? PREVIEW_W : PREVIEW_H;
  const uint32_t pixels = PREVIEW_W * PREVIEW_H * 3;
  memset(out, 0, 54);
  // BITMAPFILEHEADER, then BITMAPINFOHEADER: 24 bits, no compression. Rows
  // of 192 or 96 bytes need no padding.
  out[0] = 'B';
  out[1] = 'M';
  put32(out + 2, PREVIEW_BMP_SIZE);
  put32(out + 10, 54);
  put32(out + 14, 40);
  put32(out + 18, (uint32_t)w);
  put32(out + 22, (uint32_t)h);   // positive: the last row of the picture comes first
  put16(out + 26, 1);
  put16(out + 28, 24);
  put32(out + 34, pixels);
  put32(out + 38, 2835);          // 72 dpi, as most tools write
  put32(out + 42, 2835);
  uint8_t *p = out + 54;
  if (black || !shown) {
    memset(p, 0, pixels);
    return;
  }
  for (int y = h - 1; y >= 0; y--)
    for (int x = 0; x < w; x++) {
      const uint16_t c = shown[canvasIndex(rotation, x, y)];
      const uint8_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
      *p++ = (uint8_t)((b << 3) | (b >> 2));
      *p++ = (uint8_t)((g << 2) | (g >> 4));
      *p++ = (uint8_t)((r << 3) | (r >> 2));
    }
}
