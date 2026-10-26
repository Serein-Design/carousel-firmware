// SPDX-License-Identifier: AGPL-3.0-only
// CLOCK besides the time: which readouts a clockShow value yields, their text
// in both languages, that every one fits its line for the longest weekday and
// month, where Pierre puts them, and the cycle's turns and push.
//
//   pio test -e native -f test_clock_logic
#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "../../src/clock_logic.cpp"
#include "../../src/fonts_data.cpp"
#include "../../src/i18n.cpp"
#include "../../src/type.cpp"

void setUp() {}
void tearDown() {}

static const Strings &EN = STRINGS[LANG_EN];
static const Strings &FR = STRINGS[LANG_FR];

static void assertFits(const char *s, uint16_t width, Face f) {
  char msg[96];
  snprintf(msg, sizeof(msg), "\"%s\" is %u px", s, textWidth(s, f));
  TEST_ASSERT_TRUE_MESSAGE(textWidth(s, f) <= width, msg);
}

// --- Readouts --------------------------------------------------------------

static void test_the_default_shows_the_date_only() {
  ClockReadout r[READ_COUNT];
  TEST_ASSERT_EQUAL_UINT8(1, clockReadouts(CLOCK_SHOW_DEFAULT, true, r));
  TEST_ASSERT_EQUAL(READ_DATE, r[0]);
}

static void test_every_readout_in_order() {
  ClockReadout r[READ_COUNT];
  TEST_ASSERT_EQUAL_UINT8(3, clockReadouts(CLOCK_SHOW_ALL, true, r));
  TEST_ASSERT_EQUAL(READ_DATE, r[0]);
  TEST_ASSERT_EQUAL(READ_WEEK, r[1]);
  TEST_ASSERT_EQUAL(READ_OUTSIDE, r[2]);
}

static void test_a_hidden_readout_frees_its_turn() {
  ClockReadout r[READ_COUNT];
  TEST_ASSERT_EQUAL_UINT8(2, clockReadouts(CLOCK_SHOW_WEEK | CLOCK_SHOW_OUTSIDE, true, r));
  TEST_ASSERT_EQUAL(READ_WEEK, r[0]);
  TEST_ASSERT_EQUAL(READ_OUTSIDE, r[1]);
  TEST_ASSERT_EQUAL_UINT8(0, clockReadouts(CLOCK_SHOW_SECONDS, true, r));
  TEST_ASSERT_EQUAL_UINT8(0, clockReadouts(0, true, r));
}

static void test_no_outside_temperature_before_the_weather() {
  ClockReadout r[READ_COUNT];
  TEST_ASSERT_EQUAL_UINT8(2, clockReadouts(CLOCK_SHOW_ALL, false, r));
  TEST_ASSERT_EQUAL(READ_WEEK, r[1]);
  TEST_ASSERT_EQUAL_UINT8(0, clockReadouts(CLOCK_SHOW_OUTSIDE, false, r));
}

// --- Text --------------------------------------------------------------------

static void test_the_date_as_the_board_shows_it() {
  char b[32];
  clockDateLine(b, sizeof(b), EN, 1, 19, 9);
  TEST_ASSERT_EQUAL_STRING("MON 19 OCT", b);
  clockDateLine(b, sizeof(b), FR, 1, 19, 9);
  TEST_ASSERT_EQUAL_STRING("LUN. 19 OCT.", b);
  clockDateLine(b, sizeof(b), FR, 0, 5, 7);
  TEST_ASSERT_EQUAL_STRING("DIM. 5 AO\xC3\x9BT", b);
  clockDayMonth(b, sizeof(b), FR, 19, 9);
  TEST_ASSERT_EQUAL_STRING("19\xE2\x80\xAFOCT.", b);
}

static void test_week_and_outside_words() {
  char b[32];
  clockWeekLine(b, sizeof(b), EN, 43);
  TEST_ASSERT_EQUAL_STRING("WEEK 43", b);
  clockWeekLine(b, sizeof(b), FR, 43);
  TEST_ASSERT_EQUAL_STRING("SEMAINE 43", b);
  clockWeekLine(b, sizeof(b), FR, 7, true);
  TEST_ASSERT_EQUAL_STRING("S7", b);
  clockOutsideLine(b, sizeof(b), EN, 14.4f);
  TEST_ASSERT_EQUAL_STRING("OUTSIDE 14\xC2\xB0", b);
  clockOutsideLine(b, sizeof(b), FR, 13.6f);
  TEST_ASSERT_EQUAL_STRING("DEHORS 14\xC2\xB0", b);
  clockDegrees(b, sizeof(b), -0.4f);
  TEST_ASSERT_EQUAL_STRING("0\xC2\xB0", b);
  clockDegrees(b, sizeof(b), -12.5f);
  TEST_ASSERT_EQUAL_STRING("-13\xC2\xB0", b);
}

