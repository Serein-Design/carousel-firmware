// SPDX-License-Identifier: AGPL-3.0-only
#include "info_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void groupDigits(uint32_t v, char *out, size_t size) {
  if (!size) return;
  char raw[12];
  const int n = snprintf(raw, sizeof(raw), "%lu", (unsigned long)v);
  size_t j = 0;
  for (int i = 0; i < n; i++) {
    const bool gap = i > 0 && (n - i) % 3 == 0;
    if (j + (gap ? 3 : 0) + 1 >= size) break;
    if (gap) {
      memcpy(out + j, "\xE2\x80\xAF", 3);   // U+202F
      j += 3;
    }
    out[j++] = raw[i];
  }
  out[j] = '\0';
}

Face numberFace(const char *text, uint16_t width) {
  if (textWidth(text, Face::PIERRE) <= width) return Face::PIERRE;
  if (textWidth(text, Face::BODY) <= width) return Face::BODY;
  return Face::LABEL;
}

int16_t faceTop(Face f, int16_t top, int16_t bottom) {
  return top + (bottom - top + 1 - FONT_FACES[(int)f].cap) / 2;
}

void dateDayLine(char *out, size_t size, const Strings &t, int yday1, bool tall) {
  if (tall) snprintf(out, size, "%s%d", t.dayPrefix, yday1);
  else snprintf(out, size, t.dateDay, (unsigned)yday1);
}

uint8_t signalBars(bool connected, int rssi) {
  if (!connected) return 0;
  return rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1;
}

void uptimeText(char *out, size_t size, const Strings &t, uint32_t seconds) {
  const unsigned m = seconds / 60, h = m / 60, d = h / 24;
  if (!h) snprintf(out, size, t.sysUpMin, m);
  else if (!d) snprintf(out, size, "%uH%02u", h, m % 60);
  // From 100 days the hours no longer fit beside them, nor matter
  else if (d < 100) snprintf(out, size, "%u%s %02uH", d, t.sysUpDay, h % 24);
  else snprintf(out, size, "%u%s", d, t.sysUpDay);
}

int signalDbm(int rssi) { return rssi < -99 ? -99 : rssi; }

uint8_t ramPercent(uint32_t freeBytes, uint32_t totalBytes) {
  if (!totalBytes) return 0;
  const uint64_t p = 100ULL * freeBytes / totalBytes;
  return (uint8_t)(p > 99 ? 99 : p);
}

MoneyDay moneyDay(const MoneyTerms &m, int32_t nowSec, int mday) {
  const int days = m.days > 1 ? m.days : 1;
  const int32_t start = m.start * 3600;
  const int32_t span = (m.end - m.start > 1 ? m.end - m.start : 1) * 3600;
  const float perSec = m.salary / (days * (float)span);
  int32_t worked = nowSec - start;
  if (worked < 0) worked = 0;
  if (worked > span) worked = span;
  MoneyDay d;
  d.today = worked * perSec;
  d.progress = (float)worked / span;
  d.working = nowSec >= start && nowSec < start + span;
  // The working days already gone, as a share of a 30-day month, then today
  int gone = mday * days / 30;
  if (gone > days) gone = days;
  d.month = gone * (m.salary / days) + d.today;
  return d;
}

// The symbols in Currency order; € and $ and £ in the label face (atelier.mjs)
static const struct { const char *symbol; bool before; } CURRENCIES[CURRENCY_COUNT] = {
  {"\xE2\x82\xAC", false}, {"$", true}, {"\xC2\xA3", true}, {"CHF", false},
};

static const char *symbolBefore(uint8_t currency) {
  return currency < CURRENCY_COUNT && CURRENCIES[currency].before ? CURRENCIES[currency].symbol
                                                                   : "";
}

// " €" with the language's space, or "" when the symbol goes before
static void symbolAfter(char *out, size_t size, const Strings &t, uint8_t currency) {
  const uint8_t c = currency < CURRENCY_COUNT ? currency : (uint8_t)CURRENCY_EUR;
  if (CURRENCIES[c].before) out[0] = '\0';
  else snprintf(out, size, "%s%s", t.moneySpace, CURRENCIES[c].symbol);
}

void moneyAmount(MoneyText &out, const Strings &t, uint8_t currency, float v) {
  if (v < 0) v = 0;
  // Whole cents, rounded down, as the counter only ever climbs
  const uint64_t c = (uint64_t)(v * 100.0);
  snprintf(out.before, sizeof(out.before), "%s", symbolBefore(currency));
  groupDigits((uint32_t)(c / 100), out.units, sizeof(out.units));
  char sym[12];
  symbolAfter(sym, sizeof(sym), t, currency);
  snprintf(out.after, sizeof(out.after), "%s%02u%s", t.decimalMark, (unsigned)(c % 100), sym);
}

uint16_t moneyRoom(const MoneyText &m, uint16_t width, bool tall) {
  int room = width;
  if (m.before[0]) room -= textWidth(m.before, Face::LABEL) + INFO_GAP;
  if (!tall) room -= INFO_GAP + textWidth(m.after, Face::LABEL);
  return room > 0 ? (uint16_t)room : 0;
}

void moneyTotal(char *out, size_t size, const Strings &t, uint8_t currency, float v,
                uint16_t maxW) {
  char digits[24], sym[12];
  groupDigits((uint32_t)(v > 0 ? v : 0), digits, sizeof(digits));
  symbolAfter(sym, sizeof(sym), t, currency);
  snprintf(out, size, "%s%s%s", symbolBefore(currency), digits, sym);
  if (textWidth(out, Face::LABEL) > maxW) snprintf(out, size, "%s", digits);
}

uint8_t socialShown(const uint32_t value[SOCIAL_COUNT], const bool live[SOCIAL_COUNT],
                    uint8_t out[SOCIAL_COUNT]) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SOCIAL_COUNT; i++)
    if (value[i] || live[i]) out[n++] = i;
  if (n) return n;
  for (uint8_t i = 0; i < SOCIAL_COUNT; i++) out[i] = i;
  return SOCIAL_COUNT;
}

int16_t socialPagerWidth(uint8_t count) { return count > 1 ? count * 3 - 1 : 0; }

Face socialCount(char *out, size_t size, const Strings &t, uint32_t v, uint16_t width) {
  groupDigits(v, out, size);
  const Face f = numberFace(out, width);
  if (textWidth(out, f) <= width) return f;
  const uint32_t tenths = v / 100000;
  char n[16];
  if (tenths >= 1000) snprintf(n, sizeof(n), "%u", (unsigned)(tenths / 10));
  else snprintf(n, sizeof(n), "%u%s%u", (unsigned)(tenths / 10), t.decimalMark,
                (unsigned)(tenths % 10));
  snprintf(out, size, t.socialMillions, n);
  return Face::LABEL;
}
