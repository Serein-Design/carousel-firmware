// SPDX-License-Identifier: AGPL-3.0-only
// The colour roles: Chaud by day, Ambre during the night hours (the same
// hours that dim the panel, crossing midnight or not, none when start and
// end are equal), and the values the design picked, in RGB565.
//
//   pio test -e native -f test_palette
#include <unity.h>

#include "../../src/palette.cpp"
#include "../../src/type.h"

void setUp() {}
void tearDown() {}

static void test_day_and_night_values() {
  const Palette &day = paletteFor(false), &night = paletteFor(true);
  TEST_ASSERT_EQUAL_HEX16(0xEE54, day.text);     // #E8C9A0
  TEST_ASSERT_EQUAL_HEX16(0x9AE8, day.label);    // #9A5E46
  TEST_ASSERT_EQUAL_HEX16(0xEC47, day.accent);   // #E8893A
  TEST_ASSERT_EQUAL_HEX16(0xC987, day.alert);    // #C8323C
  TEST_ASSERT_EQUAL_HEX16(0x59E5, day.dim);      // #5A3E2C
  TEST_ASSERT_EQUAL_HEX16(0x3943, day.ghost);    // #3A281C
  TEST_ASSERT_EQUAL_HEX16(0x0000, day.off);
  TEST_ASSERT_EQUAL_HEX16(0xD3A5, night.text);   // #D0742A
  TEST_ASSERT_EQUAL_HEX16(0x5961, night.label);  // #5E2C0C
  TEST_ASSERT_EQUAL_HEX16(0xFD08, night.accent); // #FFA040
  TEST_ASSERT_EQUAL_HEX16(0x38E1, night.dim);    // #3E1C08
  TEST_ASSERT_EQUAL_HEX16(0x0000, night.off);
  // The alert, and so the box for a missing character, is the same at night
  TEST_ASSERT_EQUAL_HEX16(TYPE_MISSING_COLOR, day.alert);
  TEST_ASSERT_EQUAL_HEX16(TYPE_MISSING_COLOR, night.alert);
}

static void assertNight(bool night, int hour, int start, int end) {
  TEST_ASSERT_EQUAL_PTR(&paletteFor(night), &paletteAtHour(hour, start, end));
}

static void test_night_hours_crossing_midnight() {
  assertNight(false, 21, 22, 7);
  assertNight(true, 22, 22, 7);
  assertNight(true, 0, 22, 7);
  assertNight(true, 6, 22, 7);
  assertNight(false, 7, 22, 7);
}

static void test_night_hours_within_a_day() {
  assertNight(false, 0, 1, 5);
  assertNight(true, 1, 1, 5);
  assertNight(true, 4, 1, 5);
  assertNight(false, 5, 1, 5);
}

static void test_same_hours_mean_no_night() {
  for (int h = 0; h < 24; h++) assertNight(false, h, 3, 3);
  for (int h = 0; h < 24; h++) assertNight(false, h, 0, 0);
}

static int luma(uint16_t c) {   // 0..~252, enough to order the roles
  return ((c >> 11) * 8 * 3 + ((c >> 5) & 0x3F) * 4 * 6 + (c & 0x1F) * 8) / 10;
}

// At the lowest levels the panel keeps few shades: the quiet roles are lit
// higher there, stay in order, and the loud ones do not change
static void test_low_light_lifts_the_quiet_roles() {
  TEST_ASSERT_FALSE(lowLight(255));
  TEST_ASSERT_FALSE(lowLight(50));
  TEST_ASSERT_TRUE(lowLight(25));
  TEST_ASSERT_TRUE(lowLight(10));
  TEST_ASSERT_TRUE(lowLight(0));
  for (bool night : {false, true}) {
    const Palette &n = paletteFor(night), &l = paletteFor(night, true);
    TEST_ASSERT_TRUE(luma(l.label) > luma(n.label));
    TEST_ASSERT_TRUE(luma(l.dim) > luma(n.dim));
    TEST_ASSERT_TRUE(luma(l.ghost) > luma(n.ghost));
    TEST_ASSERT_TRUE(luma(l.text) > luma(l.label));
    TEST_ASSERT_TRUE(luma(l.label) > luma(l.dim) + 20);   // apart, not just ordered
    TEST_ASSERT_TRUE(luma(l.dim) > luma(l.ghost));
    TEST_ASSERT_EQUAL_HEX16(n.accent, l.accent);
    TEST_ASSERT_EQUAL_HEX16(n.alert, l.alert);
    TEST_ASSERT_EQUAL_HEX16(0, l.off);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_day_and_night_values);
  RUN_TEST(test_night_hours_crossing_midnight);
  RUN_TEST(test_night_hours_within_a_day);
  RUN_TEST(test_same_hours_mean_no_night);
  RUN_TEST(test_low_light_lifts_the_quiet_roles);
  return UNITY_END();
}
