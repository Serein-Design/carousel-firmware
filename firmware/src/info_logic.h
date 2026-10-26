// SPDX-License-Identifier: AGPL-3.0-only
// What DATE, SYSTEM, MONEY and SOCIAL say and in which face, without the
// drawing: numbers grouped by threes, the face a number takes in its line,
// the uptime and the signal, the day's earnings, which counters show.
// Hardware-free: host-tested in test_info_logic; screens/info.cpp draws.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "i18n.h"
#include "region.h"
#include "type.h"

// The room between two things on a line (a number and its unit, two columns)
static const int16_t INFO_GAP = 3;

// v with its digits grouped by threes, a narrow no-break space (U+202F)
// between groups: "12 345". Every face draws it, Pierre too (no comma there).
void groupDigits(uint32_t v, char *out, size_t size);

// The face a number takes on a line width px wide: Pierre while it fits,
// then the body face, then the label face (which may still not fit)
Face numberFace(const char *text, uint16_t width);

// The top row that centres face f's capitals between rows top and bottom
int16_t faceTop(Face f, int16_t top, int16_t bottom);

// --- DATE ------------------------------------------------------------------------
// The day of the year (1-366): "DAY 292" / "JOUR 292"; tall: "D292" / "J292"
void dateDayLine(char *out, size_t size, const Strings &t, int yday1, bool tall);

// --- SYSTEM ----------------------------------------------------------------------
// The Wi-Fi signal in bars, 0 (offline) to 4; drawn 2 px wide, 1 px apart
uint8_t signalBars(bool connected, int rssi);
static const int16_t SIGNAL_BARS_W = 11;
// How long the panel has run: "42 MIN", "3H05", "12D 04H" (FR "12J 04H"), and
// from 100 days the days alone, "123D"
void uptimeText(char *out, size_t size, const Strings &t, uint32_t seconds);
// The signal as it shows, in dBm: down to -99, as below that there is no link
int signalDbm(int rssi);
// The free heap as a share of the whole, 0-99: the firmware always holds some
uint8_t ramPercent(uint32_t freeBytes, uint32_t totalBytes);
// Below it, the share shows in the alert colour (with its number: never colour alone)
static const uint8_t RAM_LOW_PERCENT = 15;

// --- MONEY -----------------------------------------------------------------------
struct MoneyTerms {
  float   salary;          // net, a month
  int     days;            // working days a month
  int     start, end;      // the working hours, [start, end)
};
struct MoneyDay {
  float today;             // earned so far today
  float month;             // earned so far this month
  float progress;          // 0..1 of today's working hours gone
  bool  working;           // within the working hours
};
// nowSec: seconds since midnight; mday: the day of the month (1-31). A day
// or month of 0 (NVS may hold one) counts as 1, never a division by zero.
MoneyDay moneyDay(const MoneyTerms &m, int32_t nowSec, int mday);
// An amount as the panel writes it in settings.currency (Currency; out of
// range: euros): $ and £ before it, tight ("$123"); € and CHF after it, with
// the language's space ("123,45 €", "123.45 CHF"); the decimal mark the
// language's. The symbols are the same in every language: not in i18n.
struct MoneyText {
  char before[8];    // the symbol written before ("$", "£"), or ""
  char units[24];    // the whole units, grouped: "12 345"
  char after[24];    // the cents and the symbol written after: ".45", ",45 €"
};
void moneyAmount(MoneyText &out, const Strings &t, uint8_t currency, float v);
// The room the whole units have on a line width px wide: beside the symbol
// before them and, in landscape, the cents after them (portrait puts the
// cents on the next line)
uint16_t moneyRoom(const MoneyText &m, uint16_t width, bool tall);
// A whole amount, grouped, with its symbol: "2 345 €", "$2 345". Wider than
// maxW in the label face, the symbol is left out (the amount above has it).
void moneyTotal(char *out, size_t size, const Strings &t, uint8_t currency, float v,
                uint16_t maxW = UINT16_MAX);

// --- SOCIAL ----------------------------------------------------------------------
enum SocialNet : uint8_t { SOCIAL_YT, SOCIAL_IG, SOCIAL_X, SOCIAL_GH, SOCIAL_COUNT };
// The counters to show, in order, into out: those with a number or a live
// source; none of them: all four, as zeros say there is something to set
uint8_t socialShown(const uint32_t value[SOCIAL_COUNT], const bool live[SOCIAL_COUNT],
                    uint8_t out[SOCIAL_COUNT]);
// The foot's pager: a 2 px dash per counter, 1 px apart; none for one counter
int16_t socialPagerWidth(uint8_t count);
// A count as its line width px wide shows it, and its face: grouped in the
// first face it fits (numberFace), else in millions: a tenth kept below 100
// ("12.3M" / "12,3 M"), whole above ("4294M")
Face socialCount(char *out, size_t size, const Strings &t, uint32_t v, uint16_t width);
