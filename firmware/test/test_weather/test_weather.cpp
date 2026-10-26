// SPDX-License-Identifier: AGPL-3.0-only
// WEATHER without the panel: every condition's word fits its line in both
// languages, temperatures fit their slots, and the columns of the hours ahead
// follow the clock. The forecast itself is read in test_met.
//
//   pio test -e native -f test_weather
#include <unity.h>

#include <cstring>
#include <string>

#include "../../src/fonts_data.cpp"
#include "../../src/i18n.cpp"
#include "../../src/type.cpp"
#include "../../src/weather_logic.cpp"

void setUp() {}
void tearDown() {}

static void test_every_word_fits() {
  static_assert(sizeof(Strings::wxCond) / sizeof(const char *) == WX_COND_COUNT,
                "one word per condition");
  for (int l = 0; l < LANG_COUNT; l++)
    for (int c = 0; c < WX_COND_COUNT; c++) {
      const char *w = STRINGS[l].wxCond[c];
      char msg[64];
      snprintf(msg, sizeof(msg), "%s: \"%s\"", STRINGS[l].code, w);
      TEST_ASSERT_TRUE_MESSAGE(w && w[0], msg);
      // The measure to hold is the body face; the foot line draws the label face
      TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(60, textWidth(w, Face::BODY), msg);
      TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(60, textWidth(w, Face::LABEL), msg);
      // Portrait: two label lines of 28 px, nothing lost
      char a[32], b[32];
      splitLines(w, Face::LABEL, 28, a, b, sizeof(a));
      TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(28, textWidth(a, Face::LABEL), msg);
      TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(28, textWidth(b, Face::LABEL), msg);
      std::string joined = std::string(a) + (b[0] ? " " : "") + b, hyph = std::string(a);
      if (!hyph.empty() && hyph.back() == '-') hyph.pop_back();
      TEST_ASSERT_TRUE_MESSAGE(joined == w || hyph + b == w, msg);
    }
}

static void test_words_as_written() {
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("Clear", en.wxCond[WX_CLEAR]);
  TEST_ASSERT_EQUAL_STRING("Some clouds", en.wxCond[WX_PARTLY]);
  TEST_ASSERT_EQUAL_STRING("D\xC3\xA9gag\xC3\xA9", fr.wxCond[WX_CLEAR]);
  TEST_ASSERT_EQUAL_STRING("\xC3\x89" "claircies", fr.wxCond[WX_PARTLY]);
  TEST_ASSERT_EQUAL_STRING("Orage", fr.wxCond[WX_STORM]);
  char h[8];
  for (int l = 0; l < LANG_COUNT; l++) {
    snprintf(h, sizeof(h), STRINGS[l].wxHour, 23u);
    TEST_ASSERT_EQUAL_STRING("23H", h);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(14, textWidth(h, Face::LABEL));
  }
}

static void test_split_lines() {
  char a[32], b[32];
  splitLines("Beau temps", Face::LABEL, 28, a, b, sizeof(a));
  TEST_ASSERT_EQUAL_STRING("Beau", a);
  TEST_ASSERT_EQUAL_STRING("temps", b);
  splitLines("\xC3\x89" "claircies", Face::LABEL, 28, a, b, sizeof(a));
  TEST_ASSERT_EQUAL_STRING("\xC3\x89" "clair-", a);       // the board's ÉCLAIR- / CIES
  TEST_ASSERT_EQUAL_STRING("cies", b);
  splitLines("Pluie", Face::LABEL, 28, a, b, sizeof(a));
  TEST_ASSERT_EQUAL_STRING("Pluie", a);
  TEST_ASSERT_EQUAL_STRING("", b);
}

static void test_temperatures() {
  TEST_ASSERT_EQUAL(0, roundTemp(-0.4f));
  TEST_ASSERT_EQUAL(-1, roundTemp(-0.6f));
  TEST_ASSERT_EQUAL(15, roundTemp(14.5f));
  char s[16];
  formatMinMax(s, sizeof(s), 5.9f, 13.3f);
  TEST_ASSERT_EQUAL_STRING("6\xC2\xB0 13\xC2\xB0", s);
  // Every plausible temperature fits each place it is drawn, in either unit
  for (int c = -40; c <= 50; c++)
  for (uint8_t unit : {TEMP_C, TEMP_F}) {
    const float t = tempIn(c, unit);
    char msg[16];
    snprintf(msg, sizeof(msg), "%d %s", c, unit == TEMP_F ? "F" : "C");
    TEST_ASSERT_TRUE_MESSAGE(formatTemp(s, sizeof(s), t, Face::PIERRE, 42), msg);  // W1 landscape
    TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(42, textWidth(s, Face::PIERRE), msg);
    TEST_ASSERT_TRUE_MESSAGE(formatTemp(s, sizeof(s), t, Face::PIERRE, 28), msg);  // portrait
    TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(28, textWidth(s, Face::PIERRE), msg);
    TEST_ASSERT_TRUE_MESSAGE(formatTemp(s, sizeof(s), t, Face::LABEL, 14), msg);   // W3 column
    TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(14, textWidth(s, Face::LABEL), msg);
    formatMinMax(s, sizeof(s), t, t);   // half the label line at most: the place keeps the rest
    TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(WX_MINMAX_W, textWidth(s, Face::LABEL), msg);
  }
  formatTemp(s, sizeof(s), 14.2f, Face::PIERRE, 42);
  TEST_ASSERT_EQUAL_STRING("14 \xC2\xB0", s);              // the board's spaced degree
  formatTemp(s, sizeof(s), -12, Face::PIERRE, 28);
  TEST_ASSERT_EQUAL_STRING("-12", s);                      // the degree gives way first
  formatTemp(s, sizeof(s), -3, Face::LABEL, 14);
  TEST_ASSERT_EQUAL_STRING("-3\xC2\xB0", s);
}

