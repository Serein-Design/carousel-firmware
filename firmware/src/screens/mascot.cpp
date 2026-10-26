// SPDX-License-Identifier: AGPL-3.0-only
// The mascot (Nimbus by default) and the screens that star it on its own:
// walk, dance, sleep and run. The session monitor lives in agent_monitor.cpp.
#include "display.h"
#include "grid.h"
#include "i18n.h"
#include "mascot_logic.h"
#include "palette.h"
#include "screens/screens.h"
#include "screens/mascot.h"
#include "type.h"

// --- Nimbus sprite -------------------------------------------------------------
// See mascot.h for the 18 x 10 layout. Both functions are weak: an optional,
// git-ignored override, when present, replaces them at link time.

// 75 % of an RGB565 colour: the shaded underside of the cloud
static uint16_t darker(uint16_t c) {
  const uint16_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return (uint16_t)(((r * 3 / 4) << 11) | ((g * 3 / 4) << 5) | (b * 3 / 4));
}

__attribute__((weak)) const char *mascotName() { return nullptr; }

__attribute__((weak)) void drawMascot(int x, int y, int s, const MascotPose &p) {
  // Default colour: cloud white. The session monitor replaces it with the
  // tint of the displayed session.
  const uint16_t body = p.body ? p.body : display->color565(232, 240, 255);
  const int e = p.squash;

  // The cloud, row by row: first column and width. Breathing (squash) drops
  // the top rows, so the cloud flattens instead of shrinking.
  static const int8_t ROWS[8][2] = {{6, 4}, {4, 9}, {3, 12}, {2, 14}, {2, 14}, {2, 14}, {2, 14}, {3, 12}};
  for (int r = e; r < 8; r++)
    display->fillRect(x + ROWS[r][0] * s, y + r * s, ROWS[r][1] * s, s, r == 7 ? darker(body) : body);

  // Hands: two puffs on the sides, at the pose's height
  display->fillRect(x,          y + (p.armL + e) * s, 2 * s, 2 * s, body);
  display->fillRect(x + 16 * s, y + (p.armR + e) * s, 2 * s, 2 * s, body);

  // Moving: drizzle under the cloud, every other drop one pixel lower
  if (p.legs) {
    const uint16_t drop = display->color565(110, 170, 255);
    const int DROP_X[4] = {4, 7, 10, 13};
    for (int i = 0; i < 4; i++) {
      const bool low = (p.legs == 1) == (i % 2 == 0);
      display->fillRect(x + DROP_X[i] * s, y + (low ? 9 : 8) * s, s, s, drop);
    }
  }

  // Pixels switched off inside the cloud, in sprite coordinates
  auto hole = [&](int px, int py, int pw, int ph) {
    display->fillRect(x + px * s, y + (py + e) * s, pw * s, ph * s, BLACK);
  };
  const int EYE_X[2] = {4, 10};

  switch (p.eyes) {
    case EYES_OPEN:
      hole(5 + p.gaze, 3, 2, 2);
      hole(11 + p.gaze, 3, 2, 2);
      break;
    case EYES_CLOSED:
      hole(5, 4, 2, 1);
      hole(11, 4, 2, 1);
      break;
    case EYES_HAPPY:                         //  ^ ^
      for (int ox : EYE_X) {
        hole(ox, 4, 1, 1); hole(ox + 1, 3, 1, 1); hole(ox + 2, 4, 1, 1);
      }
      break;
    case EYES_KO:                            //  x x
      for (int ox : EYE_X) {
        hole(ox, 3, 1, 1); hole(ox + 2, 3, 1, 1); hole(ox + 1, 4, 1, 1);
        hole(ox, 5, 1, 1); hole(ox + 2, 5, 1, 1);
      }
      break;
  }
  hole(8, 6, 2, 1);                          // mouth
}

// True for 6 frames, then schedules the next blink 2 to 5 s later
bool isBlinking(uint32_t f, uint32_t &next) {
  if (f < next) return false;
  if (f >= next + 6) next = f + random(120, 320);
  return true;
}

// A short word in a bubble over the scene: body face on a black card, so it
// reads over whatever passes behind it. cx is its middle, y its cap top.
static void drawCallout(int cx, int y, const char *txt, uint16_t color) {
  const int16_t w = (int16_t)textWidth(txt, Face::BODY);
  const int16_t x = calloutX((int16_t)cx, w, (int16_t)W());
  display->fillRect(x - 1, y - 1, w + 2, 9, BLACK);
  drawText(*display, txt, x, y, Face::BODY, color);
}

