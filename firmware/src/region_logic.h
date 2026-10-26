// SPDX-License-Identifier: AGPL-3.0-only
// The region as text for other programs: the ISO code of each currency (the
// web pages name the money in it) and the fields /api/info gives. Checked on
// the PC by test/test_region.
#pragma once

#include <Arduino.h>

#include "region.h"

// "EUR", "USD", "GBP" or "CHF"; anything else (a byte no panel stores) is EUR
inline const char *currencyCode(uint8_t currency) {
  static const char *const CODES[CURRENCY_COUNT] = {"EUR", "USD", "GBP", "CHF"};
  return currency < CURRENCY_COUNT ? CODES[currency] : CODES[CURRENCY_EUR];
}

// ,"temp_unit":"C","currency":"EUR","clock12":false -- appended to the
// /api/info object; out-of-range bytes read as the defaults
void regionInfoJson(String &j, uint8_t tempUnit, uint8_t currency, bool clock12);
