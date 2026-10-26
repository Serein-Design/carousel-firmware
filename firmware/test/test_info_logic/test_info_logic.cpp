// SPDX-License-Identifier: AGPL-3.0-only
// DATE, SYSTEM, MONEY and SOCIAL: what they say in both languages, the face
// a number takes, and that every line fits its row in landscape (60 px) and
// portrait (28 px) for the longest values: the widest weekday and month, the
// widest week and day of the year, the longest uptime, the largest amounts
// and counts.
//
//   pio test -e native -f test_info_logic
#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "../../src/clock_logic.cpp"
#include "../../src/fonts_data.cpp"
#include "../../src/i18n.cpp"
#include "../../src/info_logic.cpp"
#include "../../src/type.cpp"

void setUp() {}
void tearDown() {}

static const Strings &EN = STRINGS[LANG_EN];
static const Strings &FR = STRINGS[LANG_FR];
static const uint16_t LINE = 60, TALL = 28;   // grid.h: a system line

static uint16_t w(const char *s, Face f) { return textWidth(s, f); }

static void assertFits(const char *s, uint16_t width, Face f) {
  char msg[96];
  snprintf(msg, sizeof(msg), "\"%s\" is %u px, room for %u", s, w(s, f), width);
  TEST_ASSERT_TRUE_MESSAGE(s && s[0], msg);
  TEST_ASSERT_TRUE_MESSAGE(w(s, f) <= width, msg);
}

static void assertSum(uint16_t sum, uint16_t width, const char *what) {
  char msg[96];
  snprintf(msg, sizeof(msg), "%s: %u px, room for %u", what, sum, width);
  TEST_ASSERT_TRUE_MESSAGE(sum <= width, msg);
}

// Nothing a line draws is a box: every character is in its face (or
// composed from one), as the missing-glyph box is the alert red
static void assertDrawable(const char *s, Face f) {
  GFXcanvas16 c(128, 16);
  drawText(c, s, 0, 2, f, 0xFFFF);
  char msg[64];
  snprintf(msg, sizeof(msg), "\"%s\" has a character its face lacks", s);
  for (int16_t y = 0; y < 16; y++)
    for (int16_t x = 0; x < 128; x++)
      TEST_ASSERT_NOT_EQUAL_MESSAGE(TYPE_MISSING_COLOR, c.getPixel(x, y), msg);
}

// --- Numbers ----------------------------------------------------------------------

