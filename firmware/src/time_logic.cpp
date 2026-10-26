// SPDX-License-Identifier: AGPL-3.0-only
#include "time_logic.h"

#include <string.h>

const char *timeSourceName(TimeSource s) {
  switch (s) {
    case TIME_ROUTER:    return "router";
    case TIME_POOL:      return "pool";
    case TIME_HTTP_DATE: return "http-date";
  }
  return "?";
}

NtpServer ntpServerFor(uint32_t window, bool haveRouter) {
  if (haveRouter) return (NtpServer)(window % 3);
  return (NtpServer)(NTP_PUBLIC_1 + window % 2);
}

uint32_t ntpWindowMs(uint32_t window, bool timeKnown) {
  if (timeKnown) return 15000;
  return window == 0 ? 2000 : 5000;
}

static const char MONTHS[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

// "Oct" -> 0..11, -1 if no month
static int monthIndex(const char *s) {
  for (int m = 0; m < 12; m++)
    if (strncmp(s, MONTHS + 3 * m, 3) == 0) return m;
  return -1;
}

// n digits at s, -1 if one is not a digit
static int digits(const char *s, int n) {
  int v = 0;
  for (int i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return -1;
    v = v * 10 + (s[i] - '0');
  }
  return v;
}

static bool leapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static int daysInMonth(int y, int m) {
  static const uint8_t DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 1 && leapYear(y) ? 29 : DAYS[m];
}

// Days from 1970-01-01 to a civil date (H. Hinnant's days_from_civil): no
// timegm() needed, which newlib lacks, and no time zone involved.
static int64_t daysFromCivil(int y, int m, int d) {   // m 1..12
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// "Sun, 06 Nov 1994 08:49:37 GMT": 29 characters, every one in its place
bool parseHttpDate(const char *s, time_t *out) {
  if (!s || strlen(s) != 29) return false;
  if (s[3] != ',' || s[4] != ' ' || s[7] != ' ' || s[11] != ' ' || s[16] != ' ' ||
      s[19] != ':' || s[22] != ':' || s[25] != ' ' || strcmp(s + 26, "GMT") != 0)
    return false;
  const int day = digits(s + 5, 2), mon = monthIndex(s + 8), year = digits(s + 12, 4);
  const int h = digits(s + 17, 2), mi = digits(s + 20, 2), sec = digits(s + 23, 2);
  if (mon < 0 || year < 1970 || day < 1 || day > daysInMonth(year, mon) || h < 0 || h > 23 ||
      mi < 0 || mi > 59 || sec < 0 || sec > 60)   // 60: a leap second
    return false;
  *out = (time_t)(daysFromCivil(year, mon + 1, day) * 86400 + h * 3600 + mi * 60 + sec);
  return true;
}

// "2026-10-25T11:00:00Z": 20 characters, every one in its place
bool parseIsoUtc(const char *s, time_t *out) {
  if (!s || strlen(s) != 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
      s[16] != ':' || s[19] != 'Z')
    return false;
  const int year = digits(s, 4), mon = digits(s + 5, 2), day = digits(s + 8, 2);
  const int h = digits(s + 11, 2), mi = digits(s + 14, 2), sec = digits(s + 17, 2);
  if (year < 1970 || mon < 1 || mon > 12 || day < 1 || day > daysInMonth(year, mon - 1) ||
      h < 0 || h > 23 || mi < 0 || mi > 59 || sec < 0 || sec > 60)
    return false;
  *out = (time_t)(daysFromCivil(year, mon, day) * 86400 + h * 3600 + mi * 60 + sec);
  return true;
}

// "Oct 23 2026" or "Oct  3 2026"
time_t buildDay(const char *date) {
  if (!date || strlen(date) != 11 || date[3] != ' ' || date[6] != ' ') return 0;
  const int mon = monthIndex(date);
  const int day = date[4] == ' ' ? digits(date + 5, 1) : digits(date + 4, 2);
  const int year = digits(date + 7, 4);
  if (mon < 0 || year < 1970 || day < 1 || day > daysInMonth(year, mon)) return 0;
  return (time_t)(daysFromCivil(year, mon + 1, day) * 86400);
}

bool timePlausible(time_t now, time_t built) { return now >= built; }
