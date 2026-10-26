// SPDX-License-Identifier: AGPL-3.0-only
// WEATHER in the owner's style (screen_style.h, chosen on Running order):
// W1 glance (icon, temperature, the condition, min and max), W2 scene
// (today's sky, calmed, the place and temperature in a black foot band), W3
// day (now and +3, +6, +9 h). Portrait has room for W1 only: the other two
// draw it there. The art keeps its natural colours; words and numbers take
// the palette's roles and the grid's rows.
#include <WiFi.h>

#include "clock_logic.h"
#include "display.h"
#include "gallery.h"
#include "grid.h"
#include "i18n.h"
#include "network.h"
#include "palette.h"
#include "screen_style.h"
#include "screens/screens.h"
#include "settings.h"
#include "type.h"
#include "weather.h"
#include "weather_logic.h"

namespace {

// --- Content colours, as the board gives them --------------------------------
struct Rgb { uint8_t r, g, b; };
const Rgb SUN = {255, 194, 58}, SUN_CORE = {255, 224, 122}, CLOUD = {201, 210, 220},
          CLOUD_SHADE = {143, 156, 171}, RAIN = {76, 143, 232}, SNOW = {230, 236, 245},
          BOLT = {255, 220, 60}, GRASS = {62, 140, 58}, HOUSE = {190, 90, 60};

uint16_t c565(Rgb c, float k = 1) {
  return Frame::color565((uint8_t)(c.r * k), (uint8_t)(c.g * k), (uint8_t)(c.b * k));
}

Rgb mix(Rgb a, Rgb b, float k) {
  return {(uint8_t)(a.r + (b.r - a.r) * k), (uint8_t)(a.g + (b.g - a.g) * k),
          (uint8_t)(a.b + (b.b - a.b) * k)};
}

// '#' in c, 's' in shade, anything else left alone
void sprite(const char *const *rows, int n, int x, int y, uint16_t c, uint16_t shade) {
  for (int j = 0; j < n; j++)
    for (int i = 0; rows[j][i]; i++) {
      if (rows[j][i] == '#') display->drawPixel(x + i, y + j, c);
      else if (rows[j][i] == 's') display->drawPixel(x + i, y + j, shade);
    }
}

const char *const CLOUD_BIG[] = {"...####....", "..######.##", ".##########", "###########",
                                 "###########", ".sssssssss."};
const char *const CLOUD_SMALL[] = {"..####...", ".#######.", "#########", ".sssssss."};
const char *const CLOUD_MINI[] = {"..###..", ".#####.", "#######", ".sssss."};
const char *const MOON[] = {"..##", ".##.", "##..", "##..", "##..", ".##.", "..##"};
const char *const MOON_MINI[] = {".##", "##.", "##.", ".##"};
const char *const BOLT_SHAPE[] = {"..#", ".#.", "###", ".#.", "#.."};
const char *const HOUSE_SHAPE[] = {"...#...", "..###..", ".#####.", "#######", ".#####.",
                                   ".##.##.", ".##.##."};

void cloud(int x, int y, bool dark = false) {
  sprite(CLOUD_BIG, 6, x, y, c565(dark ? CLOUD_SHADE : CLOUD),
         c565(CLOUD_SHADE, dark ? 0.6f : 1));
}

// A disc with a lighter core and 8 rays turning once every 25 s
void sun(int cx, int cy, int r, uint32_t ms) {
  for (int j = -r; j <= r; j++)
    for (int i = -r; i <= r; i++)
      if (i * i + j * j <= r * r + r)
        display->drawPixel(cx + i, cy + j,
                           c565(i * i + j * j < (r - 1) * (r - 1) ? SUN_CORE : SUN));
  const float a0 = ms / 4000.0f;
  for (int k = 0; k < 8; k++) {
    const float a = a0 + k * PI / 4;
    display->drawPixel(cx + (int)lroundf(cosf(a) * (r + 2)), cy + (int)lroundf(sinf(a) * (r + 2)),
                       c565(SUN, 0.8f));
  }
}

// Falling drops or flakes under a cloud: n of them, step px apart
void fall(int x, int y, int n, int step, int depth, uint32_t ms, uint32_t period, Rgb c) {
  for (int k = 0; k < n; k++) {
    const int ph = (int)((ms / period + k * 5) % depth);
    display->drawPixel(x + k * step, y + ph, c565(c));
  }
}

// The condition's icon, 16x12 from (x, y), as the board draws W1's
void weatherIcon(int x, int y, WxCond c, bool day, uint32_t ms) {
  switch (c) {
    case WX_CLEAR:
      if (day) sun(x + 7, y + 6, 3, ms);
      else sprite(MOON, 7, x + 6, y + 2, c565(SUN_CORE), 0);
      break;
    case WX_MOSTLY_CLEAR:
    case WX_PARTLY:
      if (day) sun(x + 5, y + 4, 3, ms);
      else sprite(MOON, 7, x + 4, y + 1, c565(SUN_CORE), 0);
      cloud(x + 4, y + 5);
      break;
    case WX_FOG:
      for (int i = 0; i < 4; i++) {
        const int shift = (int)(sinf(ms / 1200.0f + i) * 2);
        display->drawFastHLine(x + 2 + shift, y + 2 + i * 3, 12, c565(CLOUD_SHADE));
      }
      break;
    case WX_DRIZZLE:
      cloud(x + 2, y + 1);
      fall(x + 5, y + 7, 2, 5, 5, ms, 120, RAIN);
      break;
    case WX_FREEZING:
    case WX_RAIN:
    case WX_SHOWERS:
      cloud(x + 2, y + 1);
      fall(x + 4, y + 7, 4, 3, 5, ms, 90, RAIN);
      break;
    case WX_SNOW:
      cloud(x + 2, y + 1);
      fall(x + 4, y + 7, 4, 3, 5, ms, 250, SNOW);
      break;
    case WX_STORM:
      cloud(x + 2, y + 1, true);
      sprite(BOLT_SHAPE, 5, x + 6, y + 7, c565(BOLT), 0);
      break;
    default:   // overcast, and a code without a word
      cloud(x + 1, y + 2, true);
      cloud(x + 4, y + 5);
  }
}

// W3's small icon, about 8x8 from (x, y); still: four of them would be busy
void miniIcon(int x, int y, WxCond c, bool day) {
  const bool clearish = c == WX_CLEAR || c == WX_MOSTLY_CLEAR || c == WX_PARTLY;
  if (clearish) {
    const int dx = c == WX_CLEAR ? 0 : -1;
    if (day) {
      for (int j = -2; j <= 2; j++)
        for (int i = -2; i <= 2; i++)
          if (i * i + j * j <= 5) display->drawPixel(x + 4 + i + dx, y + 3 + j + dx, c565(SUN));
    } else {
      sprite(MOON_MINI, 4, x + 3 + dx, y + 1 + dx, c565(SUN_CORE), 0);
    }
    if (c == WX_CLEAR) return;
  }
  if (c == WX_FOG) {
    for (int i = 0; i < 3; i++) display->drawFastHLine(x + i % 2, y + 2 + i * 2, 8, c565(CLOUD_SHADE));
    return;
  }
  const bool dark = c == WX_STORM || c == WX_OVERCAST;
  sprite(CLOUD_MINI, 4, x + 1, y + 2, c565(dark ? CLOUD_SHADE : CLOUD), c565(CLOUD_SHADE, 0.7f));
  const Rgb drop = c == WX_SNOW ? SNOW : c == WX_STORM ? BOLT : RAIN;
  if (c == WX_STORM) {
    display->drawPixel(x + 4, y + 6, c565(drop));
    display->drawPixel(x + 3, y + 7, c565(drop));
  } else if (!clearish && c != WX_OVERCAST && c != WX_UNKNOWN) {
    display->drawPixel(x + 2, y + 7, c565(drop));
    display->drawPixel(x + 5, y + 7, c565(drop));
  }
}

// A reading in the owner's unit (°C or °F)
float deg(float celsius) { return tempIn(celsius, settings.tempUnit); }

// --- W1, the glance -----------------------------------------------------------
void drawGlance(const Palette &p, uint32_t ms) {
  const WxCond c = weather.cond;
  char place[48], minMax[24], temp[16];
  placeName(settings.place, settings.city, place, sizeof(place));
  if (!portrait) {
    formatMinMax(minMax, sizeof(minMax), deg(weather.tMin), deg(weather.tMax));
    const uint16_t mmW = textWidth(minMax, Face::LABEL);
    drawText(*display, minMax, W() - GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.label,
             Align::RIGHT);
    fitText(place, Face::LABEL, GRID_LINE_W - mmW - 4);
    drawText(*display, place, GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.label);
    weatherIcon(GRID_MARGIN, GRID_MAIN_TOP, c, weather.day, ms);
    // Right of the icon: columns 19-61, centred there
    formatTemp(temp, sizeof(temp), deg(weather.temp), Face::PIERRE, 42);
    drawText(*display, temp, 40, GRID_MAIN_TOP, Face::PIERRE, p.text, Align::CENTER);
    if (c != WX_UNKNOWN)
      drawText(*display, tr().wxCond[c], W() / 2, GRID_FOOT_TOP, Face::LABEL, p.text,
               Align::CENTER);
    return;
  }
  // Portrait, as the board's grid: the place, the icon, the digits, the
  // condition on two label lines
  const int mid = W() / 2;
  fitText(place, Face::LABEL, GRID_PORTRAIT_LINE_W);
  drawText(*display, place, mid, GRID_PORTRAIT_LABEL_TOP, Face::LABEL, p.label, Align::CENTER);
  weatherIcon(mid - 8, 11, c, weather.day, ms);
  formatTemp(temp, sizeof(temp), deg(weather.temp), Face::PIERRE, GRID_PORTRAIT_LINE_W);
  drawText(*display, temp, mid, 27, Face::PIERRE, p.text, Align::CENTER);
  if (c == WX_UNKNOWN) return;
  char a[32], b[32];
  splitLines(tr().wxCond[c], Face::LABEL, GRID_PORTRAIT_LINE_W, a, b, sizeof(a));
  drawText(*display, a, mid, 46, Face::LABEL, p.text, Align::CENTER);
  drawText(*display, b, mid, 53, Face::LABEL, p.text, Align::CENTER);
}

// --- W2, the scene --------------------------------------------------------------
// The sky of the condition, calmed (75 %: the board's lower saturation)
void sky(WxCond c, bool day) {
  Rgb top, bottom;
  const bool clearish = c == WX_CLEAR || c == WX_MOSTLY_CLEAR || c == WX_PARTLY;
  if (!day)                 { top = {4, 6, 20};   bottom = {14, 22, 48}; }
  else if (c == WX_STORM)   { top = {30, 32, 42}; bottom = {62, 64, 76}; }
  else if (clearish)        { top = {22, 70, 140}; bottom = {70, 120, 180}; }
  else                      { top = {60, 68, 80}; bottom = {105, 112, 122}; }
  for (int y = 0; y < 22; y++)
    display->drawFastHLine(0, y, W(), c565(mix(top, bottom, y / 22.0f), 0.75f));
}

void drawScene(const Palette &p, uint32_t ms) {
  const WxCond c = weather.cond;
  const bool day = weather.day;
  sky(c, day);
  const bool clearish = c == WX_CLEAR || c == WX_MOSTLY_CLEAR || c == WX_PARTLY;
  if (clearish) {
    if (day) {
      for (int j = -3; j <= 3; j++)
        for (int i = -3; i <= 3; i++)
          if (i * i + j * j <= 10) display->drawPixel(50 + i, 6 + j, c565(SUN_CORE));
    } else {
      sprite(MOON, 7, 49, 3, c565(SUN_CORE), 0);
      // A few fixed stars: the clear night
      static const uint8_t STARS[][2] = {{6, 3}, {17, 8}, {29, 2}, {38, 11}, {58, 14}, {24, 15}};
      for (const auto &s : STARS) display->drawPixel(s[0], s[1], c565(SNOW, 0.5f));
    }
  }
  // Clouds drift 3 px/s across, each coming back round on its own
  const uint32_t drift = ms / 333;
  const int big = (int)(drift % 90) - 20, small = (int)((drift + 30) % 90) - 20,
            third = (int)((drift + 60) % 90) - 20;
  const bool grey = !clearish;
  const uint16_t body = c565(grey ? CLOUD_SHADE : CLOUD, day ? 1 : 0.4f);
  const uint16_t shade = c565(CLOUD_SHADE, day ? 0.7f : 0.3f);
  if (c != WX_CLEAR && c != WX_FOG) sprite(CLOUD_BIG, 6, big, 4, body, shade);
  if (c != WX_CLEAR && c != WX_MOSTLY_CLEAR && c != WX_FOG)
    sprite(CLOUD_SMALL, 4, small, 10, body, shade);
  if (grey && c != WX_FOG) sprite(CLOUD_SMALL, 4, third, 7, body, shade);
  switch (c) {
    case WX_DRIZZLE: fall(big + 2, 10, 4, 3, 11, ms, 110, RAIN); break;
    case WX_FREEZING:
    case WX_RAIN:
    case WX_SHOWERS:
      fall(big + 1, 10, 4, 3, 11, ms, 60, RAIN);
      fall(small + 1, 14, 3, 3, 7, ms, 60, RAIN);
      break;
    case WX_SNOW:
      fall(big + 1, 10, 4, 3, 11, ms, 200, SNOW);
      fall(small + 1, 14, 3, 3, 7, ms, 200, SNOW);
      break;
    case WX_STORM:
      fall(big + 1, 10, 4, 3, 11, ms, 60, RAIN);
      // A strike every 5 s, for 150 ms
      if (ms % 5000 < 150) sprite(BOLT_SHAPE, 5, small + 3, 14, c565(BOLT), 0);
      break;
    case WX_FOG:
      for (int y = 9; y < 22; y += 4)
        display->drawFastHLine(0, y + (int)(sinf(ms / 1500.0f + y) * 1.5f), W(),
                               c565(CLOUD_SHADE, day ? 0.6f : 0.25f));
      break;
    default: break;
  }
  const float ground = day ? 1 : 0.35f;
  display->drawFastHLine(0, 22, W(), c565(GRASS, ground));
  display->drawFastHLine(0, 23, W(), c565(GRASS, ground * 0.7f));
  sprite(HOUSE_SHAPE, 7, 8, 15, c565(HOUSE, day ? 1 : 0.5f), 0);
  // The foot band: text never sits on the art
  display->fillRect(0, 24, W(), H() - 24, p.off);
  char temp[16], place[48];
  formatTemp(temp, sizeof(temp), deg(weather.temp), Face::LABEL, 20);
  drawText(*display, temp, W() - GRID_MARGIN, GRID_FOOT_TOP, Face::LABEL, p.text, Align::RIGHT);
  placeName(settings.place, settings.city, place, sizeof(place));
  fitText(place, Face::LABEL, GRID_LINE_W - textWidth(temp, Face::LABEL) - 4);
  drawText(*display, place, GRID_MARGIN, GRID_FOOT_TOP, Face::LABEL, p.label);
}

// --- W3, the day ------------------------------------------------------------------
// One 16 px column: the hour, a small icon, the temperature
void dayColumn(const Palette &p, int x, int hour, float temp, WxCond cond, bool day, bool now) {
  char s[16];
  if (hour >= 0) {
    clockHourLabel(s, sizeof(s), tr(), hour, settings.clock12);
    drawText(*display, s, x + 8, GRID_LABEL_TOP, Face::LABEL, now ? p.accent : p.label,
             Align::CENTER);
  }
  // The now column says its hour too ("NOW" does not fit 14 px), underlined
  if (now) display->drawFastHLine(x + 2, GRID_LABEL_BOT + 2, 12, p.accent);
  miniIcon(x + 4, 11, cond, day);
  formatTemp(s, sizeof(s), deg(temp), Face::LABEL, 14);
  drawText(*display, s, x + 8, GRID_FOOT_TOP, Face::LABEL, p.text, Align::CENTER);
}

void drawDay(const Palette &p) {
  const HourlyForecast &h = weather.hourly;
  struct tm t;
  const int nowHour = timeValid && getLocalTime(&t, 0) ? t.tm_hour : -1;
  uint8_t idx[3];
  const uint8_t n = daySlots(h, nowHour, idx);
  const int hour = nowHour >= 0 ? nowHour : h.count ? h.hour[0] : -1;
  // Without the hours ahead, now alone, in the middle
  const int x0 = n ? 0 : (W() - 16) / 2;
  dayColumn(p, x0, hour, weather.temp, weather.cond, weather.day, true);
  for (uint8_t i = 0; i < n; i++) {
    const int x = (i + 1) * 16;
    display->drawFastVLine(x, 3, 26, p.ghost);
    const uint8_t k = idx[i];
    dayColumn(p, x, h.hour[k], h.temp[k], h.cond[k], h.day[k], false);
  }
}

}  // namespace

void screenWeather(uint32_t f, bool init) {
  (void)init;
  display->clearScreen();
  const Palette &p = palette();
  if (!weather.valid) {
    const char *m = WiFi.status() == WL_CONNECTED ? tr().weatherLoading : "WIFI";
    drawText(*display, m, W() / 2, H() / 2 - 3, Face::LABEL, p.label, Align::CENTER);
    drawNetworkStatus(f);
    return;
  }
  const uint32_t ms = millis();
  const uint8_t style = portrait ? (uint8_t)WEATHER_GLANCE : settings.weatherStyle;
  switch (style) {
    case WEATHER_SCENE: drawScene(p, ms); break;
    case WEATHER_DAY:   drawDay(p); break;
    default:            drawGlance(p, ms); break;
  }
  // No fresh reading: the last one stays, dimmed, with the reason (E1).
  // MET Norway asks for no key.
  const Reason why = serviceReason({weather.valid, weather.updatedAt,
                                    WiFi.status() == WL_CONNECTED, false},
                                   ms, WEATHER_FRESH_MS);
  if (why != REASON_NONE) drawStale(why);
}
