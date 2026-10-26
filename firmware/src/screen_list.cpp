// SPDX-License-Identifier: AGPL-3.0-only
#include "screen_list.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SCREEN_INFO_ROW(name, draw, orientation, leave, settings) {&Strings::name, orientation},
const ScreenInfo SCREEN_INFO[SCREEN_COUNT] = {SCREEN_TABLE(SCREEN_INFO_ROW)};
#undef SCREEN_INFO_ROW

const char *screenNameEn(uint8_t i) { return STRINGS[LANG_EN].*SCREEN_INFO[i].name; }

int screenFind(const char *name) {
  for (uint8_t i = 0; i < SCREEN_COUNT; i++)
    if (!strcasecmp(name, screenNameEn(i))) return i;
  return -1;
}

void screenSlug(uint8_t i, char *out, size_t size) {
  if (!size) return;
  size_t n = 0;
  for (const char *p = screenNameEn(i); *p && n + 1 < size; p++)
    out[n++] = *p == ' ' ? '-' : (char)tolower((unsigned char)*p);
  out[n] = '\0';
}

int screenFindSlug(const char *slug) {
  if (!slug) return -1;
  char own[SCREEN_SLUG_MAX];
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    screenSlug(i, own, sizeof(own));
    if (!strcmp(slug, own)) return i;
  }
  return -1;
}

bool screenFits(uint8_t i, bool portrait) {
  const Orientation o = SCREEN_INFO[i].orientation;
  return o == BOTH || o == (portrait ? PORTRAIT : LANDSCAPE);
}

ScreenList screenListDefault() {
  ScreenList l = screenListAll();
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    const char *const Strings::*n = SCREEN_INFO[i].name;
    if (n == &Strings::scrBall || n == &Strings::scrMatrix || n == &Strings::scrRays || n == &Strings::scrBars)
      l.on &= ~(1u << i);
  }
  return l;
}

int screenPlace(const ScreenList &l, uint8_t i) {
  for (uint8_t p = 0; p < SCREEN_COUNT; p++)
    if (l.order[p] == i) return p;
  return -1;
}

// Stored names are written by screenListFormat: exact English names, so a
// stored entry is matched exactly, unlike a name typed into the API.
static int findStored(const char *name, size_t len) {
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    const char *n = screenNameEn(i);
    if (strlen(n) == len && !strncmp(n, name, len)) return i;
  }
  return -1;
}

ScreenList screenListParse(const char *text) {
  ScreenList l;
  uint32_t seen = 0;
  uint8_t count = 0;
  for (const char *p = text; *p;) {
    const char *end = strchr(p, ',');
    const size_t len = end ? (size_t)(end - p) : strlen(p);
    const bool off = len && *p == '-';
    const int i = findStored(p + off, len - off);
    if (i >= 0 && !((seen >> i) & 1u)) {
      seen |= 1u << i;
      l.order[count++] = (uint8_t)i;
      if (!off) l.on |= 1u << i;
    }
    if (!end) break;
    p = end + 1;
  }
  for (uint8_t i = 0; i < SCREEN_COUNT; i++)
    if (!((seen >> i) & 1u)) {
      l.order[count++] = i;
      l.on |= 1u << i;
    }
  return l;
}

bool screenListFormat(const ScreenList &l, char *out, size_t size) {
  size_t n = 0;
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) {
    const uint8_t i = l.order[p];
    const int w = snprintf(out + n, size - n, "%s%s%s", p ? "," : "", screenOn(l, i) ? "" : "-",
                           screenNameEn(i));
    if (w < 0 || (size_t)w >= size - n) {
      if (size) out[0] = '\0';
      return false;
    }
    n += w;
  }
  return true;
}

bool screenListSameView(const ScreenList &a, const ScreenList &b) {
  return a.on == b.on && !memcmp(a.order, b.order, SCREEN_COUNT);
}

static bool anyOnFits(const ScreenList &l, bool portrait) {
  for (uint8_t i = 0; i < SCREEN_COUNT; i++)
    if (screenOn(l, i) && screenFits(i, portrait)) return true;
  return false;
}

bool screenListFill(ScreenList &l) {
  bool filled = false;
  for (int o = 0; o < 2; o++) {
    const bool portrait = o == 1;
    if (anyOnFits(l, portrait)) continue;
    for (uint8_t i = 0; i < SCREEN_COUNT; i++)
      if (screenFits(i, portrait)) l.on |= 1u << i;
    filled = true;
  }
  return filled;
}

