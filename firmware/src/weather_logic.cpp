// SPDX-License-Identifier: AGPL-3.0-only
#include "weather_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "i18n.h"

static_assert(sizeof(Strings::wxCond) / sizeof(const char *) == WX_COND_COUNT,
              "one word per condition");

uint8_t daySlots(const HourlyForecast &h, int nowHour, uint8_t idx[3]) {
  if (!h.count) return 0;
  // Hours since the fetch: the forecast starts at the hour it was asked
  const int from = nowHour < 0 ? 0 : (nowHour - h.hour[0] + 24) % 24;
  uint8_t n = 0;
  for (int k = from + 3; k <= from + 9; k += 3)
    if (k < h.count) idx[n++] = (uint8_t)k;
  return n;
}

float tempIn(float celsius, uint8_t unit) {
  return unit == TEMP_F ? celsius * 9 / 5 + 32 : celsius;
}

// An integer has no -0, unlike roundf(-0.4f) printed with %.0f
int roundTemp(float t) { return (int)lroundf(t); }

bool formatTemp(char *out, size_t size, float t, Face f, uint16_t maxW) {
  static const char *const FORMATS[] = {"%d \xC2\xB0", "%d\xC2\xB0", "%d"};
  const int d = roundTemp(t);
  // Only the big digits set the degree apart, as the board draws them
  for (const char *fmt : FORMATS) {
    if (fmt == FORMATS[0] && f != Face::PIERRE) continue;
    snprintf(out, size, fmt, d);
    if (textWidth(out, f) <= maxW) return true;
  }
  return false;
}

void formatMinMax(char *out, size_t size, float tMin, float tMax) {
  snprintf(out, size, "%d\xC2\xB0 %d\xC2\xB0", roundTemp(tMin), roundTemp(tMax));
  if (textWidth(out, Face::LABEL) > WX_MINMAX_W)
    snprintf(out, size, "%d %d", roundTemp(tMin), roundTemp(tMax));
}

// The start of the UTF-8 character that ends before end
static size_t prevChar(const char *s, size_t end) {
  while (end > 0 && ((uint8_t)s[--end] & 0xC0) == 0x80) {}
  return end;
}

void fitText(char *text, Face f, uint16_t maxW) {
  size_t n = strlen(text);
  while (n && textWidth(text, f) > maxW) text[n = prevChar(text, n)] = '\0';
}

void splitLines(const char *word, Face f, uint16_t maxW, char *first, char *second,
                size_t size) {
  snprintf(first, size, "%s", word);
  second[0] = '\0';
  if (textWidth(first, f) <= maxW) return;
  // At the last space that leaves a first line that fits
  for (size_t at = strlen(word); at-- > 0; ) {
    if (word[at] != ' ') continue;
    snprintf(first, size, "%.*s", (int)at, word);
    if (textWidth(first, f) <= maxW && textWidth(word + at + 1, f) <= maxW) {
      snprintf(second, size, "%s", word + at + 1);
      return;
    }
  }
  // Otherwise inside the word, with a hyphen, as late as it fits
  for (size_t at = prevChar(word, strlen(word)); at > 0; at = prevChar(word, at)) {
    snprintf(first, size, "%.*s-", (int)at, word);
    if (textWidth(first, f) <= maxW) {
      snprintf(second, size, "%s", word + at);
      fitText(second, f, maxW);
      return;
    }
  }
  fitText(first, f, maxW);
}

void placeName(const char *place, const char *city, char *out, size_t size) {
  const char *src = place && place[0] ? place : city ? city : "";
  const char *comma = strchr(src, ',');
  const size_t n = comma ? (size_t)(comma - src) : strlen(src);
  snprintf(out, size, "%.*s", (int)n, src);
}