static void test_digits_group_by_threes_with_a_narrow_space() {
  char b[24];
  groupDigits(0, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("0", b);
  groupDigits(999, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("999", b);
  groupDigits(12345, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("12\xE2\x80\xAF" "345", b);
  groupDigits(4294967295u, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("4\xE2\x80\xAF" "294\xE2\x80\xAF" "967\xE2\x80\xAF" "295", b);
  // Too small a buffer: cut, still a string
  char small[4];
  groupDigits(12345, small, sizeof(small));
  TEST_ASSERT_TRUE(strlen(small) < sizeof(small));
}

static void test_a_number_takes_pierre_while_it_fits() {
  char b[24];
  groupDigits(88888, b, sizeof(b));
  TEST_ASSERT_EQUAL(Face::PIERRE, numberFace(b, LINE));
  groupDigits(888888, b, sizeof(b));
  TEST_ASSERT_EQUAL(Face::BODY, numberFace(b, LINE));
  TEST_ASSERT_EQUAL(Face::PIERRE, numberFace("888", TALL));
  TEST_ASSERT_EQUAL(Face::BODY, numberFace("8888", TALL));
  groupDigits(888888, b, sizeof(b));
  TEST_ASSERT_EQUAL(Face::LABEL, numberFace(b, TALL));
}

static void test_a_face_centres_in_the_main_block() {
  TEST_ASSERT_EQUAL_INT16(9, faceTop(Face::PIERRE, 9, 21));
  TEST_ASSERT_EQUAL_INT16(12, faceTop(Face::BODY, 9, 21));
  TEST_ASSERT_EQUAL_INT16(13, faceTop(Face::LABEL, 9, 21));
}

// --- DATE -------------------------------------------------------------------------

static void test_date_words() {
  char b[24];
  dateDayLine(b, sizeof(b), EN, 292, false);
  TEST_ASSERT_EQUAL_STRING("DAY 292", b);
  dateDayLine(b, sizeof(b), FR, 292, false);
  TEST_ASSERT_EQUAL_STRING("JOUR 292", b);
  snprintf(b, sizeof(b), FR.dateWeek, 43u);
  TEST_ASSERT_EQUAL_STRING("SEM. 43", b);
  dateDayLine(b, sizeof(b), EN, 5, true);
  TEST_ASSERT_EQUAL_STRING("D5", b);
  dateDayLine(b, sizeof(b), FR, 366, true);
  TEST_ASSERT_EQUAL_STRING("J366", b);
  TEST_ASSERT_EQUAL_STRING("WEDNESDAY", EN.days[3]);
  TEST_ASSERT_EQUAL_STRING("MERCREDI", FR.days[3]);
  TEST_ASSERT_EQUAL_STRING("OCTOBER", EN.months[9]);
  TEST_ASSERT_EQUAL_STRING("F\xC3\x89VRIER", FR.months[1]);
}

// Landscape: the weekday on the label line; the day in Pierre, the month
// over the year beside it ("OCT 2026" was cut before); the week and the day
// of the year on the foot, apart
static void test_date_landscape_fits() {
  for (const Strings *t : {&EN, &FR}) {
    for (const char *d : t->days) assertFits(d, LINE, Face::LABEL);
    for (const char *m : t->months) {
      assertDrawable(m, Face::LABEL);
      assertSum(w("30", Face::PIERRE) + INFO_GAP + w(m, Face::LABEL), LINE, m);
    }
    assertSum(w("30", Face::PIERRE) + INFO_GAP + w("2088", Face::LABEL), LINE, "year");
    char week[24], day[24];
    snprintf(week, sizeof(week), t->dateWeek, 53u);
    dateDayLine(day, sizeof(day), *t, 366, false);
    assertSum(w(week, Face::LABEL) + INFO_GAP + w(day, Face::LABEL), LINE, day);
  }
}

static void test_date_portrait_fits() {
  for (const Strings *t : {&EN, &FR}) {
    for (const char *d : t->clkDays) assertFits(d, TALL, Face::LABEL);
    for (const char *m : t->clkMonths) assertFits(m, TALL, Face::LABEL);
    assertFits("30", TALL, Face::PIERRE);
    assertFits("2088", TALL, Face::LABEL);
    char b[24];
    clockWeekLine(b, sizeof(b), *t, 53, true);
    assertFits(b, TALL, Face::LABEL);
    dateDayLine(b, sizeof(b), *t, 366, true);
    assertFits(b, TALL, Face::LABEL);
  }
}

// --- SYSTEM -----------------------------------------------------------------------

static void test_signal_bars_follow_the_rssi() {
  TEST_ASSERT_EQUAL_UINT8(0, signalBars(false, -40));
  TEST_ASSERT_EQUAL_UINT8(4, signalBars(true, -40));
  TEST_ASSERT_EQUAL_UINT8(3, signalBars(true, -60));
  TEST_ASSERT_EQUAL_UINT8(2, signalBars(true, -70));
  TEST_ASSERT_EQUAL_UINT8(1, signalBars(true, -90));
}

static void test_uptime_in_minutes_hours_then_days() {
  char b[24];
  uptimeText(b, sizeof(b), EN, 42 * 60 + 59);
  TEST_ASSERT_EQUAL_STRING("42 MIN", b);
  uptimeText(b, sizeof(b), EN, 3 * 3600 + 5 * 60);
  TEST_ASSERT_EQUAL_STRING("3H05", b);
  uptimeText(b, sizeof(b), EN, 12 * 86400 + 4 * 3600);
  TEST_ASSERT_EQUAL_STRING("12D 04H", b);
  uptimeText(b, sizeof(b), FR, 12 * 86400 + 4 * 3600);
  TEST_ASSERT_EQUAL_STRING("12J 04H", b);
  uptimeText(b, sizeof(b), FR, 123 * 86400 + 4 * 3600);
  TEST_ASSERT_EQUAL_STRING("123J", b);
}

static void test_signal_shows_down_to_minus_99() {
  TEST_ASSERT_EQUAL(-62, signalDbm(-62));
  TEST_ASSERT_EQUAL(-99, signalDbm(-127));
}

static void test_ram_share() {
  TEST_ASSERT_EQUAL_UINT8(50, ramPercent(160 * 1024, 320 * 1024));
  // The firmware itself takes some: never all of it free, so never 3 digits
  TEST_ASSERT_EQUAL_UINT8(99, ramPercent(320, 320));
  TEST_ASSERT_EQUAL_UINT8(0, ramPercent(10, 0));
}

// The longest uptime: 99 days and 23 hours, or 4 digits of days alone
static const uint32_t LONG_UPTIMES[] = {59 * 60 + 59, 23 * 3600 + 59 * 60,
                                        99 * 86400 + 23 * 3600, 4294967u};

// Landscape: free RAM and the frame rate on the label line; the free
// kilobytes in Pierre, the unit over the share, the bars over the dBm; the
// uptime on the foot (the address is the reach card's). The ESP32's internal heap is
// under 520 KB: three digits; the share stays under 100 (ramPercent) and the
// signal shows down to -99 dBm (signalDbm).
static void test_system_landscape_fits() {
  for (const Strings *t : {&EN, &FR}) {
    char fps[16], up[32], dur[24];
    snprintf(fps, sizeof(fps), t->sysFps, 99u);
    assertSum(w(t->sysRam, Face::LABEL) + INFO_GAP + w(fps, Face::LABEL), LINE, fps);
    const uint16_t unit = w(t->sysKb, Face::LABEL) > w("99%", Face::LABEL)
                              ? w(t->sysKb, Face::LABEL) : w("99%", Face::LABEL);
    const uint16_t signal = w("-99", Face::LABEL) > SIGNAL_BARS_W ? w("-99", Face::LABEL)
                                                                  : SIGNAL_BARS_W;
    assertSum(w("888", Face::PIERRE) + INFO_GAP + unit + INFO_GAP + signal, LINE, "main");
    for (uint32_t s : LONG_UPTIMES) {
      uptimeText(dur, sizeof(dur), *t, s);
      snprintf(up, sizeof(up), t->sysUp, dur);
      assertFits(up, LINE, Face::LABEL);
    }
  }
}

static void test_system_portrait_fits() {
  for (const Strings *t : {&EN, &FR}) {
    char b[32], dur[24];
    assertFits(t->sysRamTall, TALL, Face::LABEL);
    assertFits("888", TALL, Face::PIERRE);
    snprintf(b, sizeof(b), "%s 99%%", t->sysKb);
    assertFits(b, TALL, Face::LABEL);
    assertFits(t->sysUpTall, TALL, Face::LABEL);
    for (uint32_t s : LONG_UPTIMES) {
      uptimeText(dur, sizeof(dur), *t, s);
      assertFits(dur, TALL, Face::LABEL);
    }
    assertSum(SIGNAL_BARS_W + INFO_GAP + w("-99", Face::LABEL), TALL, "signal");
    snprintf(b, sizeof(b), t->sysFps, 99u);
    assertFits(b, TALL, Face::LABEL);
  }
}

// --- MONEY ------------------------------------------------------------------------

static const MoneyTerms TERMS = {2100.0f, 21, 9, 17};   // 100 a day, 12.50 an hour

static void test_money_during_the_day() {
  const MoneyDay d = moneyDay(TERMS, 13 * 3600, 15);
  TEST_ASSERT_TRUE(d.working);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, d.today);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, d.progress);
  // 15 * 21 / 30 = 10 days gone, and today's half
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1050.0f, d.month);
}

static void test_money_off_hours() {
  MoneyDay d = moneyDay(TERMS, 8 * 3600, 1);
  TEST_ASSERT_FALSE(d.working);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, d.today);
  d = moneyDay(TERMS, 20 * 3600, 31);
  TEST_ASSERT_FALSE(d.working);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, d.today);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, d.progress);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2200.0f, d.month);   // all 21 days, and today
}

