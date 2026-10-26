// SPDX-License-Identifier: AGPL-3.0-only
#include "https_task.h"

#include <Arduino.h>

#include "places.h"
#include "social.h"
#include "updater.h"
#include "weather.h"

static TaskHandle_t task = nullptr;

// What a job asks for in the task's notification bits; a search, none
static uint32_t bitOf(HttpsJob job) { return 1UL << job; }

// The job's turn; returns the ms until its next one
static uint32_t run(HttpsJob job, bool asked) {
  switch (job) {
    case HTTPS_WEATHER: return weatherStep(asked);
    case HTTPS_UPDATE:  return updateStep(asked);
    case HTTPS_SOCIAL:  return socialStep();
    default:            return HTTPS_LOOK_MS;
  }
}

static void httpsTask(void *) {
  HttpsSchedule plan;
  const uint32_t start = millis();
  plan.after(HTTPS_WEATHER, start, 0);       // waits for the Wi-Fi itself
  plan.after(HTTPS_UPDATE, start, 60000);
  plan.after(HTTPS_SOCIAL, start, 9000);
  for (;;) {
    for (int j = 0; j < HTTPS_JOBS; j++) {
      // Before each job: a search waits for one session at most (a
      // forecast takes up to 35 s, an update's download minutes)
      placesServe();
      const HttpsJob job = (HttpsJob)j;
      bool asked;
      if (plan.take(job, millis(), asked)) plan.after(job, millis(), run(job, asked));
    }
    uint32_t bits = 0;
    xTaskNotifyWait(0, UINT32_MAX, &bits, pdMS_TO_TICKS(plan.waitMs(millis())));
    for (int j = 0; j < HTTPS_JOBS; j++)
      if (bits & bitOf((HttpsJob)j)) plan.wake((HttpsJob)j);
  }
}

void httpsBegin() {
  // HTTPS needs a large stack: 12 KB. Priority 0, the idle task's:
  // checking certificate signatures is seconds of pure computation on this
  // chip, and above idle it starved IDLE0 until the task watchdog reset the
  // board.
  xTaskCreatePinnedToCore(httpsTask, "https", 12288, nullptr, tskIDLE_PRIORITY, &task, 0);
}

void httpsAsk(HttpsJob job) {
  if (task) xTaskNotify(task, bitOf(job), eSetBits);
}

void httpsWake() {
  if (task) xTaskNotify(task, 0, eNoAction);
}