// °F: the conversion, then the rounding WEATHER and CLOCK use for °C
static void test_fahrenheit() {
  TEST_ASSERT_EQUAL(-40, roundTemp(tempIn(-40, TEMP_F)));
  TEST_ASSERT_EQUAL(32, roundTemp(tempIn(0, TEMP_F)));
  TEST_ASSERT_EQUAL(99, roundTemp(tempIn(37, TEMP_F)));      // 98.6
  TEST_ASSERT_EQUAL(212, roundTemp(tempIn(100, TEMP_F)));
  TEST_ASSERT_EQUAL(100, roundTemp(tempIn(37.5f, TEMP_F)));  // 99.5: half away from zero
  TEST_ASSERT_EQUAL(1, roundTemp(tempIn(-17.0f, TEMP_F)));   // 1.4
  TEST_ASSERT_EQUAL(0, roundTemp(tempIn(-17.9f, TEMP_F)));   // -0.22: no "-0"
  TEST_ASSERT_EQUAL(-1, roundTemp(tempIn(-18.5f, TEMP_F)));  // -1.3
  // °C, and a unit NVS should never hold, show the reading as it came
  TEST_ASSERT_EQUAL_FLOAT(14.2f, tempIn(14.2f, TEMP_C));
  TEST_ASSERT_EQUAL_FLOAT(14.2f, tempIn(14.2f, TEMP_UNIT_COUNT));
  char s[16];
  formatMinMax(s, sizeof(s), tempIn(15, TEMP_F), tempIn(25, TEMP_F));
  TEST_ASSERT_EQUAL_STRING("59\xC2\xB0 77\xC2\xB0", s);   // the degree sign unchanged
  formatMinMax(s, sizeof(s), tempIn(38, TEMP_F), tempIn(50, TEMP_F));
  TEST_ASSERT_EQUAL_STRING("100 122", s);   // "100° 122°" is 34 px
  formatTemp(s, sizeof(s), tempIn(14.2f, TEMP_F), Face::PIERRE, 42);
  TEST_ASSERT_EQUAL_STRING("58 \xC2\xB0", s);
}

static void test_place_name() {
  char s[24];
  placeName("Paris, \xC3\x8Ele-de-France, France", "paris", s, sizeof(s));
  TEST_ASSERT_EQUAL_STRING("Paris", s);
  placeName("", "Lyon", s, sizeof(s));
  TEST_ASSERT_EQUAL_STRING("Lyon", s);
  placeName("", "", s, sizeof(s));
  TEST_ASSERT_EQUAL_STRING("", s);
  // Cut by whole characters: é goes as one, never half of its two bytes
  const uint16_t w = textWidth("Saint-R\xC3\xA9", Face::LABEL);
  strcpy(s, "Saint-R\xC3\xA9my-de-Provence");
  fitText(s, Face::LABEL, w);
  TEST_ASSERT_EQUAL_STRING("Saint-R\xC3\xA9", s);
  strcpy(s, "Saint-R\xC3\xA9my-de-Provence");
  fitText(s, Face::LABEL, w - 1);
  TEST_ASSERT_EQUAL_STRING("Saint-R", s);
  strcpy(s, "Lyon");
  fitText(s, Face::LABEL, 60);
  TEST_ASSERT_EQUAL_STRING("Lyon", s);
}

// The hours of a forecast fetched at 20:00: 20:00 to 05:00
static HourlyForecast tenHours() {
  HourlyForecast h;
  h.count = 10;
  for (uint8_t i = 0; i < h.count; i++) {
    h.hour[i] = (uint8_t)((20 + i) % 24);
    h.temp[i] = 13;
    h.cond[i] = WX_RAIN;
    h.day[i] = false;
  }
  return h;
}

static void test_day_slots() {
  const HourlyForecast h = tenHours();
  uint8_t idx[3];
  for (int now : {-1, 20}) {   // no clock yet: the forecast's first hour is now
    TEST_ASSERT_EQUAL(3, daySlots(h, now, idx));
    TEST_ASSERT_EQUAL(3, idx[0]);
    TEST_ASSERT_EQUAL(6, idx[1]);
    TEST_ASSERT_EQUAL(9, idx[2]);
  }
  // The hour turned since the fetch: the columns follow the clock, and only
  // the hours the forecast holds are shown
  TEST_ASSERT_EQUAL(2, daySlots(h, 21, idx));
  TEST_ASSERT_EQUAL(4, idx[0]);
  TEST_ASSERT_EQUAL(7, idx[1]);
  TEST_ASSERT_EQUAL(1, daySlots(h, 1, idx));   // across midnight: 5 h after 20:00
  TEST_ASSERT_EQUAL(8, idx[0]);
  TEST_ASSERT_EQUAL(0, daySlots(h, 19, idx));  // the clock behind the forecast: none
  // No hours at all: now only
  HourlyForecast none;
  TEST_ASSERT_EQUAL(0, daySlots(none, -1, idx));
  TEST_ASSERT_EQUAL(0, daySlots(none, 20, idx));
  // Four hours: +3 only
  HourlyForecast four = tenHours();
  four.count = 4;
  TEST_ASSERT_EQUAL(1, daySlots(four, -1, idx));
  TEST_ASSERT_EQUAL(3, idx[0]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_word_fits);
  RUN_TEST(test_words_as_written);
  RUN_TEST(test_split_lines);
  RUN_TEST(test_temperatures);
  RUN_TEST(test_fahrenheit);
  RUN_TEST(test_place_name);
  RUN_TEST(test_day_slots);
  return UNITY_END();
}
