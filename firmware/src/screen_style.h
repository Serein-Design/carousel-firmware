// SPDX-License-Identifier: AGPL-3.0-only
// How the screens that offer a choice draw: the owner picks a style per
// screen on Running order (stored with the settings), the default first.
#pragma once

#include <stdint.h>

enum CodeStyle : uint8_t {
  CODE_AUTO,     // the cloud with one session, the list from three
  CODE_CLOUD,    // the mascot, the state in a word, the project
  CODE_LIST,     // one row per session
  CODE_STYLE_COUNT
};

enum ClockStyle : uint8_t {
  CLOCK_STONE,   // big digits, the date above, the seconds as a line
  CLOCK_DIAL,    // an analogue dial, the date beside it
  CLOCK_CYCLE,   // big digits, one foot line taking turns
  CLOCK_STYLE_COUNT
};

// What CLOCK shows besides the time, as bits
enum ClockShow : uint8_t {
  CLOCK_SHOW_DATE    = 1 << 0,
  CLOCK_SHOW_SECONDS = 1 << 1,   // the seconds line
  CLOCK_SHOW_WEEK    = 1 << 2,
  CLOCK_SHOW_OUTSIDE = 1 << 3,   // the outside temperature
};
constexpr uint8_t CLOCK_SHOW_DEFAULT = CLOCK_SHOW_DATE | CLOCK_SHOW_SECONDS;
constexpr uint8_t CLOCK_SHOW_ALL = 0x0F;

enum WeatherStyle : uint8_t {
  WEATHER_GLANCE,   // icon, temperature, condition, min and max
  WEATHER_SCENE,    // the illustrated scene, the temperature small
  WEATHER_DAY,      // now and the next three slots
  WEATHER_STYLE_COUNT
};
