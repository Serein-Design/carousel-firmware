// SPDX-License-Identifier: AGPL-3.0-only
// What CLOCK shows besides the time, and where: the readouts settings.clockShow
// asks for, their text in the panel's language, the slots each style gives
// them, and the take-turns timing of the cycle. Hardware-free: host-tested in
// test_clock_logic; screens/clock.cpp draws from it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "i18n.h"

// The readouts besides the time and the seconds line, in the order they show
enum ClockReadout : uint8_t { READ_DATE, READ_WEEK, READ_OUTSIDE, READ_COUNT };

// The readouts show (ClockShow bits) asks for, in order, into out; the
// outside temperature only while the weather is known. Returns how many.
uint8_t clockReadouts(uint8_t show, bool outsideKnown, ClockReadout out[READ_COUNT]);

// The date as the label line shows it: "MON 19 OCT" / "LUN. 19 OCT."
void clockDateLine(char *out, size_t size, const Strings &t, int wday, int mday, int mon);
// The day and month alone, for portrait: "19 OCT." (a narrow space between,
// so the longest French month fits 28 px)
void clockDayMonth(char *out, size_t size, const Strings &t, int mday, int mon);
// "WEEK 43" / "SEMAINE 43"; tall: "W43" / "S43", for portrait
void clockWeekLine(char *out, size_t size, const Strings &t, int week, bool tall = false);
// "14°", rounded to the degree; degrees already in the owner's unit (tempIn)
void clockDegrees(char *out, size_t size, float degrees);
// "OUTSIDE 14°" / "DEHORS 14°"
void clockOutsideLine(char *out, size_t size, const Strings &t, float degrees);

// --- 12 h (settings.clock12) ----------------------------------------------------
// The hour on a 12 h clock, 1-12: midnight is 12 AM, noon 12 PM
struct Hour12 { uint8_t hour; bool pm; };
Hour12 hour12(int hour24);
// The time in Pierre: "09:05" in 24 h, "9:05" in 12 h (no leading zero)
void clockTime(char *out, size_t size, int hour, int min, bool h12);
// The hours alone, portrait's top line: "09" / "9"
void clockHours(char *out, size_t size, int hour, bool h12);
// "AM" or "PM", for the label face beside the time
const char *clockAmPm(const Strings &t, int hour24);
// The time and, in 12 h, its AM/PM beside the top of the figures, the whole
// centred on mid. timeW: the time's width in Pierre (portrait: the column of
// hours over minutes); ampm: nullptr in 24 h. Portrait leaves 1 px between
// them, the most 28 px allows beside two figures; landscape 2.
struct TimePlace { int16_t timeX, ampmX, width; };
TimePlace placeTime(uint16_t timeW, const char *ampm, int16_t mid, bool tall);
// WEATHER's hour columns: "15H"; 12 h: "3P", "12A"
void clockHourLabel(char *out, size_t size, const Strings &t, int hour24, bool h12);

// Where Pierre (C1) puts the readouts: the date on the label line; the week
// and the outside temperature on the foot line, or on the label line when the
// date is hidden. The seconds line takes the foot row, or rises to row 23
// when the foot holds text. -1: nothing there.
struct StoneLayout {
  bool   dateOnLabel;    // the date fills the label line
  bool   extrasOnLabel;  // the week and outside temperature fill it instead
  bool   extrasOnFoot;
  int8_t secondsRow;
};
StoneLayout stoneLayout(uint8_t show, bool outsideKnown);

// The cycle (C3, and the dial's column with several readouts): each readout
// shows 4 s, then the next one pushes it up in 300 ms. now and before index
// the readouts; shift is how far (px, of distance) the push still has to go,
// 0 once settled. One readout or none: it stays, nothing moves.
static const uint32_t CLOCK_CYCLE_MS = 4000;
static const uint32_t CLOCK_PUSH_MS  = 300;
struct CycleStep { uint8_t now, before, shift; };
CycleStep clockCycle(uint32_t ms, uint8_t count, uint8_t distance);
