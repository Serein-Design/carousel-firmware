// SPDX-License-Identifier: AGPL-3.0-only
// Time helpers: the night window (also across midnight), and where the time
// comes from at start (time_logic): the servers' order, the HTTP Date, MET's
// UTC times. The time zone string is test_place's.
//
//   pio test -e native
#include <unity.h>

#include "../../src/settings.h"
#include "../../src/time_logic.cpp"

void setUp() {}
void tearDown() {}

static void test_night_window() {
  TEST_ASSERT_FALSE(inHourWindow(3, 0, 0));      // same hours: off
  TEST_ASSERT_TRUE(inHourWindow(23, 23, 7));     // crosses midnight
  TEST_ASSERT_TRUE(inHourWindow(0, 23, 7));
  TEST_ASSERT_TRUE(inHourWindow(6, 23, 7));
  TEST_ASSERT_FALSE(inHourWindow(7, 23, 7));     // end excluded
  TEST_ASSERT_FALSE(inHourWindow(22, 23, 7));
  TEST_ASSERT_TRUE(inHourWindow(13, 12, 14));    // same day (a nap)
  TEST_ASSERT_FALSE(inHourWindow(14, 12, 14));
}

// --- Servers ----------------------------------------------------------------

static void test_the_router_is_asked_first_then_the_public_ones_in_turn() {
  TEST_ASSERT_EQUAL(NTP_ROUTER, ntpServerFor(0, true));
  TEST_ASSERT_EQUAL(NTP_PUBLIC_1, ntpServerFor(1, true));
  TEST_ASSERT_EQUAL(NTP_PUBLIC_2, ntpServerFor(2, true));
  TEST_ASSERT_EQUAL(NTP_ROUTER, ntpServerFor(3, true));
  TEST_ASSERT_EQUAL(NTP_PUBLIC_1, ntpServerFor(0, false));   // no router named by DHCP
  TEST_ASSERT_EQUAL(NTP_PUBLIC_2, ntpServerFor(1, false));
  TEST_ASSERT_EQUAL(NTP_PUBLIC_1, ntpServerFor(2, false));
}

static void test_windows_last_2_s_then_5_s_or_15_s_once_the_time_is_known() {
  TEST_ASSERT_EQUAL_UINT32(2000, ntpWindowMs(0, false));
  TEST_ASSERT_EQUAL_UINT32(5000, ntpWindowMs(1, false));
  TEST_ASSERT_EQUAL_UINT32(5000, ntpWindowMs(40, false));
  TEST_ASSERT_EQUAL_UINT32(15000, ntpWindowMs(0, true));
  TEST_ASSERT_EQUAL_UINT32(15000, ntpWindowMs(3, true));
}

static void test_sources_are_named_for_the_log() {
  TEST_ASSERT_EQUAL_STRING("router", timeSourceName(TIME_ROUTER));
  TEST_ASSERT_EQUAL_STRING("pool", timeSourceName(TIME_POOL));
  TEST_ASSERT_EQUAL_STRING("http-date", timeSourceName(TIME_HTTP_DATE));
}

// --- HTTP Date --------------------------------------------------------------

