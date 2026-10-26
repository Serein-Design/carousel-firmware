// SPDX-License-Identifier: AGPL-3.0-only
#include "ambiance_logic.h"

#include <string.h>

// i / 31 along the stops, each channel rounded, then RGB565 like palette.cpp
// (test_ambiance recomputes it from the stops)
const uint16_t AMBIANCE_RAMP[RAMP_SIZE] = {
  0x0000, 0x0800, 0x1000, 0x1820, 0x2021, 0x2821, 0x3041, 0x3841,
  0x4062, 0x4862, 0x5062, 0x58A2, 0x60E3, 0x6923, 0x7163, 0x71A3,
  0x79E4, 0x8204, 0x8A44, 0x9284, 0x9AC4, 0xA305, 0xA326, 0xA347,
  0xA368, 0xA388, 0xA3A9, 0xA3CA, 0xA40B, 0xA42C, 0xA44D, 0xA46E,
};

uint16_t starPeriodMs(uint8_t draw) { return (uint16_t)(1000 + draw * 2000u / 255); }

int16_t shootingStar(uint32_t since) {
  const uint32_t in = since % SHOOTING_EVERY_MS, from = SHOOTING_EVERY_MS - SHOOTING_FOR_MS;
  if (in < from) return -1;
  return (int16_t)((in - from) * 1024 / SHOOTING_FOR_MS);
}

// --- Colonie ---------------------------------------------------------------------------

// Which cells the new seed has taken at a point of the dissolve: each cell
// its own threshold, scattered, so it comes in as grain rather than a wipe
static uint16_t dissolveRank(uint16_t i) { return (uint16_t)((i * 2654435761u) >> 22); }   // 0..1023

uint32_t Colony::nextRandom() {   // xorshift32: the same seed, the same colony
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return rng_;
}

void Colony::put(uint8_t g, uint16_t i, bool on) {
  if (on) grid_[g][i >> 3] |= (uint8_t)(1 << (i & 7));
  else grid_[g][i >> 3] &= (uint8_t)~(1 << (i & 7));
}

void Colony::seedInto(uint8_t g) {
  memset(grid_[g], 0, sizeof(grid_[g]));
  for (uint16_t i = 0; i < w_ * h_; i++)
    if (nextRandom() % 100 < SEED_PERCENT) put(g, i, true);
}

void Colony::begin(uint8_t w, uint8_t h, uint32_t seed, uint32_t now) {
  w_ = w ? w : 1;
  h_ = (uint16_t)w_ * h <= MAX_CELLS ? h : (uint8_t)(MAX_CELLS / w_);
  rng_ = seed ? seed : 1;   // xorshift never leaves 0
  cur_ = 0; prev_ = 1; old_ = 2; spare_ = 3;
  seedInto(cur_);
  memcpy(grid_[prev_], grid_[cur_], sizeof(grid_[0]));
  memcpy(grid_[old_], grid_[cur_], sizeof(grid_[0]));
  gen_ = 0;
  now_ = epochAt_ = now;
  nextGenAt_ = now + GENERATION_MS;
  dissolving_ = false;
}

void Colony::clear() {
  memset(grid_, 0, sizeof(grid_));
}

void Colony::set(uint8_t x, uint8_t y, bool on) {
  if (x < w_ && y < h_) put(cur_, (uint16_t)(y * w_ + x), on);
}

uint16_t Colony::alive() const {
  uint16_t n = 0;
  for (uint16_t b = 0; b < (w_ * h_ + 7) / 8; b++) n += (uint16_t)__builtin_popcount(grid_[cur_][b]);
  return n;
}

// Born with three neighbours, alive on with two or three; the edges wrap.
// The new generation goes into the oldest grid, which the trails no longer need.
void Colony::step() {
  const uint8_t next = old_;
  memset(grid_[next], 0, sizeof(grid_[next]));
  for (uint8_t y = 0; y < h_; y++) {
    const uint8_t up = y ? y - 1 : h_ - 1, down = y + 1 < h_ ? y + 1 : 0;
    for (uint8_t x = 0; x < w_; x++) {
      const uint8_t left = x ? x - 1 : w_ - 1, right = x + 1 < w_ ? x + 1 : 0;
      const uint8_t rows[3] = {up, y, down}, cols[3] = {left, x, right};
      uint8_t n = 0;
      for (uint8_t r = 0; r < 3; r++)
        for (uint8_t c = 0; c < 3; c++)
          if (r != 1 || c != 1) n += bit(cur_, (uint16_t)(rows[r] * w_ + cols[c]));
      const bool live = bit(cur_, (uint16_t)(y * w_ + x));
      if (n == 3 || (live && n == 2)) put(next, (uint16_t)(y * w_ + x), true);
    }
  }
  old_ = prev_;
  prev_ = cur_;
  cur_ = next;
  gen_++;
}

void Colony::update(uint32_t now) {
  now_ = now;
  if (dissolving_) {
    if (now - dissolveAt_ < DISSOLVE_MS) return;
    // The seed takes over whole, with no births or trails on its first frame
    memcpy(grid_[cur_], grid_[spare_], sizeof(grid_[0]));
    memcpy(grid_[prev_], grid_[spare_], sizeof(grid_[0]));
    memcpy(grid_[old_], grid_[spare_], sizeof(grid_[0]));
    dissolving_ = false;
    epochAt_ = now;
    nextGenAt_ = now + GENERATION_MS;
    return;
  }
  if (now - epochAt_ >= RESEED_MS || !alive()) {
    seedInto(spare_);
    dissolving_ = true;
    dissolveAt_ = now;
    return;
  }
  if ((int32_t)(now - nextGenAt_) < 0) return;
  step();
  nextGenAt_ += GENERATION_MS;
  if ((int32_t)(now - nextGenAt_) >= 0) nextGenAt_ = now + GENERATION_MS;   // late: no catching up
}

Cell Colony::cell(uint8_t x, uint8_t y) const {
  if (x >= w_ || y >= h_) return Cell::EMPTY;
  const uint16_t i = (uint16_t)(y * w_ + x);
  if (dissolving_ && dissolveRank(i) < (now_ - dissolveAt_) * 1024 / DISSOLVE_MS)
    return bit(spare_, i) ? Cell::ALIVE : Cell::EMPTY;
  const bool live = bit(cur_, i), was = bit(prev_, i);
  if (live) return was ? Cell::ALIVE : Cell::BORN;
  if (was) return Cell::TRAIL;
  return bit(old_, i) ? Cell::OLD_TRAIL : Cell::EMPTY;
}
