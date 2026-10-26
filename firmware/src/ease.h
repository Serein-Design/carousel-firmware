// SPDX-License-Identifier: AGPL-3.0-only
// The curve every motion on the panel shares (the board's LED.ease): the
// overlays' rise and slide, the passage between screens, the clock's cycle;
// and a symmetric one for the few motions that must start and land softly.
#pragma once

#include <math.h>

// Exponential ease-out, 1 - 2^(-10x), clamped to 0..1
static inline float easeOut(float x) {
  return x >= 1.0f ? 1.0f : x <= 0.0f ? 0.0f : 1.0f - exp2f(-10.0f * x);
}

// Smoothstep, 3x^2 - 2x^3, clamped to 0..1: slow at both ends (the boot's opening)
static inline float easeInOut(float x) {
  return x >= 1.0f ? 1.0f : x <= 0.0f ? 0.0f : x * x * (3.0f - 2.0f * x);
}
