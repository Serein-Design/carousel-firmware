// SPDX-License-Identifier: AGPL-3.0-only
// ===========================================================================
// WEATHER: MET Norway's Locationforecast (free, no key, CC BY 4.0) over
// HTTPS; the place and its time zone from the city table (places.cpp)
//
// The handshake and the answer take a second or more, which would freeze the
// display. It therefore runs in the HTTPS task (https_task.cpp), on core 0,
// while the display loop keeps core 1. What the answers mean is met_logic's
// and places_logic's.
// ===========================================================================
#include "weather.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>

#include <algorithm>

#include "certs.h"
#include "https_task.h"
#include "i18n.h"
#include "met_logic.h"
#include "network.h"
#include "ota.h"
#include "places.h"
#include "settings.h"
#include "time_logic.h"

Weather weather;
volatile bool locateFailed = false;

// MET's terms: an identifying User-Agent with a contact, or 403
static const char USER_AGENT[] = "Carousel/" FIRMWARE_VERSION
    " (+https://github.com/Serein-Design/carousel-firmware; contact@serein-design.fr)";
static const char *MET_HEADERS[] = {"Date", "Expires", "Last-Modified"};

// Between two answers, the HTTPS task's alone: the last forecast (advanced on a
// 304), what to send back as If-Modified-Since, the place it is for
static MetForecast forecast;
static char lastModified[32] = "";
static float askedLat = NAN, askedLon = NAN;
static uint8_t failures = 0;
static uint16_t slot = 0;          // the panel's second of the hour (metSlot)

// A wait from a moment: millis() - from never misreads, whatever the uptime
struct Wait {
  uint32_t from = 0, ms = 0;
  void start(uint32_t wait) { from = millis(); ms = wait; }
  bool over() const { return millis() - from >= ms; }
};

// The table's own pacing, apart from MET's: its backoff and failures in a
// row (an unreachable or refused table; a city not found is not one)
static Wait locateWait;
static uint8_t locateFailures = 0;

// The city typed or chosen -> place, coordinates and zone, from the city
// table (places.cpp). A place an earlier firmware found takes the zone.
static void locate() {
  const int32_t cid = placesChosen;
  PlaceHit hit;
  char zone[sizeof(settings.zone)];
  const LocateResult r = placesLocate(settings.city, cid, hit, zone, sizeof(zone));
  if (r == LOCATE_UNREACHABLE) {
    if (locateFailures < 255) locateFailures++;
    const uint32_t s = locateRetryDelay(locateFailures, slot);
    locateWait.start(s * 1000UL);
    Serial.printf("[PLACE] city table again in %lu min\n", (unsigned long)(s / 60));
    return;
  }
  locateFailures = 0;
  if (placesChosen == cid) placesChosen = -1;  // unless the form chose another meanwhile
  const Location loc = {settings.place, sizeof(settings.place), &settings.lat, &settings.lon,
                        settings.zone, sizeof(settings.zone)};
  if (r == LOCATE_NOT_FOUND || !takeLocation(hit, zone, loc)) {
    locateFailed = true;                       // not asked again until the city changes
    Serial.printf("[PLACE] \"%s\" not found%s\n", settings.city,
                  r == LOCATE_FOUND ? " near the place kept" : "");
    return;
  }
  locateFailed = false;
  // A suggestion chosen: the city is named after it, so that a restart
  // before then, or the page, shows that city
  if (r == LOCATE_FOUND && cid >= 0 && hit.id == cid)
    snprintf(settings.city, sizeof(settings.city), "%s", hit.name);
  applyZone(settings.zone);
  saveSettings();
  Serial.printf("[PLACE] %s (%.2f, %.2f) %s\n", settings.place, settings.lat, settings.lon,
                settings.zone);
}

static void show(const MetForecast &f) {
  const HourlyForecast &h = f.hourly;
  weather.temp = h.temp[0];
  weather.cond = h.cond[0];
  weather.day  = h.day[0];
  weather.tMin = f.tMin;
  weather.tMax = f.tMax;
  weather.hourly = h;
  weather.valid = true;
  weather.updatedAt = millis();
}

// Seconds before asking again after a failure
static uint32_t retry(int status) {
  if (failures < 255) failures++;
  const uint32_t s = metRetryDelay(status, failures);
  Serial.printf("[WEATHER] %s %d, again in %lu min\n", status < 0 ? "no answer" : "HTTP", status,
                (unsigned long)(s / 60));
  return s;
}

