// SPDX-License-Identifier: AGPL-3.0-only
// The words ISS puts on the panel, in the label face. Hardware-free:
// host-tested in test_text_screen.
#pragma once

#include <stddef.h>
#include <stdio.h>

#include "i18n.h"

// Said before any distance: the orbit is simulated
static const char *const ISS_DEMO_LABEL = "DEMO";

// Near, the word replaces the unit ("PRES 1999"): both would be too wide
inline void issDistanceText(char *out, size_t size, bool nearby, int km, const Strings &t) {
  if (nearby) snprintf(out, size, "%s %d", t.near, km);
  else        snprintf(out, size, "%d KM", km);
}

// The close-up's two lines, alternating
inline void issSpeedText(char *out, size_t size, bool altitude) {
  snprintf(out, size, "%s", altitude ? "ALT 420 KM" : "27600 KM/H");
}
