// SPDX-License-Identifier: AGPL-3.0-only
#include "https_logic.h"

void HttpsSchedule::after(HttpsJob job, uint32_t now, uint32_t ms) {
  from_[job] = now;
  ms_[job] = ms;
}

void HttpsSchedule::wake(HttpsJob job) { asked_[job] = true; }

bool HttpsSchedule::take(HttpsJob job, uint32_t now, bool &asked) {
  asked = asked_[job];
  asked_[job] = false;
  return asked || now - from_[job] >= ms_[job];
}

uint32_t HttpsSchedule::waitMs(uint32_t now) const {
  uint32_t wait = HTTPS_LOOK_MS;
  for (int j = 0; j < HTTPS_JOBS; j++) {
    if (asked_[j]) return 0;
    const uint32_t gone = now - from_[j];
    if (gone >= ms_[j]) return 0;
    if (ms_[j] - gone < wait) wait = ms_[j] - gone;
  }
  return wait;
}
