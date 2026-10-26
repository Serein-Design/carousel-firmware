// SPDX-License-Identifier: AGPL-3.0-only
// The grid every system screen shares ("G1 Fil"), so the eye finds the same
// things in the same rows. Rows are inclusive. Content screens (images,
// ambiances, weather art) may go full bleed; system text never does.
#pragma once

#include <stdint.h>

// Landscape, 64x32
static const int16_t GRID_MARGIN     = 2;    // columns 0-1 and 62-63 stay empty
static const int16_t GRID_LINE_W     = 60;   // 64 - 2 margins: the widest system line
static const int16_t GRID_LABEL_TOP  = 2;    // label caps, 3x5 face
static const int16_t GRID_LABEL_BOT  = 6;
static const int16_t GRID_MAIN_TOP   = 9;    // 13-row digits, or a body line and its detail
static const int16_t GRID_MAIN_BOT   = 22;
static const int16_t GRID_FOOT_TOP   = 25;   // foot caps, 3x5 face
static const int16_t GRID_FOOT_BOT   = 29;
static const int16_t GRID_STRIP_ROW  = 31;   // status strip: time to the next screen, agent activity

// Portrait, 32x64: the same margins and label line; digits stack below it
static const int16_t GRID_PORTRAIT_LINE_W    = 28;   // 32 - 2 margins
static const int16_t GRID_PORTRAIT_LABEL_TOP = 2;
static const int16_t GRID_PORTRAIT_LABEL_BOT = 6;
