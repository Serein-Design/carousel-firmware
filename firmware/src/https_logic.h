// SPDX-License-Identifier: AGPL-3.0-only
// The HTTPS task's timetable (https_task.cpp), checked on the PC by
// test/test_https. Each job runs in turn, at the time it gave after its last
// turn, or as soon as another task asks for it (wake).
#pragma once

#include <stdint.h>

enum HttpsJob : uint8_t {
  HTTPS_WEATHER,   // MET Norway's forecast and the city lookups (weather.cpp)
  HTTPS_UPDATE,    // the update check and install (updater.cpp)
  HTTPS_SOCIAL,    // the YouTube and GitHub counters (social.cpp)
  HTTPS_JOBS,
};

// The task looks at least this often, a job due or not: the city table is
// freed once unused (PLACES_IDLE_MS) and a job held back by the Wi-Fi or an
// update looks again
static const uint32_t HTTPS_LOOK_MS = 60000;

class HttpsSchedule {
 public:
  // The job's next turn, ms after now
  void after(HttpsJob job, uint32_t now, uint32_t ms);
  // Its turn as soon as the job running ends, whatever its time: the owner
  // changed the city, or asked for an update check
  void wake(HttpsJob job);
  // True when the job's turn has come; asked says whether it was woken, and
  // the ask is taken with it. The time stays until after() gives the next.
  bool take(HttpsJob job, uint32_t now, bool &asked);
  // Until the soonest turn: 0 when one is due, HTTPS_LOOK_MS at most
  uint32_t waitMs(uint32_t now) const;

 private:
  // A wait from a moment: now - from never misreads, whatever the uptime
  uint32_t from_[HTTPS_JOBS] = {}, ms_[HTTPS_JOBS] = {};
  bool asked_[HTTPS_JOBS] = {};
};