static void test_money_never_divides_by_zero() {
  const MoneyDay d = moneyDay({2000.0f, 0, 9, 9}, 12 * 3600, 10);
  TEST_ASSERT_TRUE(d.today == d.today);   // not NaN
  TEST_ASSERT_TRUE(d.month == d.month);
}

static void test_money_amounts_in_each_language() {
  MoneyText m;
  char e[32];
  moneyAmount(m, EN, CURRENCY_EUR, 123.456f);
  TEST_ASSERT_EQUAL_STRING("", m.before);
  TEST_ASSERT_EQUAL_STRING("123", m.units);
  TEST_ASSERT_EQUAL_STRING(".45 \xE2\x82\xAC", m.after);
  moneyAmount(m, FR, CURRENCY_EUR, 12345.999f);
  TEST_ASSERT_EQUAL_STRING("12\xE2\x80\xAF" "345", m.units);
  TEST_ASSERT_EQUAL_STRING(",99\xE2\x80\xAF\xE2\x82\xAC", m.after);
  moneyTotal(e, sizeof(e), FR, CURRENCY_EUR, 2345.6f);
  TEST_ASSERT_EQUAL_STRING("2\xE2\x80\xAF" "345\xE2\x80\xAF\xE2\x82\xAC", e);
  moneyTotal(e, sizeof(e), EN, CURRENCY_EUR, 2345.6f);
  TEST_ASSERT_EQUAL_STRING("2\xE2\x80\xAF" "345 \xE2\x82\xAC", e);
}

