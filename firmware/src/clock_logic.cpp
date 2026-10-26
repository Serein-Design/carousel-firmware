// SPDX-License-Identifier: AGPL-3.0-only
#include "clock_logic.h"

#include <math.h>
#include <stdio.h>

#include "ease.h"
#include "type.h"
#include "screen_style.h"

uint8_t clockReadouts(uint8_t show, bool outsideKnown, ClockReadout out[READ_COUNT]) {
  uint8_t n = 0;
  if (show & CLOCK_SHOW_DATE) out[n++] = READ_DATE;
  if (show & CLOCK_SHOW_WEEK) out[n++] = READ_WEEK;
  if ((show & CLOCK_SHOW_OUTSIDE) && outsideKnown) out[n++] = READ_OUTSIDE;
  return n;
}

void clockDateLine(char *out, size_t size, const Strings &t, int wday, int mday, int mon) {
  snprintf(out, size, "%s %d %s", t.clkDays[wday % 7], mday, t.clkMonths[mon % 12]);
}

void clockDayMonth(char *out, size_t size, const Strings &t, int mday, int mon) {
  snprintf(out, size, "%d\xE2\x80\xAF%s", mday, t.clkMonths[mon % 12]);
}

void clockWeekLine(char *out, size_t size, const Strings &t, int week, bool tall) {
  if (tall) snprintf(out, size, "%s%d", t.weekPrefix, week);
  else      snprintf(out, size, "%s %d", t.clkWeek, week);
}

void clockDegrees(char *out, size_t size, float degrees) {
  // lroundf(-0.4) is 0: no "-0°"
  snprintf(out, size, "%ld\xC2\xB0", lroundf(degrees));
}

void clockOutsideLine(char *out, size_t size, const Strings &t, float degrees) {
  char deg[12];
  clockDegrees(deg, sizeof(deg), degrees);
  snprintf(out, size, "%s %s", t.clkOutside, deg);
}

Hour12 hour12(int hour24) {
  const int h = hour24 % 12;
  return {(uint8_t)(h ? h : 12), hour24 >= 12};
}

void clockTime(char *out, size_t size, int hour, int min, bool h12) {
  if (h12) snprintf(out, size, "%u:%02d", hour12(hour).hour, min);
  else snprintf(out, size, "%02d:%02d", hour, min);
}

void clockHours(char *out, size_t size, int hour, bool h12) {
  if (h12) snprintf(out, size, "%u", hour12(hour).hour);
  else snprintf(out, size, "%02d", hour);
}

const char *clockAmPm(const Strings &t, int hour24) { return hour12(hour24).pm ? t.clkPm : t.clkAm; }

TimePlace placeTime(uint16_t timeW, const char *ampm, int16_t mid, bool tall) {
  const int16_t gap = tall ? 1 : 2;
  const int16_t width = ampm ? timeW + gap + textWidth(ampm, Face::LABEL) : timeW;
  const int16_t left = mid - (width >> 1);   // drawText's centring: the odd pixel right
  return {left, (int16_t)(left + timeW + gap), width};
}

void clockHourLabel(char *out, size_t size, const Strings &t, int hour24, bool h12) {
  if (!h12) {
    snprintf(out, size, t.wxHour, (unsigned)hour24);
    return;
  }
  const Hour12 h = hour12(hour24);
  snprintf(out, size, h.pm ? t.wxHourPm : t.wxHourAm, (unsigned)h.hour);
}

StoneLayout stoneLayout(uint8_t show, bool outsideKnown) {
  const bool date = show & CLOCK_SHOW_DATE;
  const bool extras = (show & CLOCK_SHOW_WEEK) || ((show & CLOCK_SHOW_OUTSIDE) && outsideKnown);
  StoneLayout l;
  l.dateOnLabel = date;
  l.extrasOnLabel = extras && !date;
  l.extrasOnFoot = extras && date;
  l.secondsRow = (show & CLOCK_SHOW_SECONDS) ? (l.extrasOnFoot ? 23 : 25) : -1;
  return l;
}

CycleStep clockCycle(uint32_t ms, uint8_t count, uint8_t distance) {
  if (count < 2) return {0, 0, 0};
  const uint32_t turn = ms / CLOCK_CYCLE_MS, phase = ms % CLOCK_CYCLE_MS;
  const uint8_t now = turn % count;
  const uint8_t before = (now + count - 1) % count;
  if (phase >= CLOCK_PUSH_MS) return {now, before, 0};
  const float k = easeOut((float)phase / CLOCK_PUSH_MS);
  return {now, before, (uint8_t)(distance - lroundf(k * distance))};
}
