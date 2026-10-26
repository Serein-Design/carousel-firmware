// SPDX-License-Identifier: AGPL-3.0-only
// The ambiances' shared parts, without the drawing: the Serein ramp their
// colours come from, the star sky's timing and the colony (LIFE) with its
// rules, its pace and its reseed. screens/ambiances.cpp and
// screens/black_hole.cpp draw them; host-tested in test_ambiance.
#pragma once

#include <stddef.h>
#include <stdint.h>

// --- The Serein ramp ---------------------------------------------------------------
// Off, lifted bordeaux (120,22,30), orange (232,137,58), beige (232,201,160),
// mixed straight between the stops and capped at 70 % of full light: an
// ambiance is a lamp, never the brightest thing in the room. RGB565, 64 bytes.
static const uint8_t RAMP_SIZE = 32;
extern const uint16_t AMBIANCE_RAMP[RAMP_SIZE];
static const uint8_t RAMP_TOP     = RAMP_SIZE - 1;   // beige
static const uint8_t RAMP_ORANGE  = 21;              // nearest entry to the orange stop
static const uint8_t RAMP_EMBER   = 9;               // a dim bordeaux, for what fades
// Entry i, the last for anything past it
inline uint16_t ramp(uint8_t i) { return AMBIANCE_RAMP[i < RAMP_SIZE ? i : RAMP_TOP]; }
// A level 0..255 on the ramp (eight levels per entry)
inline uint16_t rampLevel(uint8_t v) { return AMBIANCE_RAMP[v >> 3]; }

// --- Étoiles (STARS) ---------------------------------------------------------------
// Every star twinkles on its own cycle of 1 to 3 s, from a 0..255 draw
uint16_t starPeriodMs(uint8_t draw);
// The shooting star: one every 20 s, crossing in 1.4 s at the end of each
// cycle (the first 18.6 s after the screen comes). since: ms since it came;
// 0..1023 along its path, or -1 while there is none.
static const uint32_t SHOOTING_EVERY_MS = 20000, SHOOTING_FOR_MS = 1400;
int16_t shootingStar(uint32_t since);

// --- Colonie (LIFE) ----------------------------------------------------------------
// Conway's game of life on a wrapping grid of at most 2048 cells (64 x 32 or
// 32 x 64), four generations a second. Cells are bits: the shown grid, the
// two before it (for the births and the ember trails) and a spare one for the
// next generation or the next seed: four 256-byte grids, no allocation.
// Every 30 s, or as soon as nothing is left alive, a new seed dissolves in
// over 400 ms, the generations paused.
enum class Cell : uint8_t { EMPTY, ALIVE, BORN, TRAIL, OLD_TRAIL };

class Colony {
 public:
  static const uint16_t MAX_CELLS = 2048;
  static const uint32_t GENERATION_MS = 250, RESEED_MS = 30000, DISSOLVE_MS = 400;
  static const uint8_t SEED_PERCENT = 28;

  // A w x h grid (w * h <= MAX_CELLS), seeded from seed
  void begin(uint8_t w, uint8_t h, uint32_t seed, uint32_t now);
  // To now: at most one generation per call, so a late frame never pays for
  // the ones it missed; the reseed when it is due
  void update(uint32_t now);
  Cell cell(uint8_t x, uint8_t y) const;

  uint32_t generation() const { return gen_; }
  bool dissolving() const { return dissolving_; }
  uint16_t alive() const;   // cells alive in the shown grid
  // For the tests: every grid emptied, then cells set by hand in the shown one
  void clear();
  void set(uint8_t x, uint8_t y, bool on);

 private:
  bool bit(uint8_t g, uint16_t i) const { return grid_[g][i >> 3] >> (i & 7) & 1; }
  void put(uint8_t g, uint16_t i, bool on);
  void seedInto(uint8_t g);
  void step();
  uint32_t nextRandom();

  uint8_t grid_[4][MAX_CELLS / 8];
  uint8_t cur_ = 0, prev_ = 1, old_ = 2, spare_ = 3;
  uint8_t w_ = 0, h_ = 0;
  uint32_t rng_ = 1, gen_ = 0;
  uint32_t nextGenAt_ = 0, epochAt_ = 0, dissolveAt_ = 0;
  uint32_t now_ = 0;
  bool dissolving_ = false;
};
