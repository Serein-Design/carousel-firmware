// SPDX-License-Identifier: AGPL-3.0-only
// The owner's region as other programs read it: the ISO code of each
// currency (the web pages name the money in it) and the three fields
// /api/info gives.
//
//   pio test -e native -f test_region
#include <unity.h>

#include <Arduino.h>

#include "../../src/region_logic.cpp"

void setUp() {}
void tearDown() {}

static void test_currency_codes() {
  TEST_ASSERT_EQUAL_STRING("EUR", currencyCode(CURRENCY_EUR));
  TEST_ASSERT_EQUAL_STRING("USD", currencyCode(CURRENCY_USD));
  TEST_ASSERT_EQUAL_STRING("GBP", currencyCode(CURRENCY_GBP));
  TEST_ASSERT_EQUAL_STRING("CHF", currencyCode(CURRENCY_CHF));
  // A value no panel stores reads as the default
  TEST_ASSERT_EQUAL_STRING("EUR", currencyCode(CURRENCY_COUNT));
  TEST_ASSERT_EQUAL_STRING("EUR", currencyCode(255));
}

static void test_defaults_are_what_v010_showed() {
  String j;
  regionInfoJson(j, TEMP_C, CURRENCY_EUR, false);
  TEST_ASSERT_EQUAL_STRING(",\"temp_unit\":\"C\",\"currency\":\"EUR\",\"clock12\":false", j.c_str());
}

static void test_chosen_region_in_info() {
  String j;
  regionInfoJson(j, TEMP_F, CURRENCY_CHF, true);
  TEST_ASSERT_EQUAL_STRING(",\"temp_unit\":\"F\",\"currency\":\"CHF\",\"clock12\":true", j.c_str());
  j = String();
  regionInfoJson(j, TEMP_F, CURRENCY_GBP, true);
  TEST_ASSERT_EQUAL_STRING(",\"temp_unit\":\"F\",\"currency\":\"GBP\",\"clock12\":true", j.c_str());
}

// Out-of-range bytes (a corrupt RAM copy) answer the defaults, never garbage
static void test_out_of_range_in_info() {
  String j;
  regionInfoJson(j, 9, 77, false);
  TEST_ASSERT_EQUAL_STRING(",\"temp_unit\":\"C\",\"currency\":\"EUR\",\"clock12\":false", j.c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_currency_codes);
  RUN_TEST(test_defaults_are_what_v010_showed);
  RUN_TEST(test_chosen_region_in_info);
  RUN_TEST(test_out_of_range_in_info);
  return UNITY_END();
}
