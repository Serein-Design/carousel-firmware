// SPDX-License-Identifier: AGPL-3.0-only
// Weather from MET Norway's forecast (free, no key), fetched in the background.
#pragma once

#include <Arduino.h>

#include "weather_logic.h"

// Written by the HTTPS task on core 0, read by the screen on core 1 without
// a lock: a frame drawn during an update may mix two answers for 1/60 s.
struct Weather {
  bool     valid     = false;
  float    temp      = 0;
  float    tMin      = 0;
  float    tMax      = 0;
  WxCond   cond      = WX_UNKNOWN;
  bool     day       = true;   // the symbol does not say night
  uint32_t updatedAt = 0;      // millis() of the last success
  HourlyForecast hourly;       // hour 0 is now; count 1: W3 shows now only
};
extern Weather weather;

// The HTTPS task's turn (https_task.cpp): looks up the city and fetches the
// forecast when due, or asked (the city changed). Returns the ms until the
// next look.
uint32_t weatherStep(bool asked);

// The city changed: find it and fetch its weather now, not in an hour.
void weatherRefresh();
// The last city searched for does not exist (shown on the settings page).
extern volatile bool locateFailed;
