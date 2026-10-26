// SPDX-License-Identifier: AGPL-3.0-only
// The zone a phone reports at setup -> the POSIX TZ the C library applies:
// canonical names, links, names tzdata does not know, and one string put to
// work on both sides of a daylight saving change.
//
//   pio test -e native -f test_tz_names
#include <unity.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../src/tz_names.cpp"

void setUp() {}
void tearDown() {}

static void test_canonical_names() {
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", tzPosix("Europe/Paris"));
  TEST_ASSERT_EQUAL_STRING("EST5EDT,M3.2.0,M11.1.0", tzPosix("America/New_York"));
  TEST_ASSERT_EQUAL_STRING("IST-5:30", tzPosix("Asia/Kolkata"));
  TEST_ASSERT_EQUAL_STRING("AEST-10AEDT,M10.1.0,M4.1.0/3", tzPosix("Australia/Sydney"));
  // The first and last names of the table
  TEST_ASSERT_NOT_NULL(tzPosix("Africa/Abidjan"));
  TEST_ASSERT_NOT_NULL(tzPosix("Zulu"));
}

static void test_links() {
  // Older names some phones still report share their zone's string
  TEST_ASSERT_EQUAL_PTR(tzPosix("America/New_York"), tzPosix("US/Eastern"));
  TEST_ASSERT_EQUAL_PTR(tzPosix("Asia/Kolkata"), tzPosix("Asia/Calcutta"));
  TEST_ASSERT_EQUAL_STRING("UTC0", tzPosix("UTC"));
}

static void test_unknown_names() {
  TEST_ASSERT_NULL(tzPosix("Mars/Olympus_Mons"));
  TEST_ASSERT_NULL(tzPosix("Europe/Pari"));     // a prefix of a name
  TEST_ASSERT_NULL(tzPosix("Europe/Paris2"));   // a name continued
  TEST_ASSERT_NULL(tzPosix("europe/paris"));    // names are case-sensitive
  TEST_ASSERT_NULL(tzPosix(""));
  TEST_ASSERT_NULL(tzPosix(nullptr));
}

static void test_the_string_applies_daylight_saving() {
  setenv("TZ", tzPosix("Europe/Paris"), 1);
  tzset();
  time_t winter = 1767268800, summer = 1782907200;   // 2026-01-01 and 2026-07-01, 12:00 UTC
  struct tm tm;
  localtime_r(&winter, &tm);
  TEST_ASSERT_EQUAL(13, tm.tm_hour);
  localtime_r(&summer, &tm);
  TEST_ASSERT_EQUAL(14, tm.tm_hour);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_canonical_names);
  RUN_TEST(test_links);
  RUN_TEST(test_unknown_names);
  RUN_TEST(test_the_string_applies_daylight_saving);
  return UNITY_END();
}
