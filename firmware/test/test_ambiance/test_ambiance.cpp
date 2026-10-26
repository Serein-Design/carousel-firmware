// SPDX-License-Identifier: AGPL-3.0-only
// The ambiances' parts that need no panel: the Serein ramp and its 70 % cap,
// the star sky's timing, and the colony (LIFE): its rules on a wrapping grid,
// what each cell shows, four generations a second, the reseed every 30 s by
// a dissolve, and a cost per frame that stays bounded.
//
//   pio test -e native -f test_ambiance
#include <unity.h>

#include <cmath>
#include <string>
#include <type_traits>

#include "../../src/ambiance_logic.cpp"

void setUp() {}
void tearDown() {}

// --- The ramp ---------------------------------------------------------------------

static uint16_t rgb565(int r, int g, int b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// The design's ramp, mixed straight between its stops and scaled to 70 %
static void test_ramp_is_the_stops_at_70_percent() {
  static const double STOPS[4][3] = {{0, 0, 0}, {120, 22, 30}, {232, 137, 58}, {232, 201, 160}};
  for (int i = 0; i < RAMP_SIZE; i++) {
    const double v = i / 31.0 * 3;
    const int k = v >= 2 ? 2 : (int)v;
    const double t = v - k;
    int c[3];
    for (int j = 0; j < 3; j++)
      c[j] = (int)std::lround((STOPS[k][j] + (STOPS[k + 1][j] - STOPS[k][j]) * t) * 0.7);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(rgb565(c[0], c[1], c[2]), AMBIANCE_RAMP[i], std::to_string(i).c_str());
  }
  TEST_ASSERT_EQUAL_HEX16(0, ramp(0));
  TEST_ASSERT_EQUAL_HEX16(AMBIANCE_RAMP[RAMP_TOP], ramp(200));   // past the end: the top
  TEST_ASSERT_EQUAL_HEX16(AMBIANCE_RAMP[RAMP_TOP], rampLevel(255));
  TEST_ASSERT_EQUAL_HEX16(AMBIANCE_RAMP[0], rampLevel(7));
}

static void test_ramp_never_passes_70_percent_and_rises() {
  int last = -1;
  for (int i = 0; i < RAMP_SIZE; i++) {
    const uint16_t c = AMBIANCE_RAMP[i];
    const int r = c >> 11, g = (c >> 5) & 63, b = c & 31;
    TEST_ASSERT_LESS_OR_EQUAL(31 * 7 / 10, r);
    TEST_ASSERT_LESS_OR_EQUAL(63 * 7 / 10, g);
    TEST_ASSERT_LESS_OR_EQUAL(31 * 7 / 10, b);
    const int light = r * 2 + g + b * 2;   // each channel on 6 bits
    TEST_ASSERT_GREATER_OR_EQUAL(last, light);
    last = light;
  }
}

static void test_named_entries() {
  const uint16_t orange = rgb565(162, 96, 41);   // (232,137,58) at 70 %
  TEST_ASSERT_EQUAL_HEX16(orange, ramp(RAMP_ORANGE));
  TEST_ASSERT_EQUAL_HEX16(rgb565(162, 141, 112), ramp(RAMP_TOP));
  const uint16_t ember = ramp(RAMP_EMBER);
  TEST_ASSERT_TRUE((ember >> 11) > (ember & 31));   // red over blue: bordeaux, not grey
}

// --- Étoiles ------------------------------------------------------------------------

static void test_stars_twinkle_on_1_to_3_s() {
  TEST_ASSERT_EQUAL_UINT16(1000, starPeriodMs(0));
  TEST_ASSERT_EQUAL_UINT16(3000, starPeriodMs(255));
  for (int d = 0; d < 256; d++) {
    TEST_ASSERT_GREATER_OR_EQUAL(1000, starPeriodMs((uint8_t)d));
    TEST_ASSERT_LESS_OR_EQUAL(3000, starPeriodMs((uint8_t)d));
  }
}

static void test_one_shooting_star_every_20_s() {
  TEST_ASSERT_EQUAL_INT16(-1, shootingStar(0));
  TEST_ASSERT_EQUAL_INT16(-1, shootingStar(18599));
  TEST_ASSERT_EQUAL_INT16(0, shootingStar(18600));
  TEST_ASSERT_GREATER_OR_EQUAL(1000, shootingStar(19999));
  TEST_ASSERT_LESS_OR_EQUAL(1023, shootingStar(19999));
  TEST_ASSERT_EQUAL_INT16(-1, shootingStar(20000));
  TEST_ASSERT_EQUAL_INT16(0, shootingStar(38600));
  int starts = 0, last = -1;
  for (uint32_t t = 0; t < 100000; t += 10) {
    const int p = shootingStar(t);
    if (p >= 0 && last < 0) starts++;
    if (p >= 0 && last >= 0) TEST_ASSERT_GREATER_OR_EQUAL(last, p);   // it only moves on
    last = p;
  }
  TEST_ASSERT_EQUAL(5, starts);
}

// --- Colonie: the rules ---------------------------------------------------------------

static Colony colony;   // 1 KB: not on the test's stack

static void startEmpty(uint8_t w = 64, uint8_t h = 32) {
  colony.begin(w, h, 1, 0);
  colony.clear();
}

static bool on(uint8_t x, uint8_t y) {
  const Cell c = colony.cell(x, y);
  return c == Cell::ALIVE || c == Cell::BORN;
}

// The next generation, due 250 ms after the last one
static uint32_t clock_ = 0;
static void generation() {
  clock_ += Colony::GENERATION_MS;
  const uint32_t before = colony.generation();
  colony.update(clock_);
  TEST_ASSERT_EQUAL_UINT32(before + 1, colony.generation());
}

static void test_blinker_turns_and_block_stays() {
  startEmpty();
  clock_ = 0;
  colony.set(10, 10, true); colony.set(11, 10, true); colony.set(12, 10, true);    // blinker
  colony.set(40, 20, true); colony.set(41, 20, true);
  colony.set(40, 21, true); colony.set(41, 21, true);                              // block
  generation();
  TEST_ASSERT_TRUE(on(11, 9) && on(11, 10) && on(11, 11));
  TEST_ASSERT_FALSE(on(10, 10) || on(12, 10));
  TEST_ASSERT_TRUE(on(40, 20) && on(41, 20) && on(40, 21) && on(41, 21));
  generation();
  TEST_ASSERT_TRUE(on(10, 10) && on(11, 10) && on(12, 10));
  TEST_ASSERT_FALSE(on(11, 9) || on(11, 11));
  TEST_ASSERT_EQUAL_UINT16(7, colony.alive());
}

// Born with exactly three neighbours, dead with one or four
static void test_birth_survival_and_death() {
  startEmpty();
  clock_ = 0;
  colony.set(5, 5, true); colony.set(6, 5, true); colony.set(5, 6, true);   // an L: (6,6) is born
  colony.set(30, 5, true);                                                // alone: dies
  // A plus: its centre has four neighbours
  colony.set(50, 10, true); colony.set(49, 10, true); colony.set(51, 10, true);
  colony.set(50, 9, true);  colony.set(50, 11, true);
  generation();
  TEST_ASSERT_TRUE(on(6, 6));
  TEST_ASSERT_TRUE(on(5, 5) && on(6, 5) && on(5, 6));   // two neighbours each: they stay
  TEST_ASSERT_FALSE(on(30, 5));
  TEST_ASSERT_FALSE(on(50, 10));
}

// The edges wrap: a blinker across the right edge turns on the left one
static void test_edges_wrap() {
  startEmpty();
  clock_ = 0;
  colony.set(63, 0, true); colony.set(0, 0, true); colony.set(1, 0, true);
  generation();
  TEST_ASSERT_TRUE(on(0, 31) && on(0, 0) && on(0, 1));
  TEST_ASSERT_FALSE(on(63, 0) || on(1, 0));
  // Portrait: 32 x 64
  startEmpty(32, 64);
  clock_ = 0;
  colony.set(31, 63, true); colony.set(0, 63, true); colony.set(1, 63, true);
  generation();
  TEST_ASSERT_TRUE(on(0, 62) && on(0, 63) && on(0, 0));
}

// Births orange, the living beige, the gone leave a trail two generations long
static void test_what_each_cell_shows() {
  startEmpty();
  clock_ = 0;
  colony.set(10, 10, true); colony.set(11, 10, true); colony.set(12, 10, true);
  generation();
  TEST_ASSERT_TRUE(Cell::BORN == colony.cell(11, 9));
  TEST_ASSERT_TRUE(Cell::ALIVE == colony.cell(11, 10));
  TEST_ASSERT_TRUE(Cell::TRAIL == colony.cell(10, 10));
  TEST_ASSERT_TRUE(Cell::EMPTY == colony.cell(30, 30));
  generation();
  // (10,10) is born again; (11,9) just died, (11,11) too
  TEST_ASSERT_TRUE(Cell::BORN == colony.cell(10, 10));
  TEST_ASSERT_TRUE(Cell::TRAIL == colony.cell(11, 9));
  generation();
  TEST_ASSERT_TRUE(Cell::TRAIL == colony.cell(10, 10) && Cell::BORN == colony.cell(11, 9));
  startEmpty();
  clock_ = 0;
  colony.set(30, 5, true);                              // dies, leaves a trail
  colony.set(40, 20, true); colony.set(41, 20, true);
  colony.set(40, 21, true); colony.set(41, 21, true);   // keeps the colony alive
  generation();
  TEST_ASSERT_TRUE(Cell::TRAIL == colony.cell(30, 5));
  generation();
  TEST_ASSERT_TRUE(Cell::OLD_TRAIL == colony.cell(30, 5));
  generation();
  TEST_ASSERT_TRUE(Cell::EMPTY == colony.cell(30, 5));
}

// --- Colonie: pace, reseed, cost ----------------------------------------------------

static void test_four_generations_a_second() {
  colony.begin(64, 32, 7, 1000);
  for (uint32_t t = 1000; t <= 11000; t += 16) colony.update(t);
  TEST_ASSERT_UINT32_WITHIN(1, 40, colony.generation());
}

// A frame that comes late (the screen was away, the loop was busy) pays for
// one generation, never for the ones it missed
static void test_a_late_frame_steps_once() {
  colony.begin(64, 32, 7, 0);
  colony.update(5000);
  TEST_ASSERT_LESS_OR_EQUAL(1, colony.generation());
  const uint32_t g = colony.generation();
  colony.update(5016);
  TEST_ASSERT_EQUAL_UINT32(g, colony.generation());   // the pace starts again from there
  colony.update(5250);
  TEST_ASSERT_EQUAL_UINT32(g + 1, colony.generation());
}

static int shown() {
  int n = 0;
  for (uint8_t y = 0; y < 32; y++)
    for (uint8_t x = 0; x < 64; x++) n += on(x, y);
  return n;
}

// Every 30 s a new seed dissolves in over 400 ms, the generations paused
static void test_reseeds_every_30_s_by_dissolve() {
  startEmpty();
  colony.set(40, 20, true); colony.set(41, 20, true);
  colony.set(40, 21, true); colony.set(41, 21, true);   // a block: never dies out
  uint32_t t = 0;
  for (; t < Colony::RESEED_MS; t += 16) colony.update(t);
  TEST_ASSERT_FALSE(colony.dissolving());
  TEST_ASSERT_EQUAL(4, shown());
  colony.update(Colony::RESEED_MS);
  TEST_ASSERT_TRUE(colony.dissolving());
  const uint32_t g = colony.generation();
  colony.update(Colony::RESEED_MS + 100);
  const int early = shown();
  colony.update(Colony::RESEED_MS + 300);
  const int late = shown();
  TEST_ASSERT_EQUAL_UINT32(g, colony.generation());
  TEST_ASSERT_LESS_THAN(late, early);                   // the new seed comes in step by step
  colony.update(Colony::RESEED_MS + Colony::DISSOLVE_MS);
  TEST_ASSERT_FALSE(colony.dissolving());
  const int seeded = shown();
  TEST_ASSERT_GREATER_THAN(late, seeded);
  TEST_ASSERT_INT_WITHIN(150, 2048 * Colony::SEED_PERCENT / 100, seeded);
  TEST_ASSERT_TRUE(Cell::ALIVE == colony.cell(0, 0) || Cell::EMPTY == colony.cell(0, 0));   // no births shown yet
  // The next one 30 s after this one ended
  for (t = Colony::RESEED_MS + Colony::DISSOLVE_MS; t < 2 * Colony::RESEED_MS + Colony::DISSOLVE_MS; t += 16) {
    colony.update(t);
    if (colony.alive() == 0) break;   // died out: an early reseed, tested below
    TEST_ASSERT_FALSE(colony.dissolving());
  }
}

static void test_an_empty_colony_reseeds_at_once() {
  startEmpty();
  colony.update(1000);
  TEST_ASSERT_TRUE(colony.dissolving());
  colony.update(1000 + Colony::DISSOLVE_MS);
  TEST_ASSERT_GREATER_THAN(300, colony.alive());
}

static void test_seeds_follow_their_number() {
  static Colony other;
  colony.begin(64, 32, 42, 0);
  other.begin(64, 32, 42, 0);
  int same = 0;
  for (uint8_t y = 0; y < 32; y++)
    for (uint8_t x = 0; x < 64; x++) same += colony.cell(x, y) == other.cell(x, y);
  TEST_ASSERT_EQUAL(2048, same);
  other.begin(64, 32, 43, 0);
  same = 0;
  for (uint8_t y = 0; y < 32; y++)
    for (uint8_t x = 0; x < 64; x++) same += colony.cell(x, y) == other.cell(x, y);
  TEST_ASSERT_LESS_THAN(2048, same);
}

// Four bit grids and a few counters, copied as they are: nothing allocated
static void test_the_colony_is_fixed_size() {
  TEST_ASSERT_TRUE(std::is_trivially_copyable<Colony>::value);
  TEST_ASSERT_LESS_OR_EQUAL(4 * 256 + 64, (int)sizeof(Colony));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ramp_is_the_stops_at_70_percent);
  RUN_TEST(test_ramp_never_passes_70_percent_and_rises);
  RUN_TEST(test_named_entries);
  RUN_TEST(test_stars_twinkle_on_1_to_3_s);
  RUN_TEST(test_one_shooting_star_every_20_s);
  RUN_TEST(test_blinker_turns_and_block_stays);
  RUN_TEST(test_birth_survival_and_death);
  RUN_TEST(test_edges_wrap);
  RUN_TEST(test_what_each_cell_shows);
  RUN_TEST(test_four_generations_a_second);
  RUN_TEST(test_a_late_frame_steps_once);
  RUN_TEST(test_reseeds_every_30_s_by_dissolve);
  RUN_TEST(test_an_empty_colony_reseeds_at_once);
  RUN_TEST(test_seeds_follow_their_number);
  RUN_TEST(test_the_colony_is_fixed_size);
  return UNITY_END();
}