// $ and £ before the amount, tight; € and CHF after it, spaced as the
// language spaces a unit; the decimal mark is the language's
static void test_each_currency_where_it_writes_its_symbol() {
  MoneyText m;
  char e[32];
  moneyAmount(m, EN, CURRENCY_USD, 123.456f);
  TEST_ASSERT_EQUAL_STRING("$", m.before);
  TEST_ASSERT_EQUAL_STRING("123", m.units);
  TEST_ASSERT_EQUAL_STRING(".45", m.after);
  moneyAmount(m, FR, CURRENCY_GBP, 123.456f);
  TEST_ASSERT_EQUAL_STRING("\xC2\xA3", m.before);
  TEST_ASSERT_EQUAL_STRING(",45", m.after);
  moneyAmount(m, FR, CURRENCY_CHF, 123.456f);
  TEST_ASSERT_EQUAL_STRING("", m.before);
  TEST_ASSERT_EQUAL_STRING(",45\xE2\x80\xAF" "CHF", m.after);
  moneyAmount(m, EN, CURRENCY_CHF, 123.456f);
  TEST_ASSERT_EQUAL_STRING(".45 CHF", m.after);
  moneyTotal(e, sizeof(e), EN, CURRENCY_USD, 2345.6f);
  TEST_ASSERT_EQUAL_STRING("$2\xE2\x80\xAF" "345", e);
  moneyTotal(e, sizeof(e), FR, CURRENCY_GBP, 2345.6f);
  TEST_ASSERT_EQUAL_STRING("\xC2\xA3" "2\xE2\x80\xAF" "345", e);
  moneyTotal(e, sizeof(e), FR, CURRENCY_CHF, 2345.6f);
  TEST_ASSERT_EQUAL_STRING("2\xE2\x80\xAF" "345\xE2\x80\xAF" "CHF", e);
  // A currency NVS should never hold counts in euros, the default
  moneyAmount(m, EN, CURRENCY_COUNT, 1.0f);
  TEST_ASSERT_EQUAL_STRING(".00 \xE2\x82\xAC", m.after);
}

