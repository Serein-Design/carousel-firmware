// SPDX-License-Identifier: AGPL-3.0-only
// What the mascot's screens place around the art: RUN's counter and its crash
// line beside Nimbus, where a bubble stands, and the fade of the Zs and stars.
// Hardware-free and header-only (test_poster_cells builds screens/mascot.cpp
// alone): host-tested in test_mascot_logic; screens/mascot.cpp draws.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "grid.h"
#include "i18n.h"
#include "type.h"

// --- RUN's counter --------------------------------------------------------------
// Landscape only. Nimbus stands in columns 2-19 (hands included); the counter
// and the crash line keep the columns from RUN_LEFT_LIMIT to RUN_RIGHT, so a
// jump up to the top never meets them.
static const int16_t  RUN_LEFT_LIMIT = 22;
static const int16_t  RUN_RIGHT      = 62;   // the column after the last digit (2 px margin)
static const uint32_t RUN_SCORE_MAX  = 9999; // four Pierre digits are 38 px, a fifth would not fit
static const int16_t  CRASH_GAP      = 2;    // between "BUG!" and "BEST": all the French "RECORD" leaves

struct RunCounter {
  char    text[8];
  int16_t x, y;   // top left corner of the Pierre digits
};

struct CrashLine {
  int16_t bugX, bestX, y;   // the label line's two words (label face)
};

// The score as digits, held at RUN_SCORE_MAX
inline void runCounterText(uint32_t score, char out[8]) {
  snprintf(out, 8, "%lu", (unsigned long)(score > RUN_SCORE_MAX ? RUN_SCORE_MAX : score));
}

// The digits flush right on the grid's main rows (obstacles start below them)
inline RunCounter runCounter(uint32_t score) {
  RunCounter c;
  runCounterText(score, c.text);
  c.x = (int16_t)(RUN_RIGHT - (int16_t)textWidth(c.text, Face::PIERRE));
  c.y = GRID_MAIN_TOP;
  return c;
}

// On a crash the label line says "BUG!" then "BEST", flush right, above the
// best score in Pierre
inline CrashLine runCrashLine(const Strings &t) {
  CrashLine l;
  const int16_t bestW = (int16_t)textWidth(t.runBest, Face::LABEL);
  const int16_t bugW  = (int16_t)textWidth(t.runBug, Face::LABEL);
  l.bestX = (int16_t)(RUN_RIGHT - bestW);
  l.bugX  = (int16_t)(l.bestX - CRASH_GAP - bugW);
  l.y     = GRID_LABEL_TOP;
  return l;
}

// --- Bubbles --------------------------------------------------------------------
// The left edge of a bubble w px wide centred on cx, kept on a panel screenW wide
inline int16_t calloutX(int16_t cx, int16_t w, int16_t screenW) {
  int16_t x = (int16_t)(cx - w / 2);
  if (x > screenW - w) x = (int16_t)(screenW - w);
  if (x < 0) x = 0;
  return x;
}

// --- Fade -----------------------------------------------------------------------
// a at t = 0 to b at t = 255, each RGB565 channel alone
inline uint16_t blend565(uint16_t a, uint16_t b, uint8_t t) {
  const uint32_t k = t, j = 255 - t;
  const uint32_t r  = (((a >> 11) & 31) * j + ((b >> 11) & 31) * k + 127) / 255;
  const uint32_t g  = (((a >> 5) & 63) * j + ((b >> 5) & 63) * k + 127) / 255;
  const uint32_t bl = ((a & 31) * j + (b & 31) * k + 127) / 255;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}
