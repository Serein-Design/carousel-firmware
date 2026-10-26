// SPDX-License-Identifier: AGPL-3.0-only
#include "ota.h"

#include <ArduinoOTA.h>
#include <esp_ota_ops.h>

#include "display.h"
#include "network.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""   // secrets.h from before OTA existed: OTA stays off
#endif

volatile bool otaInProgress = false;

static bool started = false;

void otaBegin() {
  if (started) return;
  // Anyone on the network could flash the board: no password, no OTA.
  if (!OTA_PASSWORD[0]) {
    Serial.println("[OTA] disabled: set OTA_PASSWORD in secrets.h");
    started = true;
    return;
  }
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  // network.cpp already runs the mDNS responder; a second MDNS.begin()
  // would restart it.
  ArduinoOTA.setMdnsEnabled(false);
  // The default 1 s gives up on the first stall. Here the panel's own lines
  // cost Wi-Fi packets, TCP retransmits with growing delays, and half the
  // uploads died between 4 and 49 % although the link would have recovered.
  ArduinoOTA.setTimeout(10000);

  // A progress bar, no text: the update runs inside handle(), on the display
  // loop, so nothing else is drawn meanwhile, and no frame is pushed: the
  // callbacks push their own.
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    // The whole upload runs inside handle(), waiting up to 10 s on a stall:
    // longer than the loop watchdog. The board restarts at the end anyway.
    disableLoopWDT();
    Serial.println("[OTA] update started");
    display->clearScreen();
    display->drawRect(1, H() / 2 - 3, W() - 2, 6, display->color565(60, 60, 60));
    displayPush();
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    display->fillRect(2, H() / 2 - 2, (W() - 4) * done / total, 4, display->color565(30, 215, 96));
    displayPush();
  });
  ArduinoOTA.onEnd([]() { Serial.println("[OTA] done, rebooting"); });
  ArduinoOTA.onError([](ota_error_t e) {
    otaInProgress = false;
    enableLoopWDT();
    Serial.printf("[OTA] error %u\n", (unsigned)e);
    display->clearScreen();
  });

  ArduinoOTA.begin();
  started = true;
  Serial.println("[OTA] ready on port 3232");
}

void otaHandle() {
  if (started && OTA_PASSWORD[0]) ArduinoOTA.handle();
}

#define CONFIRM_AFTER_MS 30000UL

// Arduino core hook: without it, the core confirms a new firmware the moment
// it starts, and a broken update would stay. Declared in a C file: without
// extern "C" this would be another function and the core's would win.
extern "C" bool verifyRollbackLater() { return true; }

void otaConfirmWhenHealthy() {
  static bool told = false, done = false;
  if (!told) {                         // once, at boot
    told = true;
    esp_ota_img_states_t state;
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY)
      Serial.printf("[OTA] new firmware on trial in %s\n", running->label);
    if (esp_ota_get_last_invalid_partition())
      Serial.println("[OTA] a previous update failed and was rolled back");
  }
  if (done || networkState != NET_SYNCED || millis() < CONFIRM_AFTER_MS) return;
  done = true;
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("[OTA] new firmware confirmed");
  }
}