// Landscape: the state on the label line; the whole units in the first face
// that leaves room for the symbol before them and the cents after; the
// month's total on the foot. The largest day: a 999 999 a month salary over
// one working day. Every currency, both languages.
static void test_money_landscape_fits() {
  for (const Strings *t : {&EN, &FR}) {
    assertFits(t->working, LINE, Face::LABEL);
    assertFits(t->offHours, LINE, Face::LABEL);
    for (uint8_t cur = 0; cur < CURRENCY_COUNT; cur++) {
      MoneyText m;
      char total[32];
      for (float v : {0.0f, 8888.88f, 88888.88f, 999999.99f}) {
        moneyAmount(m, *t, cur, v);
        assertDrawable(m.before, Face::LABEL);
        assertDrawable(m.after, Face::LABEL);
        const uint16_t room = moneyRoom(m, LINE, false);
        const uint16_t sum = (m.before[0] ? w(m.before, Face::LABEL) + INFO_GAP : 0) +
                             w(m.units, numberFace(m.units, room)) + INFO_GAP +
                             w(m.after, Face::LABEL);
        assertSum(sum, LINE, m.units);
      }
      // A good day still in Pierre: four figures, or three beside CHF's letters
      moneyAmount(m, *t, cur, cur == CURRENCY_CHF ? 888.0f : 8888.0f);
      TEST_ASSERT_EQUAL(Face::PIERRE, numberFace(m.units, moneyRoom(m, LINE, false)));
      const uint16_t room = LINE - INFO_GAP - w(t->month, Face::LABEL);
      moneyTotal(total, sizeof(total), *t, cur, 999999.0f, room);
      assertDrawable(total, Face::LABEL);
      assertSum(w(t->month, Face::LABEL) + INFO_GAP + w(total, Face::LABEL), LINE, total);
      moneyTotal(total, sizeof(total), *t, cur, 9999.0f, room);   // the symbol while it fits
      TEST_ASSERT_FALSE_MESSAGE(strspn(total, "0123456789\xE2\x80\xAF") == strlen(total), total);
    }
  }
}

// Portrait: the symbol before and the whole units on one line, the cents and
// the symbol after under them, the month's total on its own line under its
// word, its symbol left out when it does not fit (the cents line has it)
static void test_money_portrait_fits() {
  for (const Strings *t : {&EN, &FR}) {
    assertFits(t->workingShort, TALL, Face::LABEL);
    assertFits(t->pausedShort, TALL, Face::LABEL);
    assertFits(t->month, TALL, Face::LABEL);
    for (uint8_t cur = 0; cur < CURRENCY_COUNT; cur++) {
      MoneyText m;
      char total[32];
      for (float v : {8.88f, 888.88f, 88888.88f}) {
        moneyAmount(m, *t, cur, v);
        const uint16_t room = moneyRoom(m, TALL, true);
        assertSum((m.before[0] ? w(m.before, Face::LABEL) + INFO_GAP : 0) +
                      w(m.units, numberFace(m.units, room)), TALL, m.units);
        assertFits(m.after, TALL, Face::LABEL);
      }
      moneyTotal(total, sizeof(total), *t, cur, 99999.0f, TALL);
      assertFits(total, TALL, Face::LABEL);
    }
    char total[32];
    moneyTotal(total, sizeof(total), *t, CURRENCY_EUR, 99999.0f, TALL);
    TEST_ASSERT_NOT_NULL(strstr(total, "\xE2\x82\xAC"));   // € fits as it did
  }
}

// --- SOCIAL -----------------------------------------------------------------------

static void test_social_shows_the_counters_that_have_something() {
  const uint32_t v[SOCIAL_COUNT] = {0, 1200, 0, 0};
  const bool live[SOCIAL_COUNT] = {false, false, false, true};
  uint8_t out[SOCIAL_COUNT];
  TEST_ASSERT_EQUAL_UINT8(2, socialShown(v, live, out));
  TEST_ASSERT_EQUAL_UINT8(SOCIAL_IG, out[0]);
  TEST_ASSERT_EQUAL_UINT8(SOCIAL_GH, out[1]);
}