// The body as it arrives, in 512-byte reads: never held whole (tens of KB).
// Stops as soon as the reader has what it needs.
static MetForecast readForecast(HTTPClient &http, time_t date) {
  MetReader reader(date);
  WiFiClient *in = http.getStreamPtr();
  int left = http.getSize();                 // -1: until the server closes
  const uint32_t since = millis();
  uint8_t buf[512];
  while (in && left != 0 && millis() - since < 20000) {
    const int ready = in->available();
    if (ready <= 0) {
      if (!in->connected()) break;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    size_t want = std::min((size_t)ready, sizeof(buf));
    if (left > 0) want = std::min(want, (size_t)left);
    const int n = in->read(buf, want);
    if (n <= 0) continue;
    if (left > 0) left -= n;
    if (!reader.feed((const char *)buf, n)) break;
  }
  return reader.result();
}

// One request; returns the seconds until the next one
static uint32_t fetchWeather() {
  if (settings.lat != askedLat || settings.lon != askedLon) {
    forecast = MetForecast();                // another place: nothing to revalidate
    lastModified[0] = '\0';
    askedLat = settings.lat;
    askedLon = settings.lon;
  }
  char url[112];
  metUrl(url, sizeof(url), settings.lat, settings.lon);
  WiFiClientSecure client;
  trustRootCertificates(client);
  HTTPClient http;
  http.setTimeout(10000);
  // HTTP/1.0: no chunked body, so the stream read below is the JSON itself
  http.useHTTP10(true);
  if (!http.begin(client, url)) {
    http.end();
    return retry(-1);
  }
  http.setUserAgent(USER_AGENT);
  http.collectHeaders(MET_HEADERS, 3);
  // A new local day wants a whole answer: its extremes start again
  const bool revalidate = lastModified[0] && forecast.valid && timeValid &&
                          metDayOf(time(nullptr)) == forecast.day;
  if (revalidate) http.addHeader("If-Modified-Since", lastModified);
  const int code = http.GET();
  if (code <= 0) {
    http.end();
    return retry(code);
  }
  networkHttpDate(http.header("Date").c_str());
  time_t date = 0, expires = 0;
  if (!parseHttpDate(http.header("Date").c_str(), &date) && timeValid) date = time(nullptr);
  parseHttpDate(http.header("Expires").c_str(), &expires);
  uint32_t next;
  if (code == 304 && revalidate) {
    // Unchanged: the hours already held, the past ones dropped
    if (metAdvance(forecast, date ? date : time(nullptr))) {
      failures = 0;
      show(forecast);
      Serial.printf("[WEATHER] unchanged, %u hours left\n", forecast.hourly.count);
      next = metNextDelay(date, expires, slot);
    } else {
      lastModified[0] = '\0';               // nothing left: a whole answer next time
      next = retry(-1);
    }
  } else if (code == 200 || code == 203) {   // 203: the product is deprecated, still served
    if (code == 203) Serial.println("[WEATHER] MET says this API version is deprecated");
    MetForecast f = readForecast(http, date);
    if (f.valid) {
      metKeepDay(f, forecast);
      forecast = f;
      snprintf(lastModified, sizeof(lastModified), "%s", http.header("Last-Modified").c_str());
      failures = 0;
      show(forecast);
      const HourlyForecast &h = forecast.hourly;
      Serial.printf("[WEATHER] %.1fC  min %.1f  max %.1f  cond %d  %s  %u hours\n", h.temp[0],
                    forecast.tMin, forecast.tMax, (int)h.cond[0], h.day[0] ? "day" : "night",
                    h.count);
      next = metNextDelay(date, expires, slot);
    } else {
      Serial.println("[WEATHER] unreadable or cut answer");
      next = retry(-1);
    }
  } else {
    next = retry(code);
  }
  http.end();
  return next;
}

// The first fetch within a minute of the Wi-Fi up, not at a fixed 6 s: its
// Date may give the clock its time before NTP does
static bool started = false;
static uint32_t dueAt = 0;

uint32_t weatherStep(bool asked) {
  if (!started) {
    if (WiFi.status() != WL_CONNECTED) return 1000;
    uint8_t mac[6];
    WiFi.macAddress(mac);
    slot = metSlot(mac);
    dueAt = millis() + metFirstDelay(slot) * 1000UL;
    started = true;
  }
  if (WiFi.status() == WL_CONNECTED && !otaInProgress) {
    const bool ask = asked || (int32_t)(millis() - dueAt) >= 0;
    // The owner changed the city: one try now, whatever the backoff
    if (asked) locateWait.start(0);
    // The city is looked up when it changed, or along with a forecast
    // request until found: never on a timer, and not again once the
    // table said it has none. Also a place an earlier firmware found,
    // which kept no zone.
    if (ask && locateWait.over() &&
        locateWanted(settings.city, settings.place, settings.zone, locateFailed,
                     placesChosen >= 0))
      locate();
    // A new place is a new forecast: now, whatever Expires said for the old
    // one (none asked yet: the first request keeps its second)
    if (!isnan(askedLat) && (settings.lat != askedLat || settings.lon != askedLon))
      dueAt = millis();
    if ((int32_t)(millis() - dueAt) >= 0) {
      placesTidy(true);                      // its 13 KB would starve the handshake
      dueAt = millis() + fetchWeather() * 1000UL;
    }
  }
  // Until the next request; a look every minute when the Wi-Fi or an
  // update held the last one back
  return constrain((int32_t)(dueAt - millis()), 1000, (int32_t)HTTPS_LOOK_MS);
}

void weatherRefresh() { httpsAsk(HTTPS_WEATHER); }
