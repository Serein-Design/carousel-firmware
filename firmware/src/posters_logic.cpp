// SPDX-License-Identifier: AGPL-3.0-only
#include "posters.h"

#include <stdio.h>
#include <string.h>

#include "screen_style.h"

namespace {
#define POSTER_INDEX(name, draw, orientation, leave, settings) IDX_##name,
enum ScreenIndex : uint8_t { SCREEN_TABLE(POSTER_INDEX) };
#undef POSTER_INDEX

uint8_t styleCount(uint8_t screen) {
  switch (screen) {
    case IDX_scrCode:    return CODE_STYLE_COUNT;
    case IDX_scrClock:   return CLOCK_STYLE_COUNT;
    case IDX_scrWeather: return WEATHER_STYLE_COUNT;
    default:             return 0;
  }
}
}  // namespace

int posterFor(uint8_t screen, uint8_t style) {
  if (style >= styleCount(screen)) return screen;
  if (screen == IDX_scrCode && style == CODE_AUTO) style = CODE_CLOUD;
  char name[24];
  snprintf(name, sizeof(name), "%s:%u", screenNameEn(screen), (unsigned)style);
  for (int i = POSTER_NO_IMAGE + 1; i < POSTER_COUNT; i++)
    if (!strcmp(POSTER_NAMES[i], name)) return i;
  return screen;
}