static void test_social_with_nothing_set_shows_all_four() {
  const uint32_t v[SOCIAL_COUNT] = {0, 0, 0, 0};
  const bool live[SOCIAL_COUNT] = {false, false, false, false};
  uint8_t out[SOCIAL_COUNT];
  TEST_ASSERT_EQUAL_UINT8(4, socialShown(v, live, out));
  TEST_ASSERT_EQUAL_UINT8(SOCIAL_X, out[2]);
}

static void test_the_pager_has_a_dash_per_counter() {
  TEST_ASSERT_EQUAL_INT16(0, socialPagerWidth(1));   // one counter: no pager
  TEST_ASSERT_EQUAL_INT16(5, socialPagerWidth(2));
  TEST_ASSERT_EQUAL_INT16(11, socialPagerWidth(4));
}

static void test_social_counts_shrink_to_fit() {
  char b[24];
  TEST_ASSERT_EQUAL(Face::PIERRE, socialCount(b, sizeof(b), EN, 12345, LINE));
  TEST_ASSERT_EQUAL_STRING("12\xE2\x80\xAF" "345", b);
  TEST_ASSERT_EQUAL(Face::BODY, socialCount(b, sizeof(b), EN, 1234567, LINE));
  TEST_ASSERT_EQUAL(Face::LABEL, socialCount(b, sizeof(b), EN, 1234567, TALL));
  // Too long even in the label face: millions, a tenth kept
  socialCount(b, sizeof(b), EN, 4294967295u, TALL);
  TEST_ASSERT_EQUAL_STRING("4294M", b);
  socialCount(b, sizeof(b), EN, 12345678, TALL);
  TEST_ASSERT_EQUAL_STRING("12.3M", b);
  socialCount(b, sizeof(b), FR, 12345678, TALL);
  TEST_ASSERT_EQUAL_STRING("12,3\xE2\x80\xAFM", b);
}

static void test_social_fits_every_line() {
  for (const Strings *t : {&EN, &FR}) {
    for (const char *n : t->socialNames) assertFits(n, LINE - INFO_GAP - 3, Face::LABEL);   // the dot
    for (const char *n : t->socialNamesTall) assertFits(n, TALL, Face::LABEL);
    const uint16_t unit = LINE - INFO_GAP - socialPagerWidth(SOCIAL_COUNT);   // the pager
    assertFits(t->socialSubscribers, unit, Face::LABEL);
    assertFits(t->socialFollowers, unit, Face::LABEL);
    char b[24];
    for (uint32_t v : {0u, 88888u, 888888u, 8888888u, 88888888u, 4294967295u}) {
      assertFits(b, LINE, socialCount(b, sizeof(b), *t, v, LINE));
      assertFits(b, TALL, socialCount(b, sizeof(b), *t, v, TALL));
    }
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_digits_group_by_threes_with_a_narrow_space);
  RUN_TEST(test_a_number_takes_pierre_while_it_fits);
  RUN_TEST(test_a_face_centres_in_the_main_block);
  RUN_TEST(test_date_words);
  RUN_TEST(test_date_landscape_fits);
  RUN_TEST(test_date_portrait_fits);
  RUN_TEST(test_signal_bars_follow_the_rssi);
  RUN_TEST(test_uptime_in_minutes_hours_then_days);
  RUN_TEST(test_signal_shows_down_to_minus_99);
  RUN_TEST(test_ram_share);
  RUN_TEST(test_system_landscape_fits);
  RUN_TEST(test_system_portrait_fits);
  RUN_TEST(test_money_during_the_day);
  RUN_TEST(test_money_off_hours);
  RUN_TEST(test_money_never_divides_by_zero);
  RUN_TEST(test_money_amounts_in_each_language);
  RUN_TEST(test_each_currency_where_it_writes_its_symbol);
  RUN_TEST(test_money_landscape_fits);
  RUN_TEST(test_money_portrait_fits);
  RUN_TEST(test_social_shows_the_counters_that_have_something);
  RUN_TEST(test_social_with_nothing_set_shows_all_four);
  RUN_TEST(test_the_pager_has_a_dash_per_counter);
  RUN_TEST(test_social_counts_shrink_to_fit);
  RUN_TEST(test_social_fits_every_line);
  return UNITY_END();
}
