// SPDX-License-Identifier: AGPL-3.0-only
// ---------------------------------------------------------------------------
// Tourbillon (BLACKHOLE, its stored name): a warm accretion disk turning
// slowly around a dark core, faster near the centre, the back of the disk
// bent over the hole (gravitational lensing), and now and then a star drawn
// in and swallowed. An ambiance: its colours from the Serein ramp, its pace
// from the clock (ambiances.cpp says why).
// ---------------------------------------------------------------------------
#include "ambiance_logic.h"
#include "display.h"
#include "screens/screens.h"

namespace blackhole {
  const int PARTICLE_COUNT = 240;
  const int STAR_COUNT = 16;
  struct Particle { float r, a; };
  struct Star     { int8_t x, y; uint8_t shade; };
  struct Body     { float r, a, px, py; };

  Particle particles[PARTICLE_COUNT];
  Star     stars[STAR_COUNT];
  Body     victim;
  bool     hasVictim = false;

  // Light by distance, beige inside to bordeaux at the rim, times k (the
  // side coming towards us is brighter)
  uint16_t diskColor(float r, float rMax, float k) {
    const float t = constrain((r - 3.0f) / (rMax - 3.0f), 0.0f, 1.0f);
    return rampLevel((uint8_t)constrain(255 * (1 - 0.6f * t) * k, 0.0f, 255.0f));
  }

  // back = half of the disk that passes behind the hole (sine of the angle < 0)
  void drawDisk(bool back, float cx, float cy, float rMax, float squash, int rh) {
    for (const Particle &p : particles) {
      const float s = sinf(p.a), c = cosf(p.a);
      if ((s < 0) != back) continue;
      const float doppler = 1.0f - c * 0.3f;
      display->drawPixel((int)(cx + c * p.r), (int)(cy + s * p.r * squash),
                         diskColor(p.r, rMax, back ? doppler * 0.8f : doppler));
      if (back && p.r < rMax * 0.5f) {          // bent image above the hole
        const float radius = rh + 1 + p.r * 0.35f, dx = c * radius;
        const float under = radius * radius - dx * dx;
        const int ly = (int)(cy - sqrtf(under > 0 ? under : 0));
        display->drawPixel((int)(cx + dx), ly, diskColor(p.r, rMax, doppler * 0.6f));
      }
    }
  }
}

void screenBlackHole(uint32_t, bool init) {
  const bool  landscape = W() >= 60;
  const float cx = W() / 2.0f, cy = H() / 2.0f;
  const float rMax = landscape ? 30.0f : 15.0f, squash = landscape ? 0.3f : 0.45f;
  const int   rh = 3;
  static uint32_t last;
  const uint32_t now = millis();

  if (init) {
    for (auto &p : blackhole::particles) {
      p.r = random(40, (long)(rMax * 10)) / 10.0f;
      p.a = random(0, 628) / 100.0f;
    }
    for (auto &s : blackhole::stars) {
      s.x = random(0, W()); s.y = random(0, H()); s.shade = random(5, RAMP_EMBER + 1);
    }
    blackhole::hasVictim = false;
    last = now;
  }
  // Seconds since the last frame, at most 50 ms: a late frame never jumps
  const float dt = (now - last < 50 ? now - last : 50) / 1000.0f;
  last = now;

  display->clearScreen();
  for (const auto &s : blackhole::stars)
    display->drawPixel(s.x, s.y, ramp(s.shade));   // faint, still: nothing blinks

  // The closer in, the faster it turns (a turn in about 5 s near the hole,
  // a minute at the rim), and everything slowly falls towards the centre
  for (blackhole::Particle &p : blackhole::particles) {
    p.a += 8.0f / powf(p.r, 1.3f) * dt;
    p.r -= 0.2f * rMax / p.r * dt;
    if (p.r < rh + 0.5f) { p.r = rMax - random(0, 30) / 10.0f; p.a = random(0, 628) / 100.0f; }
  }

  blackhole::drawDisk(true, cx, cy, rMax, squash, rh);
  blackhole::drawDisk(false, cx, cy, rMax, squash, rh);
  // Last: nothing lights the core; a steady ring draws its edge
  display->fillCircle((int)cx, (int)cy, rh, BLACK);
  display->drawCircle((int)cx, (int)cy, rh + 1, ramp(RAMP_ORANGE));

  // About every 25 s, a star wanders too close and is drawn in on a spiral
  if (!blackhole::hasVictim && random(0, 1500) == 0) {
    const blackhole::Star &s = blackhole::stars[random(0, blackhole::STAR_COUNT)];
    const float dx = s.x - cx, dy = s.y - cy;
    blackhole::victim = { sqrtf(dx * dx + dy * dy), atan2f(dy, dx), (float)s.x, (float)s.y };
    blackhole::hasVictim = true;
  }
  if (blackhole::hasVictim) {
    blackhole::Body &v = blackhole::victim;
    v.a += 27.0f / v.r * dt;
    v.r -= (8.0f + 90.0f / v.r) * dt;
    const float x = cx + cosf(v.a) * v.r;
    const float y = cy + sinf(v.a) * v.r * (v.r < rMax ? squash + 0.3f : 1.0f);
    display->drawLine((int)v.px, (int)v.py, (int)x, (int)y, ramp(RAMP_ORANGE));
    display->drawPixel((int)x, (int)y, ramp(RAMP_TOP));
    v.px = x; v.py = y;
    if (v.r < rh + 1) blackhole::hasVictim = false;
  }
}
