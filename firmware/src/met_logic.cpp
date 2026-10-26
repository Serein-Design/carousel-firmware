// SPDX-License-Identifier: AGPL-3.0-only
#include "met_logic.h"

#include <ctype.h>
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "time_logic.h"

static bool startsWith(const char *s, const char *prefix, const char **rest) {
  const size_t n = strlen(prefix);
  if (strncmp(s, prefix, n) != 0) return false;
  *rest = s + n;
  return true;
}

// What falls, after its strength: "rain", "sleetshowers", "snowandthunder"...
static WxCond precipitation(const char *s, bool light) {
  const char *r, *t;
  enum { RAIN, SLEET, SNOW } kind;
  if (startsWith(s, "rain", &r)) kind = RAIN;
  else if (startsWith(s, "sleet", &r)) kind = SLEET;
  else if (startsWith(s, "snow", &r)) kind = SNOW;
  else return WX_UNKNOWN;
  const bool showers = startsWith(r, "showers", &t);
  if (showers) r = t;
  if (startsWith(r, "andthunder", &t)) return *t ? WX_UNKNOWN : WX_STORM;
  if (*r) return WX_UNKNOWN;
  if (kind == SLEET) return WX_FREEZING;
  if (kind == SNOW) return WX_SNOW;
  // Light rain is drizzle's look on 64 px: a few drops
  return showers ? WX_SHOWERS : light ? WX_DRIZZLE : WX_RAIN;
}

// The symbols as https://api.met.no/weatherapi/weathericon/2.0/documentation lists them
WxCond metCondition(const char *symbol) {
  const char *u = strchr(symbol, '_');
  if (u && strcmp(u, "_day") != 0 && strcmp(u, "_night") != 0 && strcmp(u, "_polartwilight") != 0)
    return WX_UNKNOWN;
  char base[40];
  const size_t n = u ? (size_t)(u - symbol) : strlen(symbol);
  if (n == 0 || n >= sizeof(base)) return WX_UNKNOWN;
  memcpy(base, symbol, n);
  base[n] = '\0';
  static const struct { const char *name; WxCond c; } SKY[] = {
      {"clearsky", WX_CLEAR}, {"fair", WX_MOSTLY_CLEAR}, {"partlycloudy", WX_PARTLY},
      {"cloudy", WX_OVERCAST}, {"fog", WX_FOG}};
  for (const auto &k : SKY)
    if (strcmp(base, k.name) == 0) return k.c;
  // "lights" before "light": MET spells two of them lightssleet... and
  // lightssnow...; "lightsnow" then reads as "light" + "snow"
  static const char *const STRENGTH[] = {"lights", "light", "heavy", ""};
  for (const char *p : STRENGTH) {
    const char *r;
    if (!startsWith(base, p, &r)) continue;
    const WxCond c = precipitation(r, p[0] == 'l');
    if (c != WX_UNKNOWN) return c;
  }
  return WX_UNKNOWN;
}

bool metDay(const char *symbol) {
  const char *u = strrchr(symbol, '_');
  return !u || strcmp(u, "_night") != 0;
}

void metCoord(char *out, size_t size, float v) {
  // Through 5 decimals first: 2.3488 is 2.34879994 as a float, which a
  // straight cut would make 2.3487
  const long four = lround((double)v * 100000.0) / 10;   // integer division: toward zero
  const long a = labs(four);
  snprintf(out, size, "%s%ld.%04ld", four < 0 ? "-" : "", a / 10000, a % 10000);
}

void metUrl(char *out, size_t size, float lat, float lon) {
  char la[16], lo[16];
  metCoord(la, sizeof(la), lat);
  metCoord(lo, sizeof(lo), lon);
  snprintf(out, size, "https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=%s&lon=%s",
           la, lo);
}

