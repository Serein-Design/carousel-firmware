// SPDX-License-Identifier: AGPL-3.0-only
#include "places.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <new>

#include "certs.h"
#include "https_task.h"
#include "i18n.h"
#include "settings.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef PLACES_URL_DEV
#define PLACES_URL_DEV ""
#endif

volatile int32_t placesChosen = -1;

// One connection for every range of a lookup (HTTP/1.1 keep-alive): one
// handshake, then a round trip per 4 KB block.
class HttpSource : public PlacesSource {
 public:
  HttpSource() {
    placesUrl(url_, sizeof(url_), FIRMWARE_VERSION, PLACES_URL_DEV);
    https_ = strncmp(url_, "https://", 8) == 0;
    if (https_) trustRootCertificates(tls_);
  }
  ~HttpSource() {
    http_.end();
    tls_.stop();
    plain_.stop();
  }
  bool read(uint32_t offset, uint32_t len, uint8_t *out) override {
    if (WiFi.status() != WL_CONNECTED) return false;
    WiFiClient &client = https_ ? (WiFiClient &)tls_ : plain_;
    if (!http_.begin(client, url_)) return false;
    http_.setReuse(true);
    http_.setTimeout(8000);
    http_.setUserAgent("Carousel/" FIRMWARE_VERSION);
    char range[32];
    snprintf(range, sizeof(range), "bytes=%lu-%lu", (unsigned long)offset,
             (unsigned long)(offset + len - 1));
    http_.addHeader("Range", range);
    const int code = http_.GET();
    // The piece asked for and nothing else: a server ignoring Range would
    // send the whole file
    bool ok = code == 206 && http_.getSize() == (int)len;
    if (ok) {
      WiFiClient *in = http_.getStreamPtr();
      size_t got = 0;
      const uint32_t since = millis();
      while (in && got < len && millis() - since < 10000) {
        const int ready = in->available();
        if (ready > 0) {
          const int n = in->read(out + got, std::min((size_t)ready, (size_t)len - got));
          if (n > 0) got += n;
          continue;
        }
        if (!in->connected()) break;
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      ok = got == len;
    }
    if (!ok) Serial.printf("[PLACES] HTTP %d for %lu bytes at %lu\n", code, (unsigned long)len,
                           (unsigned long)offset);
    http_.end();   // the connection stays open for the next range
    return ok;
  }

 private:
  char url_[160];
  bool https_ = false;
  WiFiClientSecure tls_;
  WiFiClient plain_;
  HTTPClient http_;
};

// --- The table, the HTTPS task's alone --------------------------------------

static PlacesTable *table = nullptr;
static uint32_t lastUse = 0;
// The country of the panel's place, which a search ranks ten times: kept
// for the place it was found for (and through a city change, when the
// place is empty until the new one is found)
static char homeFor[sizeof(settings.place)] = "";
static int home = -1;

static PlacesTable *useTable() {
  if (!table) table = new (std::nothrow) PlacesTable();
  lastUse = millis();
  return table;
}

static int homeCountry(PlacesSource &src, PlacesTable &t) {
  if (settings.place[0] && strcmp(settings.place, homeFor) != 0) {
    const int c = t.countryOf(src, settings.place);
    if (t.error() != PLACES_OK) return home;   // unread: asked again next time
    home = c;
    snprintf(homeFor, sizeof(homeFor), "%s", settings.place);
  }
  return home;
}

static void logError(const PlacesTable &t) {
  static const char *const WHY[] = {"ok", "unreachable", "refused (hash or layout)", "no memory"};
  Serial.printf("[PLACES] table %s\n", WHY[t.error()]);
}

LocateResult placesLocate(const char *city, int32_t cid, PlaceHit &hit, char *zone,
                          size_t zoneSize) {
  HttpSource src;
  PlacesTable *t = useTable();
  if (!t) return LOCATE_UNREACHABLE;
  // The suggestion chosen; a number the table does not have, the name typed
  if (cid >= 0 && cid <= 0xFFFF && t->city(src, (uint16_t)cid, tr().code, hit, zone, zoneSize))
    return LOCATE_FOUND;
  if (t->error() == PLACES_OK) {
    const int n = t->search(src, city, homeCountry(src, *t), tr().code, &hit, 1);
    if (n == 0) return LOCATE_NOT_FOUND;
    if (n == 1 && t->zoneName(src, hit.zone, zone, zoneSize)) return LOCATE_FOUND;
  }
  logError(*t);
  return LOCATE_UNREACHABLE;
}

// --- The settings page's search, handed from the web task -------------------

// IDLE -> POSTED (web) -> RUNNING -> DONE (HTTPS) -> IDLE (web). The web
// task waits PLACES_WAIT_MS at most: the job carries on, and its answer is
// kept for the page's next ask with the same query. Another query replaces
// a job not started or an answer not taken; while one runs, it is "busy".
enum JobState : uint8_t { JOB_IDLE, JOB_POSTED, JOB_RUNNING, JOB_DONE };
static JobState state = JOB_IDLE;
static char jobQuery[33];
// On the heap from the search to the answer taken (800 bytes): the
// forecast's handshake needs every byte of RAM it can get (placesKeepTable)
static PlaceHit *jobHits = nullptr;
static int jobCount = -1;

// The answer not taken, dropped (under the lock)
static void dropAnswer() {
  delete[] jobHits;
  jobHits = nullptr;
  jobCount = -1;
}

// Made on first use: a function's static is built once even when two tasks
// get there together (ESP-IDF's C++ guards)
static SemaphoreHandle_t jobLock() {
  static const SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}
static SemaphoreHandle_t jobDone() {
  static const SemaphoreHandle_t s = xSemaphoreCreateBinary();
  return s;
}

struct JobLock {
  JobLock() { xSemaphoreTake(jobLock(), portMAX_DELAY); }
  ~JobLock() { xSemaphoreGive(jobLock()); }
};

// The answer to query, if the job holds it (under the lock): taken, IDLE
static bool takeAnswer(const char *query, char *json, size_t size, bool &ok) {
  if (state != JOB_DONE || strcmp(jobQuery, query) != 0) return false;
  ok = jobHits && jobCount >= 0 && placesJson(jobHits, jobCount, json, size);
  dropAnswer();
  state = JOB_IDLE;
  xSemaphoreTake(jobDone(), 0);
  return true;
}

bool placesSearch(const char *query, char *json, size_t size) {
  if (!jobLock() || !jobDone()) return false;
  bool ok = false;
  {
    JobLock l;
    if (takeAnswer(query, json, size, ok)) return ok;
    const bool same = state != JOB_IDLE && strcmp(jobQuery, query) == 0;
    if (state == JOB_RUNNING && !same) return false;   // busy: asked again
    if (!same) {
      dropAnswer();
      snprintf(jobQuery, sizeof(jobQuery), "%s", query);
      state = JOB_POSTED;
      xSemaphoreTake(jobDone(), 0);
    }
  }
  httpsWake();
  xSemaphoreTake(jobDone(), pdMS_TO_TICKS(PLACES_WAIT_MS));
  JobLock l;
  takeAnswer(query, json, size, ok);
  return ok;
}

void placesServe() {
  bool run = false;
  char q[sizeof(jobQuery)];
  if (jobLock()) {
    JobLock l;
    if (state == JOB_POSTED) {
      state = JOB_RUNNING;
      memcpy(q, jobQuery, sizeof(q));
      run = true;
    }
  }
  if (run) {
    int n = -1;
    PlaceHit *hits = new (std::nothrow) PlaceHit[PLACES_RESULTS];
    {
      HttpSource src;
      PlacesTable *t = hits ? useTable() : nullptr;
      if (t) {
        n = t->search(src, q, homeCountry(src, *t), tr().code, hits, PLACES_RESULTS);
        if (n < 0) logError(*t);
      }
    }
    JobLock l;
    jobHits = hits;
    jobCount = n;
    state = JOB_DONE;
    xSemaphoreGive(jobDone());
  }
  placesTidy(false);
}

void placesTidy(bool httpsNext) {
  if (table && !placesKeepTable(millis() - lastUse, httpsNext)) {
    delete table;
    table = nullptr;
    JobLock l;
    if (!httpsNext && state == JOB_DONE) {  // the page left without its answer
      dropAnswer();
      state = JOB_IDLE;
    }
  }
}
