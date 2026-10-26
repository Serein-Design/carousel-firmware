// SPDX-License-Identifier: AGPL-3.0-only
// Development builds only (-DSCREENS_TEST_CONSOLE): a calibration screen, so
// the owner judges the palette and the faces on the real panel, at
// brightness levels 1 and 5. Serial letter c shows it over the current
// screen until c again; a flips Chaud and Ambre on it, whatever the hour.
#pragma once

#ifdef SCREENS_TEST_CONSOLE
#include <Adafruit_GFX.h>

#include "palette.h"

void calibrationToggle();
void calibrationFlipPalette();
bool calibrationShown();
const Palette &calibrationPalette(bool low);   // low: as palette() at the lowest levels

// 64x32 holds the six swatch rows or the three face lines, not both: two
// pages. Swatches: each role's name in the label face and a 20x5 block of
// it. Faces: a label line, a body line and clock digits.
void drawCalibration(Adafruit_GFX &g, const Palette &p, bool facesPage);
#endif
