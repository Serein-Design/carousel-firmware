// SPDX-License-Identifier: AGPL-3.0-only
// Four of the five ambiances (the fifth, Tourbillon, is black_hole.cpp): calm,
// slow, warm pieces that make the panel a lamp when nothing needs saying.
// Every colour comes from the Serein ramp (ambiance_logic.h), capped at 70 %
// of full light, and every pace from the clock, not the frame count, so a
// busy loop slows nothing down and a fast one speeds nothing up.
// Braises (FIRE), Étoiles (STARS), Encre (PLASMA), Colonie (LIFE): the names
// in brackets are the stored ones and do not change.
#include "ambiance_logic.h"
#include "boot_logic.h"
#include "display.h"
#include "screens/screens.h"

// A phase that turns perSecond steps a second, 256 steps a turn of sinTab;
// it wraps with uint32_t, a multiple of 256, so it never jumps
static uint8_t phase(uint32_t ms, uint32_t perSecond) {
  return (uint8_t)((ms / 1000) * perSecond + (ms % 1000) * perSecond / 1000);
}

// A draw from a few numbers, the same every time it is asked
static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 73856093u ^ b * 19349663u ^ c * 83492791u;
  h ^= h >> 15;
  return h * 2654435761u;
}

// --- Colonie (LIFE) ----------------------------------------------------------------
// Four generations a second: the living beige, births orange, the gone an
// ember trail two generations long; a new seed dissolves in every 30 s.
void screenLife(uint32_t, bool init) {
  static Colony colony;   // 1 KB of bit grids, kept out of the loop's stack
  const uint32_t now = millis();
  if (init) colony.begin((uint8_t)W(), (uint8_t)H(), (uint32_t)random(1, 0x7FFFFFFF), now);
  colony.update(now);
  const uint16_t look[] = {0, ramp(RAMP_TOP), ramp(RAMP_ORANGE), ramp(RAMP_EMBER), ramp(5)};
  for (int y = 0; y < H(); y++)
    for (int x = 0; x < W(); x++)
      display->drawPixel(x, y, look[(uint8_t)colony.cell((uint8_t)x, (uint8_t)y)]);
}

// --- Encre (PLASMA) ----------------------------------------------------------------
// Today's plasma at a third of its pace at 60 fps, read through the ramp
// with the dark end favoured: ink spreading in water.
void screenPlasma(uint32_t, bool init) {
  static uint32_t start;
  const uint32_t now = millis();
  if (init) start = now;
  const uint32_t t16 = (uint32_t)((uint64_t)(now - start) * 20 * 16 / 1000);   // 20 steps a second, in 16ths
  const uint8_t a = (uint8_t)(t16 >> 4), b = (uint8_t)(t16 >> 5);
  for (int y = 0; y < H(); y++)
    for (int x = 0; x < W(); x++) {
      const uint16_t s = sinTab[(uint8_t)(x * 5 + a)] + sinTab[(uint8_t)(y * 7 - a * 2)] +
                         sinTab[(uint8_t)((x + y) * 4 + b)];
      const uint32_t u = s / 3;   // 0..255
      display->drawPixel(x, y, ramp((uint8_t)(5 + 26 * (u * u / 255) / 255)));
    }
}

// --- Braises (FIRE) ----------------------------------------------------------------
// Embers in the lower third under two slow waves, a grain that changes once
// a second, and now and then a spark rising and cooling down the ramp.
void screenFire(uint32_t, bool init) {
  static uint32_t start;
  const uint32_t now = millis();
  if (init) start = now;
  const uint32_t t = now - start, second = t / 1000;
  const int w = W(), h = H();
  const uint8_t a = phase(t, 37), b = phase(t, 24), c = phase(t, 16);   // 0.9, 0.6, 0.4 rad/s

  display->clearScreen();
  for (int y = 0; y < h; y++) {
    const int rise = (h - 1 - y) * 256 / (h - 1);   // 0 on the bottom row, 256 on the top
    const int base = 256 - rise * 13 / 5;           // nothing above the lower ~38 %
    if (base <= 0) continue;
    const int bend = (sinTab[(uint8_t)(y * 16 + b)] - 128) / 2;
    for (int x = 0; x < w; x++) {
      const int s = sinTab[(uint8_t)(x * 14 + a + bend)] - 128;
      const int k = sinTab[(uint8_t)(x * 5 - c)] - 128;
      const int n = 128 + s * k / 128;                                  // 0..255
      const int v = base * (141 + n * 115 / 256) / 256 + (int)(hash3(x, y, second) % 21);
      if (v > 10) display->drawPixel(x, y, rampLevel((uint8_t)(v > 255 ? 255 : v)));
    }
  }

  // Three sparks, each on its own 2.9 s cycle, each seen on a third of them
  const int y0 = h * 2 / 3 + 1;
  for (uint32_t k = 0; k < 3; k++) {
    const uint32_t m = (t / 1000) * 350 + (t % 1000) * 350 / 1000 + k * 370;   // thousandths of a cycle
    const uint32_t cycle = m / 1000, life = m % 1000;
    if (hash3(k, cycle, 1) % 3) continue;
    const int sway = (sinTab[(uint8_t)(life * 245 / 1000)] - 128) / 64;
    const int x = 2 + (int)(hash3(k, cycle, 9) % (uint32_t)(w - 4)) + sway;
    const int y = y0 - (int)(life * (uint32_t)(y0 - 2) / 1000);
    display->drawPixel(x, y, rampLevel((uint8_t)(230 * (1000 - life) / 1000)));
  }
}

// --- Étoiles (STARS) ---------------------------------------------------------------
// Fixed warm stars, beige and a few orange, each twinkling on its own 1-3 s
// cycle between 45 and 100 % of its own light; a shooting star every 20 s.
void screenStars(uint32_t, bool init) {
  struct Star { uint8_t x, y, period, at, light; bool orange; };
  static const int COUNT = 46;
  static Star stars[COUNT];
  static uint32_t start;
  const uint32_t now = millis();
  if (init) {
    start = now;
    for (Star &s : stars)
      s = {(uint8_t)random(0, W()), (uint8_t)random(0, H()), (uint8_t)random(0, 256),
           (uint8_t)random(0, 256), (uint8_t)random(150, 256), random(0, 5) == 0};
  }
  const uint32_t t = now - start;

  display->clearScreen();
  for (const Star &s : stars) {
    const uint16_t p = starPeriodMs(s.period);
    const uint8_t turn = (uint8_t)((t + (uint32_t)s.at * p / 256) % p * 256 / p);
    const uint32_t twinkle = 115 + sinTab[turn] * 140u / 255;   // 45..100 %
    display->drawPixel(s.x, s.y, bootFaded(ramp(s.orange ? RAMP_ORANGE : RAMP_TOP),
                                           (uint8_t)(twinkle * s.light / 255)));
  }

  const int16_t along = shootingStar(t);
  if (along < 0) return;
  // From the upper right, down to the left, its tail fading behind it
  const float dx = W() * 5 / 8.0f, dy = H() * 3 / 8.0f, p = along / 1024.0f;
  const float hx = W() * 25 / 32.0f - dx * p, hy = 4 + dy * p;
  for (int k = 0; k < 6; k++)
    display->drawPixel((int)(hx + k), (int)(hy - k * dy / dx),
                       bootFaded(ramp(RAMP_TOP), (uint8_t)(255 * (6 - k) / 6 * (1 - p))));
}
