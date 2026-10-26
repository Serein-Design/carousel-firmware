// SPDX-License-Identifier: AGPL-3.0-only
// The owner's region: the temperature unit, the currency MONEY counts in and
// the clock's 12 or 24 hours. Set on Panel; defaults are what v0.1.0 showed.
#pragma once

#include <stdint.h>

enum TempUnit : uint8_t { TEMP_C, TEMP_F, TEMP_UNIT_COUNT };

enum Currency : uint8_t {
  CURRENCY_EUR,   // €
  CURRENCY_USD,   // $
  CURRENCY_GBP,   // £
  CURRENCY_CHF,   // CHF
  CURRENCY_COUNT
};

// The region as a form posts it (Panel's Region, the Wi-Fi setup's script):
// each the enum's number, the clock 0 (24 h) or 1 (12 h); -1 when the form
// did not post it or posted something else
struct RegionChoice {
  int8_t unit = -1, currency = -1, clock12 = -1;
};