// Every weekday with every month, the widest day of the month, the widest
// week and the coldest and hottest readings: each fits its line
static void test_every_readout_fits_60_px() {
  for (const Strings *t : {&EN, &FR}) {
    char b[48];
    for (int d = 0; d < 7; d++)
      for (int m = 0; m < 12; m++) {
        clockDateLine(b, sizeof(b), *t, d, 30, m);
        assertFits(b, 60, Face::LABEL);
      }
    clockWeekLine(b, sizeof(b), *t, 53);
    assertFits(b, 60, Face::LABEL);
    // The coldest and hottest, in °C and °F (-40 is both; 50 °C is 122 °F)
    for (float d : {-40.0f, 48.0f, 122.0f}) {
      clockOutsideLine(b, sizeof(b), *t, d);
      assertFits(b, 60, Face::LABEL);
      // Pierre's foot holds the week and the bare temperature side by side
      char w[24], deg[12];
      clockWeekLine(w, sizeof(w), *t, 53);
      clockDegrees(deg, sizeof(deg), d);
      TEST_ASSERT_TRUE(textWidth(w, Face::LABEL) + 4 + textWidth(deg, Face::LABEL) <= 60);
    }
  }
}

// The dial's column, right of a 29 px dial: 30 px
static void test_the_dial_column_fits_30_px() {
  for (const Strings *t : {&EN, &FR}) {
    for (const char *d : t->clkDays) assertFits(d, 30, Face::LABEL);
    for (const char *m : t->clkMonths) assertFits(m, 30, Face::LABEL);
    assertFits(t->clkWeek, 30, Face::LABEL);
    assertFits(t->clkOutside, 30, Face::LABEL);
  }
  assertFits("53", 30, Face::PIERRE);
  assertFits("48\xC2\xB0", 30, Face::PIERRE);
  assertFits("-9\xC2\xB0", 30, Face::PIERRE);
  assertFits("122", 30, Face::PIERRE);   // °F: the number alone, as for -12°
}

static void test_portrait_lines_fit_28_px() {
  for (const Strings *t : {&EN, &FR}) {
    char b[32];
    for (const char *d : t->clkDays) assertFits(d, 28, Face::LABEL);
    for (int m = 0; m < 12; m++) {
      clockDayMonth(b, sizeof(b), *t, 30, m);
      assertFits(b, 28, Face::LABEL);
    }
    clockWeekLine(b, sizeof(b), *t, 53, true);
    assertFits(b, 28, Face::LABEL);
    for (float d : {-40.0f, 122.0f}) {
      clockDegrees(b, sizeof(b), d);
      assertFits(b, 28, Face::LABEL);
    }
  }
}

// --- 12 h --------------------------------------------------------------------------

// Midnight, the minute after, noon, the afternoon, the last minute of the day
static const int H12_CASES[][2] = {{0, 0}, {0, 59}, {12, 0}, {13, 5}, {23, 59}};

static void test_12_h_hours_run_1_to_12() {
  TEST_ASSERT_EQUAL_UINT8(12, hour12(0).hour);    // midnight: 12 AM
  TEST_ASSERT_FALSE(hour12(0).pm);
  TEST_ASSERT_EQUAL_UINT8(1, hour12(1).hour);
  TEST_ASSERT_EQUAL_UINT8(11, hour12(11).hour);
  TEST_ASSERT_FALSE(hour12(11).pm);
  TEST_ASSERT_EQUAL_UINT8(12, hour12(12).hour);   // noon: 12 PM
  TEST_ASSERT_TRUE(hour12(12).pm);
  TEST_ASSERT_EQUAL_UINT8(1, hour12(13).hour);
  TEST_ASSERT_EQUAL_UINT8(11, hour12(23).hour);
  TEST_ASSERT_TRUE(hour12(23).pm);
}

