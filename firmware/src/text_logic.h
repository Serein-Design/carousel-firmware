// SPDX-License-Identifier: AGPL-3.0-only
// The TEXT screen's scroll: the message enters at the right edge, crosses at a
// calm 24 px/s (as the request band) and leaves on the left, then comes back.
// Also the face choice of IMAGE's portrait states. Hardware-free: host-tested
// in test_text_screen.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "grid.h"
#include "type.h"

static const uint32_t TEXT_PX_PER_S = 24;
// Cap top of the line: seven rows centred in the main rows (accents above
// and descenders below stay inside them)
static const int16_t  TEXT_Y        = GRID_MAIN_TOP + 3;

// Pixels in one pass: the text's width and the canvas it crosses
inline uint32_t textPassPx(uint16_t textW, int16_t canvasWidth) {
  return (uint32_t)textW + (uint32_t)canvasWidth;
}

// Rounded up: the pass is over when the text has left
inline uint32_t textPassMs(uint16_t textW, int16_t canvasWidth) {
  return (textPassPx(textW, canvasWidth) * 1000 + TEXT_PX_PER_S - 1) / TEXT_PX_PER_S;
}

// The text's left column ms after the screen came up. Nothing to show: the
// right edge, for good.
inline int16_t textScrollX(uint16_t textW, int16_t canvasWidth, uint32_t ms) {
  if (!textW) return canvasWidth;
  const uint64_t pass = textPassPx(textW, canvasWidth);
  const uint64_t moved = ((uint64_t)ms * TEXT_PX_PER_S / 1000) % pass;
  return (int16_t)(canvasWidth - (int64_t)moved);
}

// What the TEXT line draws: the message as it is, and a '?' for each character
// the body face still lacks. toMessageText keeps ASCII and the composed French
// letters only, but a message stored by an older build may hold others, which
// would show as a box in the alert colour. A UTF-8 sequence is swapped or kept
// whole, never cut.
inline void messageDrawable(const char *in, char *out, size_t size) {
  size_t n = 0;
  while (*in && n + 1 < size) {
    const uint8_t c = (uint8_t)*in;
    if (c < 0x80) { out[n++] = *in++; continue; }
    const int want = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC2 ? 2 : 1;
    int len = 1;
    while (len < want && ((uint8_t)in[len] & 0xC0) == 0x80) len++;
    uint32_t cp = 0;
    if (len == want && want == 2) cp = (c & 0x1Fu) << 6 | ((uint8_t)in[1] & 0x3Fu);
    if (len == want && want == 3) cp = (c & 0x0Fu) << 12 | ((uint8_t)in[1] & 0x3Fu) << 6 | ((uint8_t)in[2] & 0x3Fu);
    if (want == 3 && cp < 0x800) cp = 0;         // overlong (E0 then < A0): not a letter
    if (cp && hasGlyph(cp, Face::BODY)) {
      if (n + len >= size) break;                // whole or not at all
      for (int i = 0; i < len; i++) out[n++] = in[i];
    } else {
      out[n++] = '?';
    }
    in += len;
  }
  if (size) out[n] = '\0';
}

// IMAGE's portrait states stack two words in the body face, then two in the
// label face; when a body word is too wide for the line (STORE) both go in
// the label face, so the pair stays alike.
inline Face stateFace(const char *a, const char *b, uint16_t lineWidth) {
  return textWidth(a, Face::BODY) <= lineWidth && textWidth(b, Face::BODY) <= lineWidth
             ? Face::BODY : Face::LABEL;
}
