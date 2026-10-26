// SPDX-License-Identifier: AGPL-3.0-only
// The mascot's screens beyond the art: RUN's counter and its crash line fit
// beside Nimbus in both languages, the greeting fits a portrait line, bubbles
// stay on the panel, and the fade the Zs and stars use.
//
//   pio test -e native -f test_mascot_logic
#include <unity.h>

#include <cstdio>
#include <cstring>

#include "../../src/fonts_data.cpp"
#include "../../src/i18n.cpp"
#include "../../src/mascot_logic.h"
#include "../../src/type.cpp"

void setUp() {}
void tearDown() {}

static const Strings &EN = STRINGS[LANG_EN];
static const Strings &FR = STRINGS[LANG_FR];

// --- RUN's counter ---------------------------------------------------------------

static void test_the_counter_is_the_score_in_digits() {
  char t[8];
  runCounterText(0, t);
  TEST_ASSERT_EQUAL_STRING("0", t);
  runCounterText(42, t);
  TEST_ASSERT_EQUAL_STRING("42", t);
  runCounterText(9999, t);
  TEST_ASSERT_EQUAL_STRING("9999", t);
}

static void test_the_counter_stops_at_four_digits() {
  char t[8];
  runCounterText(10000, t);
  TEST_ASSERT_EQUAL_STRING("9999", t);
  runCounterText(4000000000UL, t);
  TEST_ASSERT_EQUAL_STRING("9999", t);
}

static void test_every_counter_fits_beside_nimbus() {
  // The score stands on the right, Nimbus on the left (columns 2-19, hands
  // included): the widest score must stay clear of him.
  const uint32_t samples[] = {0, 7, 10, 99, 100, 999, 1000, 1234, 8888, 9999, 12345, 4000000000UL};
  for (uint32_t n : samples) {
    const RunCounter c = runCounter(n);
    char msg[64];
    snprintf(msg, sizeof(msg), "score %lu starts at %d", (unsigned long)n, c.x);
    TEST_ASSERT_TRUE_MESSAGE(c.x >= RUN_LEFT_LIMIT, msg);
    TEST_ASSERT_EQUAL_INT16(RUN_RIGHT, c.x + (int16_t)textWidth(c.text, Face::PIERRE));
    TEST_ASSERT_EQUAL_INT16(GRID_MAIN_TOP, c.y);
  }
}

static void test_the_counter_clears_the_ground_obstacles() {
  // Pierre digits are 13 rows; the obstacles start on row 22 (ground 29 - 7)
  const RunCounter c = runCounter(8888);
  TEST_ASSERT_TRUE(c.y + 13 <= 22);
}

// --- The crash line --------------------------------------------------------------

static void test_the_crash_line_fits_beside_nimbus_in_both_languages() {
  for (const Strings *t : {&EN, &FR}) {
    const CrashLine l = runCrashLine(*t);
    const int16_t bugW = textWidth(t->runBug, Face::LABEL), bestW = textWidth(t->runBest, Face::LABEL);
    char msg[96];
    snprintf(msg, sizeof(msg), "\"%s %s\" starts at %d", t->runBug, t->runBest, l.bugX);
    TEST_ASSERT_TRUE_MESSAGE(l.bugX >= RUN_LEFT_LIMIT, msg);
    TEST_ASSERT_TRUE_MESSAGE(l.bugX + bugW < l.bestX, msg);
    TEST_ASSERT_EQUAL_INT16(RUN_RIGHT, l.bestX + bestW);
    TEST_ASSERT_EQUAL_INT16(GRID_LABEL_TOP, l.y);
  }
}

// --- Bubbles ---------------------------------------------------------------------

static void test_the_greeting_fits_a_portrait_line() {
  TEST_ASSERT_TRUE(textWidth(EN.mascotHi, Face::BODY) <= GRID_PORTRAIT_LINE_W);
  TEST_ASSERT_TRUE_MESSAGE(textWidth(FR.mascotHi, Face::BODY) <= GRID_PORTRAIT_LINE_W, FR.mascotHi);
}

static void test_a_bubble_stays_on_the_panel() {
  TEST_ASSERT_EQUAL_INT16(0, calloutX(2, 20, 64));    // would start at -8
  TEST_ASSERT_EQUAL_INT16(44, calloutX(62, 20, 64));  // would end at 72
  TEST_ASSERT_EQUAL_INT16(22, calloutX(32, 20, 64));  // centred
  TEST_ASSERT_EQUAL_INT16(0, calloutX(10, 80, 64));   // wider than the panel
}

// --- The fade ----------------------------------------------------------------------

static void test_a_blend_goes_from_one_colour_to_the_other() {
  TEST_ASSERT_EQUAL_HEX16(0xE8C9, blend565(0xE8C9, 0x0000, 0));
  TEST_ASSERT_EQUAL_HEX16(0x0000, blend565(0xE8C9, 0x0000, 255));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, blend565(0xFFFF, 0xFFFF, 100));
}

static void test_a_blend_halfway_dims_each_channel_alone() {
  // pure red halved: 31 -> 15 or 16, green and blue stay 0
  const uint16_t c = blend565(0xF800, 0x0000, 128);
  TEST_ASSERT_EQUAL_UINT16(0, c & 0x07FF);
  const uint16_t r = c >> 11;
  TEST_ASSERT_TRUE(r >= 15 && r <= 16);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_the_counter_is_the_score_in_digits);
  RUN_TEST(test_the_counter_stops_at_four_digits);
  RUN_TEST(test_every_counter_fits_beside_nimbus);
  RUN_TEST(test_the_counter_clears_the_ground_obstacles);
  RUN_TEST(test_the_crash_line_fits_beside_nimbus_in_both_languages);
  RUN_TEST(test_the_greeting_fits_a_portrait_line);
  RUN_TEST(test_a_bubble_stays_on_the_panel);
  RUN_TEST(test_a_blend_goes_from_one_colour_to_the_other);
  RUN_TEST(test_a_blend_halfway_dims_each_channel_alone);
  return UNITY_END();
}
