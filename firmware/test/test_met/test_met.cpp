// SPDX-License-Identifier: AGPL-3.0-only
// The forecast from MET Norway without the panel: a real answer (compact.json,
// Paris, asked on 2026-10-25 at 11:08 UTC, trimmed to 28 of its 89 hours) read
// whole, a byte at a time, cut anywhere and with holes; every symbol MET
// documents named; the request; and when the panel asks again (Expires, its
// own second of the hour, the backoff).
//
//   pio test -e native -f test_met
#include <unity.h>

#include <stdlib.h>

#include <algorithm>
#include <string>

#include "../../src/met_logic.cpp"
#include "../../src/time_logic.cpp"

void setUp() {}
void tearDown() {}

static std::string fixture() {
  std::string path = __FILE__;   // .../firmware/test/test_met/test_met.cpp
  path = path.substr(0, path.rfind('/') + 1) + "compact.json";
  FILE *f = fopen(path.c_str(), "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, path.c_str());
  std::string s;
  char b[4096];
  for (size_t n; (n = fread(b, 1, sizeof(b), f)) > 0; ) s.append(b, n);
  fclose(f);
  return s;
}

static time_t utc(const char *iso) {
  time_t t = 0;
  TEST_ASSERT_TRUE_MESSAGE(parseIsoUtc(iso, &t), iso);
  return t;
}

// The answer's Date: Sun, 25 Oct 2026 11:08:11 GMT
static const time_t ASKED = 1792926491;

static MetForecast read(const std::string &body, time_t now, size_t chunk = 0) {
  MetReader r(now);
  if (!chunk) chunk = body.size();
  for (size_t at = 0; at < body.size() && r.feed(body.data() + at, std::min(chunk, body.size() - at));
       at += chunk) {}
  return r.result();
}

static std::string replaced(const std::string &s, const char *from, const char *to) {
  const size_t at = s.find(from);
  TEST_ASSERT_TRUE_MESSAGE(at != std::string::npos, from);
  std::string out = s;
  out.replace(at, strlen(from), to);
  return out;
}

static void test_symbols() {
  struct { const char *symbol; WxCond c; } cases[] = {
      {"clearsky_day", WX_CLEAR}, {"clearsky_night", WX_CLEAR},
      {"clearsky_polartwilight", WX_CLEAR}, {"fair_day", WX_MOSTLY_CLEAR},
      {"partlycloudy_night", WX_PARTLY}, {"cloudy", WX_OVERCAST}, {"fog", WX_FOG},
      {"lightrain", WX_DRIZZLE}, {"rain", WX_RAIN}, {"heavyrain", WX_RAIN},
      {"lightrainshowers_day", WX_SHOWERS}, {"rainshowers_night", WX_SHOWERS},
      {"heavyrainshowers_polartwilight", WX_SHOWERS},
      {"lightsleet", WX_FREEZING}, {"sleet", WX_FREEZING}, {"heavysleetshowers_day", WX_FREEZING},
      {"lightsnow", WX_SNOW}, {"snow", WX_SNOW}, {"heavysnowshowers_night", WX_SNOW},
      {"rainandthunder", WX_STORM}, {"lightrainshowersandthunder_day", WX_STORM},
      {"heavysleetandthunder", WX_STORM}, {"snowshowersandthunder_night", WX_STORM},
      // MET's own spelling, kept for compatibility
      {"lightssleetshowersandthunder_day", WX_STORM}, {"lightssnowshowersandthunder_night", WX_STORM},
      {"tornado", WX_UNKNOWN}, {"", WX_UNKNOWN}, {"_day", WX_UNKNOWN}, {"rainy", WX_UNKNOWN},
  };
  for (const auto &k : cases) TEST_ASSERT_EQUAL_MESSAGE(k.c, metCondition(k.symbol), k.symbol);
  TEST_ASSERT_TRUE(metDay("clearsky_day"));
  TEST_ASSERT_FALSE(metDay("fair_night"));
  TEST_ASSERT_TRUE(metDay("partlycloudy_polartwilight"));
  TEST_ASSERT_TRUE(metDay("cloudy"));
}

static void test_request() {
  char s[24];
  metCoord(s, sizeof(s), 48.85341f);
  TEST_ASSERT_EQUAL_STRING("48.8534", s);
  metCoord(s, sizeof(s), 2.3488f);         // 2.34879994 as a float: not cut to 2.3487
  TEST_ASSERT_EQUAL_STRING("2.3488", s);
  metCoord(s, sizeof(s), -74.00597f);      // cut toward zero, not rounded
  TEST_ASSERT_EQUAL_STRING("-74.0059", s);
  metCoord(s, sizeof(s), -0.00004f);
  TEST_ASSERT_EQUAL_STRING("0.0000", s);
  metCoord(s, sizeof(s), 0.5f);
  TEST_ASSERT_EQUAL_STRING("0.5000", s);
  metCoord(s, sizeof(s), -0.5f);
  TEST_ASSERT_EQUAL_STRING("-0.5000", s);
  char url[128];
  metUrl(url, sizeof(url), 48.85341f, 2.3488f);
  TEST_ASSERT_EQUAL_STRING(
      "https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=48.8534&lon=2.3488", url);
}

static void test_answer_at_noon() {
  const MetForecast f = read(fixture(), ASKED);
  TEST_ASSERT_TRUE(f.valid);
  const HourlyForecast &h = f.hourly;
  // Now: the 11:00 UTC hour (12:00 in Paris, back on UTC+1 since 01:00 UTC
  // that day), not the 10:00 one the answer starts with
  TEST_ASSERT_EQUAL(utc("2026-10-25T11:00:00Z"), h.start);
  TEST_ASSERT_EQUAL(WEATHER_HOURS, h.count);
  TEST_ASSERT_EQUAL(12, h.hour[0]);
  TEST_ASSERT_EQUAL_FLOAT(14.0f, h.temp[0]);
  TEST_ASSERT_EQUAL(WX_MOSTLY_CLEAR, h.cond[0]);   // fair_day
  TEST_ASSERT_TRUE(h.day[0]);
  TEST_ASSERT_EQUAL(15, h.hour[3]);
  TEST_ASSERT_EQUAL_FLOAT(14.6f, h.temp[3]);
  TEST_ASSERT_EQUAL(18, h.hour[6]);
  TEST_ASSERT_FALSE(h.day[6]);                     // fair_night
  TEST_ASSERT_EQUAL(23, h.hour[11]);
  TEST_ASSERT_EQUAL(WX_PARTLY, h.cond[11]);
  // The local day: 10:00 to 22:00 UTC in the answer, the hour before now included
  TEST_ASSERT_EQUAL(20261025, f.day);
  TEST_ASSERT_EQUAL_FLOAT(10.2f, f.tMin);
  TEST_ASSERT_EQUAL_FLOAT(14.6f, f.tMax);
  // A byte at a time, or 7: the same
  for (size_t chunk : {1, 7, 1460}) {
    const MetForecast g = read(fixture(), ASKED, chunk);
    TEST_ASSERT_TRUE(g.valid);
    TEST_ASSERT_EQUAL(h.count, g.hourly.count);
    TEST_ASSERT_EQUAL_FLOAT(h.temp[11], g.hourly.temp[11]);
    TEST_ASSERT_EQUAL_FLOAT(f.tMin, g.tMin);
  }
}

static void test_answer_in_the_evening() {
  // Read at 20:30 UTC (21:30 in Paris): the hours run past midnight, the day
  // keeps the hours of the morning the answer still holds
  const MetForecast f = read(fixture(), utc("2026-10-25T20:30:00Z"));
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL(WEATHER_HOURS, f.hourly.count);
  TEST_ASSERT_EQUAL(21, f.hourly.hour[0]);
  TEST_ASSERT_EQUAL_FLOAT(10.7f, f.hourly.temp[0]);
  TEST_ASSERT_EQUAL(WX_PARTLY, f.hourly.cond[0]);
  TEST_ASSERT_FALSE(f.hourly.day[0]);
  TEST_ASSERT_EQUAL(0, f.hourly.hour[3]);           // 23:00 UTC, midnight in Paris
  TEST_ASSERT_EQUAL(WX_OVERCAST, f.hourly.cond[4]);
  TEST_ASSERT_EQUAL(8, f.hourly.hour[11]);
  TEST_ASSERT_EQUAL(20261025, f.day);
  TEST_ASSERT_EQUAL_FLOAT(10.2f, f.tMin);
  TEST_ASSERT_EQUAL_FLOAT(14.6f, f.tMax);
}

static void test_without_a_date() {
  // No Date and no clock: the answer's first hour stands for now
  const MetForecast f = read(fixture(), 0);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL(utc("2026-10-25T10:00:00Z"), f.hourly.start);
  TEST_ASSERT_EQUAL_FLOAT(13.8f, f.hourly.temp[0]);
  TEST_ASSERT_EQUAL(WX_SHOWERS, f.hourly.cond[0]);
}

static void test_answer_with_holes() {
  const std::string full = fixture();
  // The current hour without its temperature: nothing to show
  std::string s = replaced(full, "\"air_temperature\":14.0,", "");
  TEST_ASSERT_FALSE(read(s, ASKED).valid);
  // A later hour without its symbol: the hours stop there, the day does not
  s = replaced(full, "{\"symbol_code\":\"clearsky_day\"}", "{}");
  MetForecast f = read(s, ASKED);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL(2, f.hourly.count);              // 11:00, 12:00; 13:00 has none
  TEST_ASSERT_EQUAL_FLOAT(14.6f, f.tMax);
  // A temperature out of reason (or null) ends the run the same way
  s = replaced(full, "\"air_temperature\":14.5,", "\"air_temperature\":999,");
  TEST_ASSERT_EQUAL(2, read(s, ASKED).hourly.count);
  s = replaced(full, "\"air_temperature\":14.5,", "\"air_temperature\":null,");
  TEST_ASSERT_EQUAL(2, read(s, ASKED).hourly.count);
  // A symbol MET adds one day: no word, the hour still counts
  s = replaced(full, "\"clearsky_day\"", "\"hail_day\"");
  f = read(s, ASKED);
  TEST_ASSERT_EQUAL(WEATHER_HOURS, f.hourly.count);
  TEST_ASSERT_EQUAL(WX_UNKNOWN, f.hourly.cond[2]);
  // An hour missing from the series: the run stops before the gap
  s = replaced(full, "\"2026-10-25T14:00:00Z\"", "\"2026-10-25T14:30:00Z\"");
  TEST_ASSERT_EQUAL(3, read(s, ASKED).hourly.count);
  // A broken time
  s = replaced(full, "\"2026-10-25T11:00:00Z\"", "\"2026-10-25T11:00\"");
  f = read(s, ASKED);
  TEST_ASSERT_TRUE(f.valid);                          // the 12:00 hour starts the run
  TEST_ASSERT_EQUAL(utc("2026-10-25T12:00:00Z"), f.hourly.start);
  // Not the answer expected at all
  TEST_ASSERT_FALSE(read("{\"error\":\"Bad Request\"}", ASKED).valid);
  TEST_ASSERT_FALSE(read("<html>502 Bad Gateway</html>", ASKED).valid);
  TEST_ASSERT_FALSE(read("", ASKED).valid);
  // The series ends early: what it holds stands
  const size_t end = full.find(",{\"time\":\"2026-10-25T16:00:00Z\"");
  s = full.substr(0, end) + "]}}";
  f = read(s, ASKED);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL(5, f.hourly.count);
  // Too old: every hour is past
  TEST_ASSERT_FALSE(read(full, utc("2026-11-04T00:00:00Z")).valid);
}

static void test_answer_cut_anywhere() {
  const std::string full = fixture();
  // Where the reading has all it needs: the 23:00 UTC hour, the next local day
  const size_t needed = full.find("\"2026-10-25T23:00:00Z\"");
  const size_t closed = full.find("{\"time\":\"2026-10-26T00:00:00Z\"");
  TEST_ASSERT_TRUE(needed != std::string::npos && closed != std::string::npos);
  // AddressSanitizer watches every copy
  for (size_t n = 0; n < full.size(); n += 3) {
    char *cut = new char[n];
    memcpy(cut, full.data(), n);
    MetReader r(ASKED);
    r.feed(cut, n);
    const MetForecast f = r.result();
    if (n < needed) TEST_ASSERT_FALSE_MESSAGE(f.valid, std::to_string(n).c_str());
    if (n >= closed) {
      TEST_ASSERT_TRUE_MESSAGE(f.valid, std::to_string(n).c_str());
      TEST_ASSERT_EQUAL(WEATHER_HOURS, f.hourly.count);
      TEST_ASSERT_EQUAL_FLOAT(10.2f, f.tMin);
    }
    delete[] cut;
  }
  // Done: the rest of the body is not wanted
  MetReader r(ASKED);
  TEST_ASSERT_FALSE(r.feed(full.data(), closed));
  TEST_ASSERT_TRUE(r.done());
}

// A scalar at the root then a comma: no container to go on in, refused
// (it read the container bit at depth -1, undefined behaviour)
static void test_scalar_at_the_root() {
  for (const char *body : {"1,", "\"a\",", "true ,", "1]", "\"a\"}"}) {
    MetReader r(ASKED);
    TEST_ASSERT_FALSE_MESSAGE(r.feed(body, strlen(body)), body);
    TEST_ASSERT_FALSE_MESSAGE(r.done(), body);
    TEST_ASSERT_FALSE_MESSAGE(r.result().valid, body);
  }
}

static void test_not_modified() {
  MetForecast f = read(fixture(), ASKED);
  // 304 at 13:10 UTC: the answer of 11:08 still holds, two hours shorter
  TEST_ASSERT_TRUE(metAdvance(f, utc("2026-10-25T13:10:00Z")));
  TEST_ASSERT_EQUAL(WEATHER_HOURS - 2, f.hourly.count);
  TEST_ASSERT_EQUAL(utc("2026-10-25T13:00:00Z"), f.hourly.start);
  TEST_ASSERT_EQUAL(14, f.hourly.hour[0]);
  TEST_ASSERT_EQUAL_FLOAT(14.5f, f.hourly.temp[0]);
  TEST_ASSERT_EQUAL(WX_CLEAR, f.hourly.cond[0]);
  TEST_ASSERT_EQUAL(23, f.hourly.hour[9]);
  // The same hour, or a clock behind: unchanged
  TEST_ASSERT_TRUE(metAdvance(f, utc("2026-10-25T13:59:59Z")));
  TEST_ASSERT_TRUE(metAdvance(f, utc("2026-10-25T09:00:00Z")));
  TEST_ASSERT_EQUAL(WEATHER_HOURS - 2, f.hourly.count);
  // Past its last hour: nothing left to show
  TEST_ASSERT_FALSE(metAdvance(f, utc("2026-10-25T23:00:00Z")));
  TEST_ASSERT_FALSE(f.valid);
}

static void test_the_day_is_kept() {
  MetForecast morning = read(fixture(), ASKED);
  morning.tMin = 7.5f;                        // the morning's low, gone from later answers
  MetForecast later = read(fixture(), ASKED);
  metKeepDay(later, morning);
  TEST_ASSERT_EQUAL_FLOAT(7.5f, later.tMin);
  TEST_ASSERT_EQUAL_FLOAT(14.6f, later.tMax);
  // Another day: the new answer's own
  MetForecast tomorrow = read(fixture(), ASKED);
  morning.day = 20261024;
  metKeepDay(tomorrow, morning);
  TEST_ASSERT_EQUAL_FLOAT(10.2f, tomorrow.tMin);
  // Nothing before: unchanged
  metKeepDay(tomorrow, MetForecast());
  TEST_ASSERT_EQUAL_FLOAT(10.2f, tomorrow.tMin);
}

static void test_next_request() {
  time_t expires = 0;
  TEST_ASSERT_TRUE(parseHttpDate("Sun, 25 Oct 2026 11:28:00 GMT", &expires));
  // Expires before the hour is up: an hour, then the panel's second (20:34)
  TEST_ASSERT_EQUAL(utc("2026-10-25T12:20:34Z") - ASKED, metNextDelay(ASKED, expires, 1234));
  // Its second comes before the hour is up: the next one
  TEST_ASSERT_EQUAL(utc("2026-10-25T13:05:00Z") - ASKED, metNextDelay(ASKED, expires, 300));
  // Expires later than an hour: never before it
  TEST_ASSERT_TRUE(parseHttpDate("Sun, 25 Oct 2026 12:30:00 GMT", &expires));
  TEST_ASSERT_EQUAL(utc("2026-10-25T13:20:34Z") - ASKED, metNextDelay(ASKED, expires, 1234));
  TEST_ASSERT_EQUAL(utc("2026-10-25T12:31:40Z") - ASKED, metNextDelay(ASKED, expires, 1900));
  TEST_ASSERT_EQUAL(utc("2026-10-25T12:30:00Z") - ASKED, metNextDelay(ASKED, expires, 1800));
  // No Expires, or no Date: the hour floor holds
  TEST_ASSERT_EQUAL(utc("2026-10-25T12:20:34Z") - ASKED, metNextDelay(ASKED, 0, 1234));
  TEST_ASSERT_EQUAL(3600, metNextDelay(0, expires, 1234));
  // An Expires a day away (a server's slip) waits 6 h at most, then the slot
  TEST_ASSERT_EQUAL(utc("2026-10-25T17:20:34Z") - ASKED,
                    metNextDelay(ASKED, ASKED + 86400, 1234));
  // Every delay lies within an hour and two
  for (uint16_t slot = 0; slot < 3600; slot += 7) {
    const uint32_t d = metNextDelay(ASKED, 0, slot);
    TEST_ASSERT_TRUE(d >= 3600 && d < 7200);
    TEST_ASSERT_EQUAL(slot, (ASKED + d) % 3600);
  }
}

static void test_each_panel_its_second() {
  const uint8_t a[6] = {0x24, 0x6F, 0x28, 0x01, 0x69, 0x5C};
  const uint8_t b[6] = {0x24, 0x6F, 0x28, 0x01, 0x69, 0x5D};
  TEST_ASSERT_EQUAL(metSlot(a), metSlot(a));
  TEST_ASSERT_NOT_EQUAL(metSlot(a), metSlot(b));
  // Spread over the hour: 1000 panels in a row fill most of its minutes
  bool minute[60] = {};
  uint8_t m[6] = {0x24, 0x6F, 0x28, 0, 0, 0};
  for (int i = 0; i < 1000; i++) {
    m[4] = (uint8_t)(i >> 8);
    m[5] = (uint8_t)i;
    const uint16_t s = metSlot(m);
    TEST_ASSERT_TRUE(s < 3600);
    minute[s / 60] = true;
  }
  int used = 0;
  for (bool u : minute) used += u;
  TEST_ASSERT_TRUE(used >= 55);
}

// Panels started together (a power cut in a street, a crash loop) do not
// ask at the same second: the first request waits for the panel's second
// of the minute, within a minute since its Date may set the clock
static void test_first_request_after_a_start() {
  TEST_ASSERT_EQUAL_UINT32(0, metFirstDelay(0));
  TEST_ASSERT_EQUAL_UINT32(59, metFirstDelay(59));
  TEST_ASSERT_EQUAL_UINT32(1, metFirstDelay(61));
  TEST_ASSERT_EQUAL_UINT32(59, metFirstDelay(3599));
  const uint8_t a[6] = {0x24, 0x6F, 0x28, 0x01, 0x02, 0x03}, b[6] = {0x24, 0x6F, 0x28, 0x01, 0x02, 0x04};
  TEST_ASSERT_NOT_EQUAL(metFirstDelay(metSlot(a)), metFirstDelay(metSlot(b)));
}

static void test_backoff() {
  TEST_ASSERT_EQUAL(3600, metRetryDelay(429, 1));
  TEST_ASSERT_EQUAL(7200, metRetryDelay(503, 2));
  TEST_ASSERT_EQUAL(14400, metRetryDelay(500, 3));
  TEST_ASSERT_EQUAL(21600, metRetryDelay(500, 4));     // capped at 6 h
  TEST_ASSERT_EQUAL(21600, metRetryDelay(429, 200));
  TEST_ASSERT_EQUAL(3600, metRetryDelay(403, 1));      // refused: no asking again soon
  TEST_ASSERT_EQUAL(600, metRetryDelay(-1, 1));        // no answer
  TEST_ASSERT_EQUAL(1200, metRetryDelay(-11, 2));
  TEST_ASSERT_EQUAL(3600, metRetryDelay(-1, 4));       // capped at the hour
  TEST_ASSERT_EQUAL(3600, metRetryDelay(-1, 255));
  TEST_ASSERT_EQUAL(600, metRetryDelay(-1, 0));
}

int main() {
  // The panel's zone, as Paris has it: UTC+1, UTC+2 in summer
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  UNITY_BEGIN();
  RUN_TEST(test_symbols);
  RUN_TEST(test_request);
  RUN_TEST(test_answer_at_noon);
  RUN_TEST(test_answer_in_the_evening);
  RUN_TEST(test_without_a_date);
  RUN_TEST(test_answer_with_holes);
  RUN_TEST(test_answer_cut_anywhere);
  RUN_TEST(test_scalar_at_the_root);
  RUN_TEST(test_not_modified);
  RUN_TEST(test_the_day_is_kept);
  RUN_TEST(test_next_request);
  RUN_TEST(test_each_panel_its_second);
  RUN_TEST(test_first_request_after_a_start);
  RUN_TEST(test_backoff);
  return UNITY_END();
}