// --- Mascot: walk -------------------------------------------------------------
// It walks, waves hello at the edge of the screen and jumps now and then.
void screenMascotWalk(uint32_t f, bool init) {
  const int JUMP_FRAMES = 24;
  const int WAVE_FRAMES = 110;

  static float    x;
  static int      dir;
  static int      wave, jump;
  static uint32_t nextBlink;

  int   s      = (W() >= 40) ? 2 : 1;
  int   width  = 18 * s;
  int   height = 10 * s;
  int   ground = H() - 1;
  float xMax   = W() - width;

  if (init) {
    x = 0; dir = 1; wave = 0; jump = 0;
    nextBlink = 90;
  }

  // Movement
  if (wave > 0) {
    if (--wave == 0) dir = -dir;
  } else {
    x += dir * 0.12f * s;
    if (x <= 0 || x >= xMax) {
      x = (x <= 0) ? 0 : xMax;
      if (random(0, 2) == 0) wave = WAVE_FRAMES;
      else                   dir = -dir;
    }
    if (jump == 0 && random(0, 350) == 0) jump = JUMP_FRAMES;
  }

  // Parabolic jump, 4 logical pixels high at most
  int dy = 0;
  if (jump > 0) {
    int t = JUMP_FRAMES - jump;
    dy = t * (JUMP_FRAMES - t) * 4 * s / ((JUMP_FRAMES / 2) * (JUMP_FRAMES / 2));
    jump--;
  }

  MascotPose p;
  p.eyes  = isBlinking(f, nextBlink) ? EYES_CLOSED : EYES_OPEN;
  p.gaze  = (wave > 0) ? 0 : dir;
  p.legs  = (wave == 0 && jump == 0) ? 1 + (f / 8) % 2 : 0;
  p.armR  = (wave > 0) ? (((f / 10) % 2) ? 1 : 3) : 5;
  int y   = ground - height - dy;

  const Palette &pal = palette();
  display->clearScreen();
  display->drawFastHLine(0, ground, W(), pal.dim);
  drawMascot((int)x, y, s, p);

  if (wave > 0 && y - 9 >= 0) drawCallout((int)x + width / 2, y - 9, tr().mascotHi, pal.text);
}

// --- Mascot: dance ------------------------------------------------------------
// Light-up dance floor, music notes and a 4-beat routine.
void screenMascotDance(uint32_t f, bool init) {
  const int     NOTE_COUNT = 6;
  const uint8_t NOTE[5]    = {0b0011, 0b0010, 0b0010, 0b1110, 0b1100};

  static float nx[NOTE_COUNT], ny[NOTE_COUNT], nv[NOTE_COUNT];

  int s      = (W() >= 40) ? 2 : 1;
  int width  = 18 * s;
  int height = 10 * s;
  int ground = H() - 3;
  int beat   = f / 16;                     // one beat = 16 frames
  int t      = f % 16;

  if (init) {
    for (int i = 0; i < NOTE_COUNT; i++) {
      nx[i] = random(0, W() - 4);
      ny[i] = random(0, ground);
      nv[i] = random(10, 30) / 100.0f;
    }
  }

  const Palette &pal = palette();
  display->clearScreen();

  // Floor tiles lighting up in a checkerboard on the beat: the accent where
  // lit, the ghost where not
  const int TILE = 8;
  for (int i = 0; i * TILE < W(); i++)
    display->fillRect(i * TILE, ground, TILE - 1, 3, ((i + beat) % 2) ? pal.accent : pal.ghost);

  // Notes rising in a wavy line, in the label colour and the accent by turns
  for (int i = 0; i < NOTE_COUNT; i++) {
    int bx = (int)(nx[i] + sinf((ny[i] + i * 10) * 0.25f) * 1.5f);
    int by = (int)ny[i];
    uint16_t c = (i % 2) ? pal.accent : pal.label;
    for (int r = 0; r < 5; r++)
      for (int k = 0; k < 4; k++)
        if (NOTE[r] & (0b1000 >> k)) display->drawPixel(bx + k, by + r, c);

    ny[i] -= nv[i];
    if (ny[i] < -5) {
      nx[i] = random(0, W() - 4);
      ny[i] = ground - 5;
      nv[i] = random(10, 30) / 100.0f;
    }
  }

  MascotPose p;
  p.eyes   = EYES_HAPPY;
  p.legs   = 1 + beat % 2;
  p.squash = (t < 4) ? 1 : 0;              // small knee bend on every beat
  int dy   = 0;
  switch (beat % 4) {
    case 0: p.armL = -1; p.armR = 5;  break;
    case 1: p.armL = 5;  p.armR = -1; break;
    case 2: p.armL = -1; p.armR = -1; break;
    case 3:                                // jump, arms up
      p.armL = 1; p.armR = 1; p.legs = 0; p.squash = 0;
      dy = t * (16 - t) * 3 * s / 64;
      break;
  }
  int x = (W() - width) / 2 + ((beat % 2) ? s : -s);
  drawMascot(x, ground - height - dy, s, p);
}

