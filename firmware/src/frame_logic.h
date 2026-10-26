// SPDX-License-Identifier: AGPL-3.0-only
// The frame push, hardware-free: screens draw a whole frame off screen, and
// only the pixels that differ from what the panel shows are sent to it.
//
// Why: the panel's DMA buffer is on show while it is written, and the library
// writes a pixel in several steps (its bit planes cleared, then set). Screens
// that drew straight into it showed those in-between states on every rewrite,
// and a black pixel between each clearScreen() and the redraw: visible
// flicker on every screen, at any brightness and clock.
#pragma once

#include <stdint.h>

// Writes one pixel to the unrotated panel: x across its columns, y down its rows.
typedef void (*PixelWrite)(int16_t x, int16_t y, uint16_t color);

// frame and shown are width x height RGB565, row after row, in the panel's own
// unrotated layout (the off-screen canvas applies the rotation). Every pixel
// of frame that differs from shown goes to write, in row order, and into
// shown. Returns how many were written: 0 for an unchanged frame.
uint16_t framePush(const uint16_t *frame, uint16_t *shown, int16_t width, int16_t height,
                   PixelWrite write);

// The buffer index of on-screen pixel (x, y) of a width x height frame, as its
// rotation (0-3) puts it (Adafruit GFX's GFXcanvas16::drawPixel)
static inline int frameIndex(uint8_t rotation, int16_t width, int16_t height, int x, int y) {
  switch (rotation & 3) {
    case 1:  return (width - 1 - y) + x * width;
    case 2:  return (width - 1 - x) + (height - 1 - y) * width;
    case 3:  return y + (height - 1 - x) * width;
    default: return x + y * width;
  }
}
