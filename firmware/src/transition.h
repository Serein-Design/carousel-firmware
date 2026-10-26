// SPDX-License-Identifier: AGPL-3.0-only
// The passage from one screen to the next, hardware-free: the outgoing frame
// is kept, the incoming screen draws into the frame each tick as usual, and
// the two are combined before the overlays and the push. Push (the knob's
// direction), dissolve (Bayer 4x4 ordered dither), wipe, or a cut.
//
// Why a second copy of the incoming frame: screens may draw on top of their
// last frame instead of clearing it. Each tick hands them back their own
// frame, never the mix the panel showed.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "feedback_style.h"

// How long a kind runs, in ms; 0 for a cut and any unknown value
uint16_t transitionMs(uint8_t kind);

// The knob turned by detents (not 0): +1, the next screen pushes the last
// one left; -1, the previous one pushes it right
int8_t transitionDirection(int32_t detents);

// Writes into out the passage from `from` to `to` at progress p (0..1).
// All three are width x height RGB565 in the frame's unrotated layout;
// rotation (0-3, as the frame's) says which way is left on screen. line
// colours the wipe's edge.
void transitionCombine(uint8_t kind, const uint16_t *from, const uint16_t *to, uint16_t *out,
                       int16_t width, int16_t height, uint8_t rotation, float p, int8_t dir,
                       uint16_t line);

typedef void *(*TransitionAlloc)(size_t);
typedef void (*TransitionFree)(void *);

// One passage at a time, its two frame copies (2 x 4 KB on the panel)
// allocated only while it runs.
class Passage {
 public:
  explicit Passage(TransitionAlloc alloc = malloc, TransitionFree release = free)
      : alloc_(alloc), release_(release) {}
  ~Passage() { cancel(); }

  // The screen changes now: frame is what the panel shows. False means a
  // cut: the kind is one, or the copies could not be allocated. A passage
  // already running restarts from the frame on show.
  bool begin(uint8_t kind, int8_t dir, const uint16_t *frame, int16_t width, int16_t height,
             uint8_t rotation, uint32_t now);
  bool running() const { return from_ != nullptr; }
  // Before the screen draws: gives it back its own last frame
  void beforeDraw(uint16_t *frame) const;
  // After the screen drew: combines it with the outgoing frame, or ends the
  // passage once its time is up or the frame turned (a tilt cuts)
  void afterDraw(uint16_t *frame, uint8_t rotation, uint32_t now, uint16_t line);
  void cancel();

 private:
  TransitionAlloc alloc_;
  TransitionFree  release_;
  uint16_t *from_ = nullptr, *to_ = nullptr;   // one allocation, from_ first
  bool      drawn_ = false;                    // to_ holds the incoming frame
  uint8_t   kind_ = TRANSITION_CUT, rotation_ = 0;
  int8_t    dir_ = 1;
  int16_t   width_ = 0, height_ = 0;
  uint32_t  start_ = 0;
};
