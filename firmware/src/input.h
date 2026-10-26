// SPDX-License-Identifier: AGPL-3.0-only
// Rotary encoder, encoder push button and tilt switch.
#pragma once

#include <Arduino.h>

// Debounced contact (button, tilt switch). Closed contact = LOW.
struct Contact {
  uint8_t  pin;
  uint16_t debounceMs;
  bool     stable;
  bool     raw;
  uint32_t lastChange;

  void begin(uint8_t p, uint16_t debounce, bool internalPullup);
  void update();
  bool closed() const { return stable == LOW; }
};

extern Contact encoderButton;
extern Contact tilt;

void inputBegin();

// Full detents since the last call. The remainder is kept, so no drift and
// no jump around zero.
int32_t readEncoderDetents();
