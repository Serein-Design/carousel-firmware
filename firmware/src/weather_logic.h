// SPDX-License-Identifier: AGPL-3.0-only
// What WEATHER says, without the hardware: the conditions it names, the
// temperatures as text that fits, and the hours ahead it shows. The forecast
// itself comes from met_logic. Host-tested in test_weather.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "region.h"
#include "type.h"

// The conditions the panel names, in the order of Strings::wxCond (MET
// Norway's symbols, metCondition()). Rain and snow of every strength share
// one word: the icon and the temperature carry the rest.
enum WxCond : uint8_t {
  WX_CLEAR,           // clearsky
  WX_MOSTLY_CLEAR,    // fair
  WX_PARTLY,          // partlycloudy
  WX_OVERCAST,        // cloudy
  WX_FOG,             // fog
  WX_DRIZZLE,         // lightrain
  WX_FREEZING,        // sleet of every strength, showers too
  WX_RAIN,            // rain, heavyrain
  WX_SHOWERS,         // rain showers
  WX_SNOW,            // snow, snow showers
  WX_STORM,           // anything "andthunder"
  WX_COND_COUNT,
  WX_UNKNOWN = WX_COND_COUNT   // a symbol MET has not documented: no word
};

// The current hour and the 11 after it: W3 shows now, +3, +6 and +9 h, and
// the 2 spare hours keep the +9 h column while the hour turns between two
// answers (about an hour apart, a 304 shortens the run: metAdvance) or after
// a failed one. Fixed size: later hours of the answer are not kept.
static const uint8_t WEATHER_HOURS = 12;

struct HourlyForecast {
  uint8_t count = 0;                // hours in a row from now; 0: none
  time_t  start = 0;                // UTC start of the first one
  uint8_t hour[WEATHER_HOURS];      // local hour, 0-23
  float   temp[WEATHER_HOURS];
  WxCond  cond[WEATHER_HOURS];
  bool    day[WEATHER_HOURS];       // false: the symbol says night
};

// The hours W3 shows after now (+3, +6, +9 h) that the forecast holds, as
// indexes into it; returns how many (0-3). nowHour: the local hour, or -1
// when the clock is not set (the forecast's first hour stands for now).
uint8_t daySlots(const HourlyForecast &h, int nowHour, uint8_t idx[3]);

// A reading (MET gives °C) in the owner's unit (TempUnit); a unit
// out of range keeps °C, the default
float tempIn(float celsius, uint8_t unit);

// Whole degrees, never "-0"
int roundTemp(float t);

// The temperature as wide as it fits maxW in face f: "-12 °" (Pierre only),
// then "-12°", then "-12". Returns false if not even the number fits.
bool formatTemp(char *out, size_t size, float t, Face f, uint16_t maxW);

// "9° 16°": the day's minimum then maximum, in half the label line at most
// so the place keeps the rest; two °F readings past 99 drop the degrees
static const uint16_t WX_MINMAX_W = 32;
void formatMinMax(char *out, size_t size, float tMin, float tMax);

// Cuts text, whole characters at a time, until it fits maxW in face f
void fitText(char *text, Face f, uint16_t maxW);

// A word on two lines of maxW in face f: at a space when both halves fit,
// otherwise as much as fits with a hyphen, the rest below (cut to fit).
// second is empty when the word fits on one line.
void splitLines(const char *word, Face f, uint16_t maxW, char *first, char *second,
                size_t size);

// The place's name as the label line shows it: the first part of the found
// place ("Paris, Île-de-France, France"), else the city as typed
void placeName(const char *place, const char *city, char *out, size_t size);