// --- Mascot: sleep ------------------------------------------------------------
// Starry night: he breathes slowly and lets Zs drift away.
// Now and then he opens his eyes, looks around and falls back asleep.
void screenMascotSleep(uint32_t f, bool init) {
  const int STAR_COUNT  = 22;
  const int Z_COUNT     = 4;
  const int WAKE_FRAMES = 150;

  static uint8_t  sx[STAR_COUNT], sy[STAR_COUNT], sphase[STAR_COUNT];
  static float    zx[Z_COUNT], zy[Z_COUNT];
  static int      zage[Z_COUNT];           // -1 = free
  static int      waking;
  static uint32_t nextZ, nextWake;

  bool landscape = W() >= 40;
  int  s      = landscape ? 2 : 1;
  int  width  = 18 * s;
  int  height = 10 * s;
  int  ground = H() - 1;
  int  cx     = landscape ? 4 : (W() - width) / 2;
  int  cy     = ground - height;

  if (init) {
    for (int i = 0; i < STAR_COUNT; i++) {
      sx[i]     = random(0, W());
      sy[i]     = random(0, cy - 2);
      sphase[i] = random(0, 256);
    }
    for (int i = 0; i < Z_COUNT; i++) zage[i] = -1;
    waking   = 0;
    nextZ    = 20;
    nextWake = random(500, 900);
  }

  if (waking > 0) {
    if (--waking == 0) nextWake = f + random(500, 900);
  } else if (f >= nextWake) {
    waking = WAKE_FRAMES;
  }

  if (waking == 0 && f >= nextZ) {
    for (int i = 0; i < Z_COUNT; i++) {
      if (zage[i] >= 0) continue;
      zage[i] = 0;
      zx[i]   = cx + width - 4 * s;
      zy[i]   = landscape ? cy + 2 : cy - 8;
      break;
    }
    nextZ = f + 50;
  }

  const Palette &pal = palette();
  display->clearScreen();

  // Stars breathe between the ghost and the dim, one slow cycle in 4 s
  for (int i = 0; i < STAR_COUNT; i++) {
    const uint8_t l = sinTab[(uint8_t)(f + sphase[i])];
    display->drawPixel(sx[i], sy[i], blend565(pal.ghost, pal.dim, l));
  }

  // Crescent moon: a disc in the text colour eaten by a black disc
  int mx = W() - 9, my = 8;
  display->fillCircle(mx, my, 5, pal.text);
  display->fillCircle(mx - 3, my - 2, 5, BLACK);

  display->drawFastHLine(0, ground, W(), pal.dim);

  // The Zs: the label face while small, the body face once bigger, fading out
  for (int i = 0; i < Z_COUNT; i++) {
    if (zage[i] < 0) continue;
    const uint16_t c = blend565(pal.label, pal.off, (uint8_t)(zage[i] * 255 / 90));
    drawText(*display, "Z", (int)zx[i], (int)zy[i], zage[i] < 30 ? Face::LABEL : Face::BODY, c);
    zx[i] += landscape ? 0.2f  : 0.06f;
    zy[i] -= landscape ? 0.12f : 0.25f;
    if (++zage[i] > 90) zage[i] = -1;
  }

  MascotPose p;
  int r = WAKE_FRAMES - waking;            // how far into the wake-up
  if (waking > 0 && r <= 135) {
    p.gaze = (r < 50) ? -1 : (r < 100) ? 1 : 0;
  } else {
    p.eyes   = EYES_CLOSED;
    p.squash = (waking == 0 && (f % 90) >= 45) ? 1 : 0;   // breathing
  }
  drawMascot(cx, cy, s, p);

  if (waking > 0 && r >= 100 && r <= 135) {
    if (landscape) drawCallout(cx + width + 6, cy + 4, "?", pal.text);
    else           drawCallout(W() / 2, cy - 9, "?", pal.text);
  }
}