static void test_a_valid_http_date_is_read_as_utc() {
  time_t t = 0;
  TEST_ASSERT_TRUE(parseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT", &t));   // RFC 9110's example
  TEST_ASSERT_EQUAL_INT64(784111777, (int64_t)t);
  TEST_ASSERT_TRUE(parseHttpDate("Fri, 23 Oct 2026 14:02:11 GMT", &t));
  TEST_ASSERT_EQUAL_INT64(1792764131, (int64_t)t);
  TEST_ASSERT_TRUE(parseHttpDate("Thu, 29 Feb 2024 23:59:60 GMT", &t));   // leap day, leap second
  TEST_ASSERT_EQUAL_INT64(1709251200, (int64_t)t);
  TEST_ASSERT_TRUE(parseHttpDate("Thu, 01 Jan 1970 00:00:00 GMT", &t));
  TEST_ASSERT_EQUAL_INT64(0, (int64_t)t);
}

static void test_a_malformed_http_date_is_refused() {
  const char *bad[] = {
    "Sunday, 06-Nov-94 08:49:37 GMT",   // RFC 850, obsolete
    "Sun Nov  6 08:49:37 1994",         // asctime, obsolete
    "Sun, 06 Nov 1994 08:49:37 UTC",    // not GMT
    "Sun, 06 Nov 1994 08:49:37",        // no zone
    "Sun, 06 Nov 1994 08:49:37 GMT+1",  // trailing text
    "Sun, 6 Nov 1994 08:49:37 GMT",     // one-digit day
    "Sun, 06 Nob 1994 08:49:37 GMT",    // no such month
    "Sun, 31 Apr 2026 08:49:37 GMT",    // no 31 April
    "Sun, 29 Feb 2026 08:49:37 GMT",    // 2026 is no leap year
    "Sun, 00 Nov 1994 08:49:37 GMT",
    "Sun, 06 Nov 1994 24:00:00 GMT",
    "Sun, 06 Nov 1994 08:60:00 GMT",
    "Sun, 06 Nov 1994 08:49:61 GMT",
    "Sun, 06 Nov 1969 08:49:37 GMT",    // before the epoch
    "Sun, 06 Nov 19a4 08:49:37 GMT",
    "Sun 06 Nov 1994 08:49:37 GMT",     // no comma
    "Sun, 06 Nov 1994 08:49 GMT",
  };
  for (const char *s : bad) {
    time_t t = 42;
    TEST_ASSERT_FALSE_MESSAGE(parseHttpDate(s, &t), s);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(42, (int64_t)t, s);
  }
}

static void test_an_empty_or_missing_http_date_is_refused() {
  time_t t = 42;
  TEST_ASSERT_FALSE(parseHttpDate("", &t));
  TEST_ASSERT_FALSE(parseHttpDate(nullptr, &t));
  TEST_ASSERT_EQUAL_INT64(42, (int64_t)t);
}

// --- Kept across a restart --------------------------------------------------

// MET Norway's hours: UTC, to the second, nothing else accepted
static void test_an_iso_utc_time_is_read() {
  time_t t = 0;
  TEST_ASSERT_TRUE(parseIsoUtc("2026-10-25T11:00:00Z", &t));
  TEST_ASSERT_EQUAL(1792926000, t);
  TEST_ASSERT_TRUE(parseIsoUtc("2024-02-29T23:59:60Z", &t));
  TEST_ASSERT_EQUAL(1709251200, t);
  const char *bad[] = {"2026-10-25T11:00:00", "2026-10-25T11:00:00+02:00", "2026-10-25 11:00:00Z",
                       "2026-13-25T11:00:00Z", "2025-02-29T11:00:00Z", "2026-10-25T24:00:00Z",
                       "2026-10-25T11:60:00Z", "2026-1O-25T11:00:00Z", "1969-12-31T23:59:59Z",
                       "", nullptr};
  for (const char *s : bad) {
    t = 7;
    TEST_ASSERT_FALSE_MESSAGE(parseIsoUtc(s, &t), s ? s : "null");
    TEST_ASSERT_EQUAL(7, t);
  }
}

static void test_the_build_day_is_read_from_the_compilers_date() {
  TEST_ASSERT_EQUAL_INT64(1792713600, (int64_t)buildDay("Oct 23 2026"));
  TEST_ASSERT_EQUAL_INT64(1790985600, (int64_t)buildDay("Oct  3 2026"));   // space-padded day
  TEST_ASSERT_EQUAL_INT64(0, (int64_t)buildDay("Okt 23 2026"));
  TEST_ASSERT_EQUAL_INT64(0, (int64_t)buildDay(""));
  TEST_ASSERT_TRUE(buildDay(__DATE__) > 1790000000);   // this very build, after 2026-09
}

static void test_a_time_before_the_build_is_not_plausible() {
  const time_t built = buildDay("Oct 23 2026");
  TEST_ASSERT_TRUE(timePlausible(built, built));
  TEST_ASSERT_FALSE(timePlausible(built - 1, built));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_night_window);
  RUN_TEST(test_the_router_is_asked_first_then_the_public_ones_in_turn);
  RUN_TEST(test_windows_last_2_s_then_5_s_or_15_s_once_the_time_is_known);
  RUN_TEST(test_sources_are_named_for_the_log);
  RUN_TEST(test_a_valid_http_date_is_read_as_utc);
  RUN_TEST(test_a_malformed_http_date_is_refused);
  RUN_TEST(test_an_empty_or_missing_http_date_is_refused);
  RUN_TEST(test_an_iso_utc_time_is_read);
  RUN_TEST(test_the_build_day_is_read_from_the_compilers_date);
  RUN_TEST(test_a_time_before_the_build_is_not_plausible);
  return UNITY_END();
}
