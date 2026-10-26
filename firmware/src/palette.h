// SPDX-License-Identifier: AGPL-3.0-only
// The panel's colours, by role: screens ask for a role, never a colour, so
// the night palette and a later recalibration touch one table. Chaud by day,
// Ambre during the night hours. Content (weather art, images, the mascot)
// keeps its natural colours and does not use these.
#pragma once

#include <stdint.h>

struct Palette {   // RGB565
  uint16_t text;     // words and numbers of the system UI
  uint16_t label;    // labels, units, secondary lines ("ember")
  uint16_t accent;   // the one live thing on a screen
  uint16_t alert;    // errors, always with a word or an icon
  uint16_t dim;      // frames, separators, empty gauge steps
  uint16_t ghost;    // guides only
  uint16_t off;      // the panel's black
};

// Ambre when night, Chaud otherwise; low: the variant for the lowest
// brightness, where the panel keeps too few shades for the quiet roles
const Palette &paletteFor(bool night, bool low = false);
// Whether a panel brightness (0-255, as applied) is that low: levels 1 and 2
// of 6, and the night levels up to 25
bool lowLight(uint8_t brightness8);
// The palette at hour h for the night hours [start, end), as the brightness
// counts them (inHourWindow): they may cross midnight, equal means none.
const Palette &paletteAtHour(int h, int start, int end);
// The palette now: Ambre while the night hours are on, its low variant at
// the lowest brightness (gallery.cpp knows the time, the hours and the level)
const Palette &palette();