static void test_the_time_as_pierre_shows_it() {
  static const char *const TIMES[] = {"12:00", "12:59", "12:00", "1:05", "11:59"};
  static const char *const HOURS[] = {"12", "12", "12", "1", "11"};
  static const bool PM[] = {false, false, true, true, true};
  char b[8];
  for (int i = 0; i < 5; i++) {
    const int h = H12_CASES[i][0], m = H12_CASES[i][1];
    clockTime(b, sizeof(b), h, m, true);
    TEST_ASSERT_EQUAL_STRING(TIMES[i], b);   // no leading zero
    clockHours(b, sizeof(b), h, true);
    TEST_ASSERT_EQUAL_STRING(HOURS[i], b);
    for (const Strings *t : {&EN, &FR})
      TEST_ASSERT_EQUAL_STRING(PM[i] ? t->clkPm : t->clkAm, clockAmPm(*t, h));
  }
  TEST_ASSERT_EQUAL_STRING("AM", EN.clkAm);
  TEST_ASSERT_EQUAL_STRING("PM", EN.clkPm);
  // 24 h as before: two figures each
  clockTime(b, sizeof(b), 9, 5, false);
  TEST_ASSERT_EQUAL_STRING("09:05", b);
  clockTime(b, sizeof(b), 0, 0, false);
  TEST_ASSERT_EQUAL_STRING("00:00", b);
  clockHours(b, sizeof(b), 0, false);
  TEST_ASSERT_EQUAL_STRING("00", b);
  clockHours(b, sizeof(b), 13, false);
  TEST_ASSERT_EQUAL_STRING("13", b);
}

// Pierre and Alternance draw the time on one 60 px line, the AM/PM beside
// it; portrait puts it beside the column of hours over minutes, 28 px.
// Cadran's hands already go round twelve hours: no text to fit.
static void test_12_h_fits_every_style() {
  const uint16_t column = textWidth("00", Face::PIERRE);   // tabular: any two figures
  for (const Strings *t : {&EN, &FR})
    for (const auto &c : H12_CASES) {
      char d[8], msg[48];
      clockTime(d, sizeof(d), c[0], c[1], true);
      const char *ampm = clockAmPm(*t, c[0]);
      const uint16_t aw = textWidth(ampm, Face::LABEL);
      TimePlace l = placeTime(textWidth(d, Face::PIERRE), ampm, 32, false);
      snprintf(msg, sizeof(msg), "%s %s: %d px", d, ampm, l.width);
      TEST_ASSERT_TRUE_MESSAGE(l.width <= 60, msg);
      TEST_ASSERT_TRUE_MESSAGE(l.timeX >= 2 && l.ampmX + aw <= 62, msg);
      TEST_ASSERT_TRUE_MESSAGE(l.ampmX > l.timeX + textWidth(d, Face::PIERRE), msg);
      l = placeTime(column, ampm, 16, true);
      snprintf(msg, sizeof(msg), "portrait %s: %d px", ampm, l.width);
      TEST_ASSERT_TRUE_MESSAGE(l.width <= 28, msg);
      TEST_ASSERT_TRUE_MESSAGE(l.timeX >= 2 && l.ampmX + aw <= 30, msg);
      TEST_ASSERT_TRUE_MESSAGE(l.ampmX > l.timeX + column, msg);
    }
}

// 24 h: the time alone, centred where it always was
static void test_24_h_places_the_time_as_before() {
  const uint16_t w = textWidth("00:00", Face::PIERRE);
  const TimePlace l = placeTime(w, nullptr, 32, false);
  TEST_ASSERT_EQUAL_INT16(32 - (w >> 1), l.timeX);
  TEST_ASSERT_EQUAL_INT16(w, l.width);
}

// WEATHER's hour columns (14 px): "15H" in 24 h; in 12 h the hour and the
// AM/PM's letter, as "12PM" is 17 px
static void test_the_forecast_hours() {
  char b[8];
  clockHourLabel(b, sizeof(b), EN, 15, false);
  TEST_ASSERT_EQUAL_STRING("15H", b);
  clockHourLabel(b, sizeof(b), EN, 15, true);
  TEST_ASSERT_EQUAL_STRING("3P", b);
  clockHourLabel(b, sizeof(b), EN, 0, true);
  TEST_ASSERT_EQUAL_STRING("12A", b);
  clockHourLabel(b, sizeof(b), FR, 12, true);
  TEST_ASSERT_EQUAL_STRING("12P", b);
  for (const Strings *t : {&EN, &FR})
    for (int h = 0; h < 24; h++)
      for (bool h12 : {false, true}) {
        clockHourLabel(b, sizeof(b), *t, h, h12);
        assertFits(b, 14, Face::LABEL);
      }
}

// --- Pierre's slots ------------------------------------------------------------

static void test_stone_default_layout() {
  const StoneLayout l = stoneLayout(CLOCK_SHOW_DEFAULT, true);
  TEST_ASSERT_TRUE(l.dateOnLabel);
  TEST_ASSERT_FALSE(l.extrasOnLabel);
  TEST_ASSERT_FALSE(l.extrasOnFoot);
  TEST_ASSERT_EQUAL_INT8(25, l.secondsRow);
}

