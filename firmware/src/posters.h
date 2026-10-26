// SPDX-License-Identifier: AGPL-3.0-only
// The screens' thumbnails for the web pages: one indexed PNG of 64x32 cells,
// stacked top to bottom, one per screen in the firmware's order, then three
// frames the pages also show. The bytes live in posters.cpp, written by
// tools/simulator/posters.mjs from the simulator's own drawings; checked on
// the PC by test/test_posters.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "screen_list.h"

extern const uint8_t POSTERS_PNG[];
extern const size_t POSTERS_PNG_LEN;        // at most 12 288 bytes (test_posters)
extern const char *const POSTER_NAMES[];    // each cell's English name, "@" for a frame

// A screen's cell is its firmware index; the three frames follow, then one
// cell per style of CODE (cloud, list), CLOCK and WEATHER, named "CODE:1"
// for CODE in its style 1.
static const int POSTER_SIGN_IN = SCREEN_COUNT;       // the sign-in code, digits hidden
static const int POSTER_LOCKED = SCREEN_COUNT + 1;    // RESET FAILED
static const int POSTER_NO_IMAGE = SCREEN_COUNT + 2;  // IMAGE with nothing to play
static const int POSTER_STYLE_CELLS = 2 + 3 + 3;
static const int POSTER_COUNT = SCREEN_COUNT + 3 + POSTER_STYLE_CELLS;

// The cell for a screen drawn in one of its styles (screen_style.h): the
// style's own cell (CODE's Auto: the cloud), else the screen's
int posterFor(uint8_t screen, uint8_t style);
