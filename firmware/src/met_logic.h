// SPDX-License-Identifier: AGPL-3.0-only
// The forecast from MET Norway's Locationforecast 2.0 (compact), without the
// hardware: the request, the answer read as a stream, the condition of each
// symbol, and when to ask again. Host-tested in test_met; weather.cpp does
// the HTTPS. Terms: https://api.met.no/doc/TermsOfService
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "weather_logic.h"

// "lightrainshowers_day" -> WX_SHOWERS; a symbol MET has not documented gets
// no word (WX_UNKNOWN), never a wrong one
WxCond metCondition(const char *symbol);
// The sun is up, as the symbol's suffix says: only "_night" says it is not.
// "_polartwilight" draws as day: the sun sits on the horizon. The symbols
// without a suffix (cloudy, rain...) draw the same by day and by night.
bool metDay(const char *symbol);

// The request: coordinates cut (not rounded) to 4 decimals, as MET's terms
// ask; more makes every request a cache miss on their side
void metCoord(char *out, size_t size, float v);
void metUrl(char *out, size_t size, float lat, float lon);

// What one answer gives the screens. hourly.temp[0] and friends are now.
struct MetForecast {
  bool    valid = false;
  float   tMin  = 0;        // the local day's, over the hours the answer holds
  float   tMax  = 0;
  int32_t day   = 0;        // that local day, yyyymmdd
  HourlyForecast hourly;    // from the current hour, at least 1 when valid
};

// The local day of t (yyyymmdd) in the TZ in force
int32_t metDayOf(time_t t);

// Reads the answer as it arrives, a few bytes at a time, in fixed-size state:
// the body is tens of KB and never held whole. Only the hours from now and
// those of today are needed, so the reading stops (done) once past both.
class MetReader {
 public:
  // now: UTC, from the answer's Date; 0 when unknown, then the first hour
  // of the answer stands for now
  explicit MetReader(time_t now);
  // Takes the next n bytes; false once no more are wanted (done, or the
  // text is not the JSON expected)
  bool feed(const char *data, size_t n);
  bool done() const { return done_; }
  // The forecast, valid only when the reading got far enough: a truncated
  // body gives nothing rather than a day cut short
  MetForecast result() const;

 private:
  enum Key : uint8_t { K_OTHER, K_PROPERTIES, K_TIMESERIES, K_TIME, K_DATA, K_INSTANT,
                       K_DETAILS, K_AIR_TEMP, K_NEXT_1H, K_SUMMARY, K_SYMBOL };
  enum State : uint8_t { S_VALUE, S_KEY_START, S_KEY, S_KEY_ESC, S_COLON, S_STRING,
                         S_STRING_ESC, S_SCALAR, S_AFTER, S_END };
  enum Field : uint8_t { F_NONE, F_TIME, F_TEMP, F_SYMBOL };
  // Deeper than the answer goes: properties.timeseries[].data.instant.details
  // is 7. Containers past MAX_KEYS are followed, their keys not kept.
  static const uint8_t MAX_DEPTH = 32, MAX_KEYS = 8;

  bool step(char c);
  void open(bool object);
  void close(bool object);
  void startValue();
  void endScalar();
  bool inEntry() const;
  void commit();

  time_t now_, hourStart_ = 0;
  int32_t today_ = 0;
  State state_ = S_VALUE;
  uint8_t depth_ = 0;
  uint32_t objects_ = 0;            // bit d: the container at depth d is an object
  Key keys_[MAX_KEYS] = {};         // the key being read in each object
  Field field_ = F_NONE;            // what the scalar being read is
  char buf_[40];
  uint8_t len_ = 0;
  bool done_ = false, broken_ = false, runOver_ = false;
  // The entry being read
  time_t eTime_ = 0;
  float eTemp_ = 0;
  bool eHasTime_ = false, eHasTemp_ = false, eHasSymbol_ = false;
  char eSymbol_[40];
  MetForecast f_;
  bool dayFound_ = false;
};

// The forecast as time goes by without a new answer (a 304): the hours past
// drop off the front. False when none is left.
bool metAdvance(MetForecast &f, time_t now);

// A new answer for the same local day keeps the extremes of the earlier
// ones: MET sends the hours from now only, so the morning's low is gone from
// an answer at 15:00
void metKeepDay(MetForecast &f, const MetForecast &before);

// Each panel asks at its own second of the hour, from its MAC: thousands of
// panels never all ask at :00, and a restart does not move it
uint16_t metSlot(const uint8_t mac[6]);

// Seconds from the Wi-Fi up to the first request after a start: the slot's
// second of the minute. Panels started together (a power cut in a street,
// a crash loop) then do not ask at the same second; within a minute, since
// the answer's Date may set the clock before NTP does.
uint32_t metFirstDelay(uint16_t slot);

// Seconds from the answer (its Date, UTC) to the next request: at least an
// hour, never before Expires (0: none), at the panel's slot. Without a Date,
// an hour.
uint32_t metNextDelay(time_t date, time_t expires, uint16_t slot);

// Seconds to wait after the failures-th failure in a row. status: the HTTP
// code, or < 0 when no answer came (no link, DNS, TLS, a cut body). The
// server's refusals (429, 5xx, any other 4xx) wait an hour, doubling to 6 h;
// a missing answer 10 min, doubling to the hour.
uint32_t metRetryDelay(int status, uint8_t failures);