ListCheck screenListCheck(const ScreenList &l) {
  if (!l.on) return LIST_EMPTY;
  if (!anyOnFits(l, true)) return LIST_NO_PORTRAIT;
  if (!anyOnFits(l, false)) return LIST_NO_LANDSCAPE;
  return LIST_OK;
}

bool screenListValid(const ScreenList &l) {
  if (l.on & ~screenListAll().on) return false;   // a bit past the last screen
  uint32_t seen = 0;
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) {
    const uint8_t i = l.order[p];
    if (i >= SCREEN_COUNT || ((seen >> i) & 1u)) return false;
    seen |= 1u << i;
  }
  return screenListCheck(l) == LIST_OK;
}

bool rotateOffered(long seconds) {
  for (uint16_t c : ROTATE_CHOICES)
    if (seconds == c) return true;
  return false;
}

ScreensForm::ScreensForm(const ScreenList &current, uint16_t rotateSec)
    : current_(current), rotate_(rotateSec) {}

void ScreensForm::arg(const char *name, const char *value) {
  if (!strcmp(name, "o")) {
    if (orders_ >= READ_MAX) return;
    orders_++;
    const int i = findStored(value, strlen(value));
    if (i < 0 || ((posted_ >> i) & 1u)) return;   // unknown, or given twice: the first wins
    posted_ |= 1u << i;
    order_[count_++] = (uint8_t)i;
  } else if (!strcmp(name, "on")) {
    if (boxes_ >= READ_MAX) return;
    boxes_++;
    const int i = findStored(value, strlen(value));
    if (i >= 0) on_ |= 1u << i;
  } else if (!strcmp(name, "mv")) {
    // One arrow is one submit button: the first readable one counts
    const char *colon = strrchr(value, ':');
    if (moved_ >= 0 || !colon || (strcmp(colon, ":u") && strcmp(colon, ":d"))) return;
    const int i = findStored(value, colon - value);
    if (i < 0) return;
    moved_ = (int8_t)i;
    up_ = colon[1] == 'u';
  } else if (!strcmp(name, "rt")) {
    char *end;
    const long v = strtol(value, &end, 10);
    if (end == value || *end || *value < '0' || *value > '9' || !rotateOffered(v)) return;
    rotate_ = (uint16_t)v;
    rotateSet_ = true;
  }
}

ScreenList ScreensForm::list() const {
  if (!count_) return current_;
  ScreenList l;
  uint8_t n = 0;
  for (uint8_t p = 0; p < count_; p++) l.order[n++] = order_[p];
  l.on = on_ & posted_;
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) {
    const uint8_t i = current_.order[p];
    if ((posted_ >> i) & 1u) continue;
    l.order[n++] = i;
    l.on |= current_.on & (1u << i);
  }
  if (moved_ >= 0) {
    const int p = screenPlace(l, (uint8_t)moved_);
    const int q = up_ ? p - 1 : p + 1;
    if (q >= 0 && q < SCREEN_COUNT) {
      l.order[p] = l.order[q];
      l.order[q] = (uint8_t)moved_;
    }
  }
  return l;
}

int screenRequestStatus(const ScreenList &l, const char *name, bool portrait) {
  const int i = screenFind(name);
  if (i < 0) return 404;
  if (!screenOn(l, (uint8_t)i)) return 409;
  return screenFits((uint8_t)i, portrait) ? 200 : 404;
}

// Screen names are the firmware's own: letters, digits and a space, nothing
// to escape in JSON
void screensInfoJson(String &j, const ScreenList &l, uint16_t rotateSec, int32_t nextIn,
                     bool portrait) {
  j += ",\"screens\":[";
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    j += i ? ",{\"name\":\"" : "{\"name\":\"";
    j += screenNameEn(i);
    j += screenFits(i, portrait) ? "\",\"available\":true" : "\",\"available\":false";
    j += screenOn(l, i) ? ",\"enabled\":true}" : ",\"enabled\":false}";
  }
  j += "],\"order\":[";
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) {
    j += p ? ",\"" : "\"";
    j += screenNameEn(l.order[p]);
    j += '"';
  }
  j += "],\"rotate_every\":";
  j += String((unsigned int)rotateSec);
  j += ",\"rotate_next_in\":";
  if (nextIn < 0) j += "null";
  else j += String((long)nextIn);
}
