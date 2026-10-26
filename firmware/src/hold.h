// SPDX-License-Identifier: AGPL-3.0-only
// The hold line: while a press is held toward something it will do on
// release, a line fills the bottom row (the status strip, row 31; the bottom
// row in portrait) in the accent, full once the release would do it. The
// knob's long press uses it; a later hold (a mode change) passes its own
// times. gallery.cpp draws it last, over the screen and its overlays.
// Hardware-free: host-tested in test_hold.
#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

#include "reach_logic.h"

// Under this, a press shows nothing: a short press stays a plain tap
static const uint32_t HOLD_SHOW_MS = 200;

// 0 (nothing to draw) up to showMs, then linear to 1 at fullMs, and 1 past it
float holdFill(uint32_t heldMs, uint32_t fullMs = LONG_PRESS_MS, uint32_t showMs = HOLD_SHOW_MS);

// The bottom row: fill of its width in the accent from the left, the rest
// dark; nothing at all at 0. The canvas's size gives the orientation.
void drawHoldLine(Adafruit_GFX &g, float fill);
