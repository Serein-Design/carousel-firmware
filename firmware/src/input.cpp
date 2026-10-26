// SPDX-License-Identifier: AGPL-3.0-only
#include "input.h"

#include "board.h"

// ---------------------------------------------------------------------------
// Rotary encoder - quadrature decoding on interrupt
// ---------------------------------------------------------------------------

// index = (previous AB << 2) | (current AB). Impossible transitions (both
// bits changing at once, the signature of a bounce) return 0.
static const int8_t QUAD_TABLE[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

static volatile int32_t encQuarterSteps = 0;   // 4 quarters = 1 detent on an EC11
static volatile uint8_t encState        = 0;

static void IRAM_ATTR encoderISR() {
  uint8_t ab = (digitalRead(ENC_CLK_PIN) << 1) | digitalRead(ENC_DT_PIN);
  encState = ((encState << 2) | ab) & 0x0F;
  encQuarterSteps += QUAD_TABLE[encState];
}

static int32_t encQuartersRead = 0;

int32_t readEncoderDetents() {
  int32_t q = encQuarterSteps;            // aligned 32-bit read = atomic
  int32_t delta = q - encQuartersRead;
  if (delta > -4 && delta < 4) return 0;
  int32_t detents = delta / 4;
  encQuartersRead += detents * 4;
  return detents;
}

// ---------------------------------------------------------------------------
// Debounced contact (button, tilt switch)
// ---------------------------------------------------------------------------
void Contact::begin(uint8_t p, uint16_t debounce, bool internalPullup) {
  pin        = p;
  debounceMs = debounce;
  pinMode(pin, internalPullup ? INPUT_PULLUP : INPUT);
  raw = stable = digitalRead(pin);
  lastChange   = millis();
}

void Contact::update() {
  bool v = digitalRead(pin);
  if (v != raw) {
    raw = v;
    lastChange = millis();
  }
  if (v != stable && (millis() - lastChange) >= debounceMs) stable = v;
}

Contact encoderButton;
Contact tilt;

void inputBegin() {
  pinMode(ENC_CLK_PIN, INPUT_PULLUP);
  pinMode(ENC_DT_PIN,  INPUT_PULLUP);
  encState = (digitalRead(ENC_CLK_PIN) << 1) | digitalRead(ENC_DT_PIN);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT_PIN),  encoderISR, CHANGE);

  encoderButton.begin(ENC_SW_PIN, BUTTON_DEBOUNCE_MS, true);

  // GPIO34: input only, no internal pull-up. External 10k resistor.
  tilt.begin(TILT_PIN, TILT_DEBOUNCE_MS, false);

  Serial.print("Inputs: ENC_CLK="); Serial.print(ENC_CLK_PIN);
  Serial.print(" ENC_DT=");         Serial.print(ENC_DT_PIN);
  Serial.print(" BT=");             Serial.print(ENC_SW_PIN);
  Serial.print(" TILT=");           Serial.println(TILT_PIN);
}
