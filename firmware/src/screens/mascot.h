// SPDX-License-Identifier: AGPL-3.0-only
// The panel's mascot: Nimbus, a small cloud, in pixel art.
//
//   ......####........    18 x 10 logical pixel sprite, scaled x2 when
//   ....#########.....    there is room. Rows 0-7 are the cloud (columns
//   ...############...    2-15), the two side puffs are its hands, and
//   ..###  ####  ###..    drizzle under it stands in for legs.
//   ..###  ####  ###..
//   ##################    The eyes and mouth are pixels switched off
//   ########  ########    inside the cloud.
//   ...############...
//   ....:..:..:..:....
//
// Every screen draws the mascot through drawMascot() with a pose, so another
// character can replace it in one place: an optional, git-ignored override
// defines drawMascot() and mascotName() and wins at link time.
#pragma once

#include <Arduino.h>

enum MascotEyes { EYES_OPEN, EYES_CLOSED, EYES_HAPPY, EYES_KO };

struct MascotPose {
  int        gaze   = 0;             // open eyes: -1 left, 0 front, +1 right
  MascotEyes eyes   = EYES_OPEN;
  int        legs   = 0;             // 0 still, 1 or 2 = moving (alternating drizzle)
  int        armL   = 5;             // hand height: 5 resting, smaller = raised
  int        armR   = 5;
  int        squash = 0;             // 1 = flattened by one pixel (breathing)
  uint16_t   body   = 0;             // 0 = the mascot's own colour; otherwise the session tint
};

void drawMascot(int x, int y, int s, const MascotPose &p);

// The mascot screen's banner name, or nullptr for the translated default
const char *mascotName();
