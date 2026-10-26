// SPDX-License-Identifier: AGPL-3.0-only
#include "transition.h"

#include <math.h>
#include <string.h>

#include "ease.h"
#include "frame_logic.h"

uint16_t transitionMs(uint8_t kind) {
  switch (kind) {
    case TRANSITION_PUSH:     return 240;
    case TRANSITION_DISSOLVE: return 400;
    case TRANSITION_WIPE:     return 240;
    default:                  return 0;
  }
}

int8_t transitionDirection(int32_t detents) { return detents < 0 ? -1 : 1; }

// Ordered dither: deterministic, no random numbers, cheap on the ESP32
static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

void transitionCombine(uint8_t kind, const uint16_t *from, const uint16_t *to, uint16_t *out,
                       int16_t width, int16_t height, uint8_t rotation, float p, int8_t dir,
                       uint16_t line) {
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  // The size on screen: a portrait frame swaps them
  const bool swap = rotation & 1;
  const int w = swap ? height : width, h = swap ? width : height;
  const float e = easeOut(p);
  const int d = (int)lroundf(e * w);                 // push: columns moved
  const int edge = (int)lroundf(e * (w + 1));        // wipe: first column not yet revealed
  const bool edgeShown = p > 0.0f && p < 1.0f;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      uint16_t c;
      switch (kind) {
        case TRANSITION_PUSH:
          if (dir >= 0) c = x + d < w ? from[frameIndex(rotation, width, height, x + d, y)]
                                      : to[frameIndex(rotation, width, height, x + d - w, y)];
          else          c = x >= d ? from[frameIndex(rotation, width, height, x - d, y)]
                                   : to[frameIndex(rotation, width, height, x - d + w, y)];
          break;
        case TRANSITION_DISSOLVE:
          c = BAYER[(y & 3) * 4 + (x & 3)] < p * 16.0f
                ? to[frameIndex(rotation, width, height, x, y)]
                : from[frameIndex(rotation, width, height, x, y)];
          break;
        case TRANSITION_WIPE:
          if (edgeShown && x == (edge < w - 1 ? edge : w - 1)) c = line;
          else c = x < edge ? to[frameIndex(rotation, width, height, x, y)]
                            : from[frameIndex(rotation, width, height, x, y)];
          break;
        default:   // a cut
          c = to[frameIndex(rotation, width, height, x, y)];
          break;
      }
      out[frameIndex(rotation, width, height, x, y)] = c;
    }
  }
}

bool Passage::begin(uint8_t kind, int8_t dir, const uint16_t *frame, int16_t width,
                    int16_t height, uint8_t rotation, uint32_t now) {
  if (transitionMs(kind) == 0) {
    cancel();
    return false;
  }
  const size_t pixels = (size_t)width * height;
  if (!running() || width != width_ || height != height_) {
    cancel();
    from_ = (uint16_t *)alloc_(2 * pixels * sizeof(uint16_t));
    if (!from_) return false;   // no room for the outgoing frame: a cut
    to_ = from_ + pixels;
  }
  memcpy(from_, frame, pixels * sizeof(uint16_t));
  drawn_    = false;
  kind_     = kind;
  dir_      = dir;
  width_    = width;
  height_   = height;
  rotation_ = rotation;
  start_    = now;
  return true;
}

void Passage::beforeDraw(uint16_t *frame) const {
  if (running() && drawn_) memcpy(frame, to_, (size_t)width_ * height_ * sizeof(uint16_t));
}

void Passage::afterDraw(uint16_t *frame, uint8_t rotation, uint32_t now, uint16_t line) {
  if (!running()) return;
  const uint32_t elapsed = now - start_;
  const uint16_t ms = transitionMs(kind_);
  // Time is up, or a tilt turned the frame: the incoming screen as drawn
  if (elapsed >= ms || rotation != rotation_) {
    cancel();
    return;
  }
  memcpy(to_, frame, (size_t)width_ * height_ * sizeof(uint16_t));
  drawn_ = true;
  transitionCombine(kind_, from_, to_, frame, width_, height_, rotation_, (float)elapsed / ms,
                    dir_, line);
}

void Passage::cancel() {
  if (from_) release_(from_);
  from_  = to_ = nullptr;
  drawn_ = false;
}