static void test_stone_extras_take_the_foot_and_lift_the_seconds() {
  const StoneLayout l = stoneLayout(CLOCK_SHOW_ALL, true);
  TEST_ASSERT_TRUE(l.dateOnLabel);
  TEST_ASSERT_TRUE(l.extrasOnFoot);
  TEST_ASSERT_EQUAL_INT8(23, l.secondsRow);
}

static void test_stone_hidden_date_frees_the_label_line() {
  const StoneLayout l = stoneLayout(CLOCK_SHOW_WEEK | CLOCK_SHOW_SECONDS, true);
  TEST_ASSERT_FALSE(l.dateOnLabel);
  TEST_ASSERT_TRUE(l.extrasOnLabel);
  TEST_ASSERT_FALSE(l.extrasOnFoot);
  TEST_ASSERT_EQUAL_INT8(25, l.secondsRow);
}

static void test_stone_without_seconds_or_weather() {
  const StoneLayout l = stoneLayout(CLOCK_SHOW_DATE | CLOCK_SHOW_OUTSIDE, false);
  TEST_ASSERT_TRUE(l.dateOnLabel);
  TEST_ASSERT_FALSE(l.extrasOnFoot);   // no weather yet: nothing for the foot
  TEST_ASSERT_EQUAL_INT8(-1, l.secondsRow);
}

// --- The cycle -------------------------------------------------------------------

static void test_the_cycle_takes_turns_every_4_s() {
  CycleStep s = clockCycle(1000, 3, 9);
  TEST_ASSERT_EQUAL_UINT8(0, s.now);
  TEST_ASSERT_EQUAL_UINT8(0, s.shift);
  s = clockCycle(4000 + 1000, 3, 9);
  TEST_ASSERT_EQUAL_UINT8(1, s.now);
  TEST_ASSERT_EQUAL_UINT8(0, s.before);
  s = clockCycle(3 * 4000 + 1000, 3, 9);   // back to the first
  TEST_ASSERT_EQUAL_UINT8(0, s.now);
  TEST_ASSERT_EQUAL_UINT8(2, s.before);
}

static void test_the_push_lasts_300_ms_and_eases_out() {
  TEST_ASSERT_EQUAL_UINT8(9, clockCycle(4000, 2, 9).shift);    // just started
  const uint8_t early = clockCycle(4030, 2, 9).shift;
  const uint8_t late = clockCycle(4200, 2, 9).shift;
  TEST_ASSERT_TRUE(early < 9 && late <= early);
  TEST_ASSERT_TRUE(9 - early > early - late || early == late);  // fast first, then slow
  TEST_ASSERT_EQUAL_UINT8(0, clockCycle(4300, 2, 9).shift);    // settled
}

static void test_one_readout_never_moves() {
  for (uint32_t ms : {0u, 4000u, 4100u, 8000u}) {
    const CycleStep s = clockCycle(ms, 1, 9);
    TEST_ASSERT_EQUAL_UINT8(0, s.now);
    TEST_ASSERT_EQUAL_UINT8(0, s.shift);
  }
  TEST_ASSERT_EQUAL_UINT8(0, clockCycle(4100, 0, 9).shift);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_the_default_shows_the_date_only);
  RUN_TEST(test_every_readout_in_order);
  RUN_TEST(test_a_hidden_readout_frees_its_turn);
  RUN_TEST(test_no_outside_temperature_before_the_weather);
  RUN_TEST(test_the_date_as_the_board_shows_it);
  RUN_TEST(test_week_and_outside_words);
  RUN_TEST(test_every_readout_fits_60_px);
  RUN_TEST(test_the_dial_column_fits_30_px);
  RUN_TEST(test_portrait_lines_fit_28_px);
  RUN_TEST(test_12_h_hours_run_1_to_12);
  RUN_TEST(test_the_time_as_pierre_shows_it);
  RUN_TEST(test_12_h_fits_every_style);
  RUN_TEST(test_24_h_places_the_time_as_before);
  RUN_TEST(test_the_forecast_hours);
  RUN_TEST(test_stone_default_layout);
  RUN_TEST(test_stone_extras_take_the_foot_and_lift_the_seconds);
  RUN_TEST(test_stone_hidden_date_frees_the_label_line);
  RUN_TEST(test_stone_without_seconds_or_weather);
  RUN_TEST(test_the_cycle_takes_turns_every_4_s);
  RUN_TEST(test_the_push_lasts_300_ms_and_eases_out);
  RUN_TEST(test_one_readout_never_moves);
  return UNITY_END();
}