// --- Mascot: run ----------------------------------------------------------------
// Game on autopilot: the mascot jumps over the bugs.
// It misses one now and then, and the score goes back to zero.
void screenMascotRun(uint32_t f, bool init) {
  const int   OBS_COUNT    = 4;
  const float GRAVITY      = 0.14f;
  const float JUMP_IMPULSE = 2.2f;
  const int   CRASH_FRAMES = 100;

  static float    obsX[OBS_COUNT];
  static uint8_t  obsType[OBS_COUNT];      // 0 = bug, 1 = error block
  static bool     obsActive[OBS_COUNT], obsSeen[OBS_COUNT];
  static float    speed, altitude, vy, scroll, untilObstacle;
  static bool     jumping;
  static uint32_t score, best;
  static int      crash;
  static uint32_t nextBlink;

  const int cx     = 2;
  const int ground = H() - 3;
  const int cy     = ground - 10;

  auto obsWidth  = [](uint8_t type) { return type == 0 ? 5 : 3; };
  auto obsHeight = [](uint8_t type) { return type == 0 ? 4 : 7; };
  auto restart = [&]() {
    for (int i = 0; i < OBS_COUNT; i++) obsActive[i] = false;
    speed = 0.8f; altitude = 0; vy = 0; jumping = false;
    score = 0; untilObstacle = 20;
  };

  if (init) {
    restart();
    crash = 0; scroll = 0;
    nextBlink = 60;
  }

  if (crash > 0) {
    if (--crash == 0) restart();
  } else {
    scroll += speed;
    if (f % 6 == 0) score++;
    speed = 0.8f + score * 0.002f;
    if (speed > 1.8f) speed = 1.8f;

    // Obstacles: spawn, advance, despawn
    untilObstacle -= speed;
    for (int i = 0; i < OBS_COUNT; i++) {
      if (!obsActive[i]) {
        if (untilObstacle <= 0) {
          obsActive[i]  = true;
          obsSeen[i]    = false;
          obsX[i]       = W();
          obsType[i]    = random(0, 2);
          untilObstacle = random(40, 90) + speed * 30;   // enough room to land
        }
        continue;
      }
      obsX[i] -= speed;
      if (obsX[i] < -6) obsActive[i] = false;
    }

    // Autopilot: jumps just before the obstacle, misses 1 time in 12
    if (!jumping) {
      for (int i = 0; i < OBS_COUNT; i++) {
        if (!obsActive[i] || obsSeen[i]) continue;
        float d = obsX[i] - (cx + 16);
        if (d >= 0 && d <= speed * 4) {
          obsSeen[i] = true;
          if (random(0, 12) != 0) { jumping = true; vy = JUMP_IMPULSE; }
        }
      }
    }

    if (jumping) {
      altitude += vy;
      vy       -= GRAVITY;
      if (altitude <= 0) { altitude = 0; jumping = false; }
    }

    for (int i = 0; i < OBS_COUNT; i++) {
      if (!obsActive[i]) continue;
      int  ox       = (int)obsX[i];
      bool overlaps = ox < cx + 16 && ox + obsWidth(obsType[i]) > cx + 2;
      if (overlaps && altitude < obsHeight(obsType[i])) {
        crash = CRASH_FRAMES;
        if (score > best) best = score;
        break;
      }
    }
  }

  const Palette &pal = palette();
  display->clearScreen();

  // Scrolling ground
  display->drawFastHLine(0, ground, W(), pal.dim);
  for (int x = 0; x < W(); x++)
    if ((x + (int)scroll) % 9 == 0) display->drawPixel(x, ground + 2, pal.ghost);

  for (int i = 0; i < OBS_COUNT; i++) {
    if (!obsActive[i]) continue;
    int ox = (int)obsX[i];
    if (obsType[i] == 0) {
      // Bug:  #...#
      //       .###.
      //       #####
      //       .#.#.
      uint16_t c = display->color565(90, 210, 60);
      display->drawPixel(ox, ground - 4, c);
      display->drawPixel(ox + 4, ground - 4, c);
      display->drawFastHLine(ox + 1, ground - 3, 3, c);
      display->drawFastHLine(ox, ground - 2, 5, c);
      display->drawPixel(ox + 1, ground - 1, c);
      display->drawPixel(ox + 3, ground - 1, c);
    } else {
      // Error block with a "!": the alert colour, the mark in the text colour
      display->fillRect(ox, ground - 7, 3, 7, pal.alert);
      display->drawFastVLine(ox + 1, ground - 6, 3, pal.text);
      display->drawPixel(ox + 1, ground - 2, pal.text);
    }
  }

  MascotPose p;
  if (crash > 0) {
    p.eyes = EYES_KO;
  } else {
    bool step = (f / 5) % 2;
    p.eyes = isBlinking(f, nextBlink) ? EYES_CLOSED : EYES_OPEN;
    p.gaze = 1;
    p.legs = jumping ? 0 : 1 + step;
    p.armL = jumping ? 2 : (step ? 4 : 5);
    p.armR = jumping ? 2 : (step ? 5 : 4);
  }
  drawMascot(cx, cy - (int)altitude, 1, p);

  // The counter in Pierre, flush right clear of Nimbus's jump; after a crash
  // the best score, under "BUG!" and its label
  const RunCounter c = runCounter(crash > 0 ? best : score);
  drawText(*display, c.text, c.x, c.y, Face::PIERRE, crash > 0 ? pal.accent : pal.text);
  if (crash > 0) {
    const CrashLine l = runCrashLine(tr());
    drawText(*display, tr().runBug, l.bugX, l.y, Face::LABEL, pal.alert);
    drawText(*display, tr().runBest, l.bestX, l.y, Face::LABEL, pal.label);
  }
}