int32_t metDayOf(time_t t) {
  struct tm tm;
  localtime_r(&t, &tm);
  return (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;
}

// --- The answer, as a stream -------------------------------------------------

static bool blank(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

MetReader::MetReader(time_t now) : now_(now) {
  if (now_ > 0) {
    hourStart_ = now_ - now_ % 3600;
    today_ = metDayOf(now_);
  }
}

bool MetReader::feed(const char *data, size_t n) {
  for (size_t i = 0; i < n && !done_ && !broken_; i++)
    while (!step(data[i])) {}
  return !done_ && !broken_;
}

MetForecast MetReader::result() const {
  MetForecast f = f_;
  f.valid = done_ && !broken_ && f.hourly.count > 0;
  f.day = today_;
  if (!dayFound_ && f.hourly.count) f.tMin = f.tMax = f.hourly.temp[0];
  return f;
}

// root{} > "properties"{} > "timeseries"[] > {} : one hour
bool MetReader::inEntry() const {
  const auto obj = [&](uint8_t d) { return (objects_ >> d & 1u) != 0; };
  return depth_ >= 4 && obj(0) && keys_[0] == K_PROPERTIES && obj(1) &&
         keys_[1] == K_TIMESERIES && !obj(2) && obj(3);
}

void MetReader::open(bool object) {
  if (depth_ == MAX_DEPTH) { broken_ = true; return; }
  if (object) objects_ |= 1u << depth_;
  else objects_ &= ~(1u << depth_);
  if (depth_ < MAX_KEYS) keys_[depth_] = K_OTHER;
  depth_++;
  if (depth_ == 4 && inEntry()) eHasTime_ = eHasTemp_ = eHasSymbol_ = false;
  state_ = object ? S_KEY_START : S_VALUE;
}

void MetReader::close(bool object) {
  if (!depth_ || ((objects_ >> (depth_ - 1) & 1u) != 0) != object) { broken_ = true; return; }
  const bool entry = depth_ == 4 && inEntry();
  // The series itself closes: every hour has been read
  const bool series = depth_ == 3 && !object && keys_[0] == K_PROPERTIES &&
                      keys_[1] == K_TIMESERIES && (objects_ & 3u) == 3u;
  depth_--;
  if (entry) commit();
  if (series) done_ = true;
  state_ = depth_ ? S_AFTER : S_END;
}

void MetReader::startValue() {
  len_ = 0;
  field_ = F_NONE;
  if (!inEntry()) return;
  if (depth_ == 4 && keys_[3] == K_TIME) {
    field_ = F_TIME;
  } else if (depth_ == 7 && keys_[3] == K_DATA) {
    if (keys_[4] == K_INSTANT && keys_[5] == K_DETAILS && keys_[6] == K_AIR_TEMP) field_ = F_TEMP;
    else if (keys_[4] == K_NEXT_1H && keys_[5] == K_SUMMARY && keys_[6] == K_SYMBOL) field_ = F_SYMBOL;
  }
}

void MetReader::endScalar() {
  if (field_ == F_NONE || len_ >= sizeof(buf_)) return;   // not ours, or longer than ours
  buf_[len_] = '\0';
  if (field_ == F_TIME) {
    eHasTime_ = parseIsoUtc(buf_, &eTime_);
  } else if (field_ == F_TEMP) {
    char *end;
    const float t = strtof(buf_, &end);
    // The records are -89 and +57 °C: anything past them is not a reading
    eHasTemp_ = end != buf_ && !*end && t >= -90 && t <= 70;
    if (eHasTemp_) eTemp_ = t;
  } else {
    memcpy(eSymbol_, buf_, len_ + 1);
    eHasSymbol_ = true;
  }
  field_ = F_NONE;
}

bool MetReader::step(char c) {
  // The keys on the way to the three values read
  static const struct { const char *name; Key key; } KEYS[] = {
      {"properties", K_PROPERTIES}, {"timeseries", K_TIMESERIES}, {"time", K_TIME},
      {"data", K_DATA}, {"instant", K_INSTANT}, {"details", K_DETAILS},
      {"air_temperature", K_AIR_TEMP}, {"next_1_hours", K_NEXT_1H}, {"summary", K_SUMMARY},
      {"symbol_code", K_SYMBOL}};
  switch (state_) {
    case S_VALUE:
      if (blank(c)) return true;
      if (c == '{' || c == '[') { open(c == '{'); return true; }
      if (c == ']') { close(false); return true; }   // an empty array
      if (c == '"') { startValue(); state_ = S_STRING; return true; }
      if (c == '-' || isalnum((unsigned char)c)) { startValue(); state_ = S_SCALAR; return false; }
      broken_ = true;
      return true;
    case S_KEY_START:
      if (blank(c)) return true;
      if (c == '"') { len_ = 0; state_ = S_KEY; return true; }
      if (c == '}') { close(true); return true; }    // an empty object
      broken_ = true;
      return true;
    case S_KEY:
      if (c == '\\') { len_ = sizeof(buf_); state_ = S_KEY_ESC; return true; }   // never one of ours
      if (c != '"') {
        if (len_ < sizeof(buf_)) buf_[len_++] = c;
        return true;
      }
      if (depth_ <= MAX_KEYS) {
        Key k = K_OTHER;
        if (len_ < sizeof(buf_)) {
          buf_[len_] = '\0';
          for (const auto &e : KEYS)
            if (strcmp(buf_, e.name) == 0) k = e.key;
        }
        keys_[depth_ - 1] = k;
      }
      state_ = S_COLON;
      return true;
    case S_KEY_ESC:
      state_ = S_KEY;
      return true;
    case S_COLON:
      if (blank(c)) return true;
      if (c == ':') state_ = S_VALUE;
      else broken_ = true;
      return true;
    case S_STRING:
      if (c == '\\') { len_ = sizeof(buf_); state_ = S_STRING_ESC; return true; }   // nor this
      if (c == '"') {
        if (field_ == F_TEMP) field_ = F_NONE;     // a quoted temperature is not a reading
        endScalar();
        state_ = S_AFTER;
        return true;
      }
      if (len_ < sizeof(buf_)) buf_[len_++] = c;
      return true;
    case S_STRING_ESC:
      state_ = S_STRING;
      return true;
    case S_SCALAR:
      if (isalnum((unsigned char)c) || c == '-' || c == '+' || c == '.') {
        if (len_ < sizeof(buf_)) buf_[len_++] = c;
        return true;
      }
      if (field_ != F_TEMP) field_ = F_NONE;       // nor an unquoted time or symbol
      endScalar();
      state_ = S_AFTER;
      return false;
    case S_AFTER:
      if (blank(c)) return true;
      // After a scalar at the root: no container to go on in
      if (!depth_) { broken_ = true; return true; }
      if (c == ',') {
        state_ = (objects_ >> (depth_ - 1) & 1u) ? S_KEY_START : S_VALUE;
        return true;
      }
      if (c == '}' || c == ']') { close(c == '}'); return true; }
      broken_ = true;
      return true;
    case S_END:
      return true;   // after the root: not read
  }
  return true;
}

void MetReader::commit() {
  HourlyForecast &h = f_.hourly;
  // An hour without a time cannot be placed: skipped, and it ends a run
  if (!eHasTime_) {
    if (h.count) runOver_ = true;
    return;
  }
  if (now_ <= 0) {
    now_ = eTime_;
    hourStart_ = now_ - now_ % 3600;
    today_ = metDayOf(now_);
  }
  const int32_t day = metDayOf(eTime_);
  // The day's extremes: the hours before now count too, the answer may
  // still hold the last one
  if (eHasTemp_ && day == today_) {
    if (!dayFound_ || eTemp_ < f_.tMin) f_.tMin = eTemp_;
    if (!dayFound_ || eTemp_ > f_.tMax) f_.tMax = eTemp_;
    dayFound_ = true;
  }
  if (eTime_ >= hourStart_ && !runOver_) {
    // In a row from the first hour at or after now, each one whole; the
    // first one missing or out of step ends the run
    const bool inStep = !h.count || eTime_ == h.start + (time_t)h.count * 3600;
    if (inStep && eHasTemp_ && eHasSymbol_) {
      if (!h.count) h.start = eTime_;
      struct tm tm;
      localtime_r(&eTime_, &tm);
      h.hour[h.count] = (uint8_t)tm.tm_hour;
      h.temp[h.count] = eTemp_;
      h.cond[h.count] = metCondition(eSymbol_);
      h.day[h.count]  = metDay(eSymbol_);
      if (++h.count == WEATHER_HOURS) runOver_ = true;
    } else {
      runOver_ = true;
    }
  }
  if (runOver_ && day > today_) done_ = true;
}

// --- Between two answers --------------------------------------------------------

bool metAdvance(MetForecast &f, time_t now) {
  HourlyForecast &h = f.hourly;
  if (!f.valid || !h.count) return f.valid = false;
  if (now < h.start + 3600) return true;
  const time_t k = (now - h.start) / 3600;
  if (k >= h.count) {
    h.count = 0;
    return f.valid = false;
  }
  const uint8_t n = (uint8_t)(h.count - k);
  memmove(h.hour, h.hour + k, n * sizeof(h.hour[0]));
  memmove(h.temp, h.temp + k, n * sizeof(h.temp[0]));
  memmove(h.cond, h.cond + k, n * sizeof(h.cond[0]));
  memmove(h.day, h.day + k, n * sizeof(h.day[0]));
  h.count = n;
  h.start += k * 3600;
  return true;
}

void metKeepDay(MetForecast &f, const MetForecast &before) {
  if (!f.valid || !before.valid || before.day != f.day) return;
  if (before.tMin < f.tMin) f.tMin = before.tMin;
  if (before.tMax > f.tMax) f.tMax = before.tMax;
}

// --- When to ask ------------------------------------------------------------------

uint16_t metSlot(const uint8_t mac[6]) {
  uint32_t h = 2166136261u;   // FNV-1a: neighbouring MACs land far apart
  for (int i = 0; i < 6; i++) h = (h ^ mac[i]) * 16777619u;
  return (uint16_t)(h % 3600);
}

uint32_t metFirstDelay(uint16_t slot) { return slot % 60; }

uint32_t metNextDelay(time_t date, time_t expires, uint16_t slot) {
  if (date <= 0) return 3600;
  // A far Expires (a server's slip) waits 6 h at most, the backoff's ceiling
  const time_t bound = std::min(std::max(date + 3600, expires), date + 6 * 3600);
  time_t t = bound - bound % 3600 + slot % 3600;
  if (t < bound) t += 3600;
  return (uint32_t)(t - date);
}

uint32_t metRetryDelay(int status, uint8_t failures) {
  const uint8_t n = failures ? failures - 1 : 0;
  // An answer that is not the forecast: the server said no, so not before an hour
  const bool refused = status > 0;
  const uint32_t base = refused ? 3600 : 600, cap = refused ? 6 * 3600 : 3600;
  return n >= 6 ? cap : std::min(cap, base << n);
}
