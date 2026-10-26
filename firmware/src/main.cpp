// SPDX-License-Identifier: AGPL-3.0-only
/*
 * Carousel - HUB75 64x32 (P3) LED panel, screen gallery
 * Target: ESP32-WROOM
 *
 *   Encoder       -> changes screen
 *   Button (BT)   -> changes brightness (switches session on the agent monitor);
 *                    held 0.8 s: where the pages are (address, IP, QR code);
 *                    held at boot: Wi-Fi setup, or factory reset after 10 s
 *   Tilt switch   -> toggles landscape 64x32 <-> portrait 32x64
 *
 * Core 1 runs loop() and draws; core 0 runs the web server and the HTTPS
 * task (weather, updates, social counters). See board.h for the pin map
 * and the power warning.
 */

#include <Arduino.h>

#include "board.h"
#include "boot_task.h"
#include "agent_bridge.h"
#include "content.h"
#include "display.h"
#include "gallery.h"
#include "https_task.h"
#include "i18n.h"
#include "input.h"
#include "mqtt.h"
#include "network.h"
#include "ota.h"
#include "screens/screens.h"
#include "settings.h"
#include "updater.h"
#include "web.h"

#define SETUP_HOLD_MS   3000UL    // released before: Wi-Fi setup
#define RESET_HOLD_MS   10000UL   // held this long: factory reset

// Encoder button held at boot. Released within 3 s: Wi-Fi setup right away.
// Held on: the panel turns red and counts down to a factory reset, which can
// still be cancelled by letting go. Returns whether it was held at all.
static bool bootButtonHeld() {
  if (digitalRead(ENC_SW_PIN) != LOW) return false;
  bootTaskEnd();   // the mark gives way: the frame is this screen's
  const uint32_t start = millis();
  uint32_t releasedAt = 0;
  while (true) {
    const uint32_t now = millis();
    if (digitalRead(ENC_SW_PIN) == LOW) releasedAt = 0;
    else if (!releasedAt) releasedAt = now;
    else if (now - releasedAt > 100) break;          // let go, contact bounce aside
    const uint32_t held = now - start;
    if (held >= RESET_HOLD_MS) {
      factoryReset();   // returns only when it left the panel locked
      return true;
    }

    const bool reset = held >= SETUP_HOLD_MS;
    const uint16_t c = reset ? display->color565(255, 60, 40) : display->color565(255, 150, 20);
    display->clearScreen();
    display->setTextSize(1);
    display->setTextColor(c);
    display->setCursor(1, 4);
    display->print(reset ? tr().factoryReset : tr().setup);
    if (reset) {
      display->setCursor(1, 14);
      display->print((RESET_HOLD_MS - held) / 1000 + 1);
      display->fillRect(0, H() - 3, W() * (held - SETUP_HOLD_MS) / (RESET_HOLD_MS - SETUP_HOLD_MS), 3, c);
    }
    displayPush();   // before the gallery runs: no frame push yet
    delay(30);
  }
  return true;
}

void setup() {
  displayHoldDark();   // before anything: the panel's pins float until then
  Serial.begin(115200);   // no wait for a monitor: the panel lights first
  Serial.println();
  Serial.println("=== Carousel - HUB75 64x32 ===");
  Serial.println("version " FIRMWARE_VERSION);
  // Before the boot counter or any setting is read: an unfinished factory
  // reset is finished first, or locks the panel for this start
  const bool locked = resumeFactoryReset();

  Serial.printf("boot #%lu, last reset: %s\n", (unsigned long)countBoot(), resetReason());
  Serial.flush();

  randomSeed(esp_random());
  inputBegin();

  if (!displayBegin()) {
    // Nothing can be drawn without it. Retrying beats a dark panel forever;
    // the boot counter shows a loop if it keeps failing. Not when locked:
    // the radio started first there, and each start would run the reset's
    // full-partition erases again. It stays up dark instead (every draw is
    // a no-op without the frame's buffer): serial log and setup access point.
    if (locked) {
      Serial.println("ERROR: panel memory allocation failed, panel locked: running without display");
    } else {
      Serial.println("ERROR: panel memory allocation failed, restarting");
      delay(5000);
      ESP.restart();
    }
  }
  loadSettings();
  // The mark lights now, at the stored brightness and the right way up, and
  // fades on its own task while the starts below run: it used to wait for
  // loop()'s first frame, after all of them. A locked panel shows RESET
  // FAILED instead, from loop().
  galleryPanelBegin();
  if (!locked) bootTaskBegin();
  // Images: after the display (its DMA buffers and frame come first), before the gallery
  contentBegin();
  galleryBegin();

  // Encoder button held during boot: Wi-Fi setup right away, without
  // waiting for the connection to fail, or factory reset if held on. A
  // locked panel has no network to join: setup right away too.
  const bool held = bootButtonHeld();
  networkBegin(locked || held);

  // The queue before the server: the first request can arrive right away.
  agentBridgeBegin();
  webBegin();
  updateBegin();
  httpsBegin();

  printPanelTuning();
  // The frame is loop()'s from its first frame: the task's last one is pushed
  bootTaskEnd();
  // A loop() stuck for 5 s restarts the board instead of freezing the panel
  enableLoopWDT();
}

void loop() {
  serviceSerialConsole();
  handleInputs();
  networkUpdate();
  otaHandle();
  otaConfirmWhenHealthy();
  mqttLoop();
  processAgentEvents();
  updateSessions();

  drawFrame();

  // The web server runs in its own task, on the other core: the wait between
  // two frames no longer has to serve it. Hooks that arrived are still
  // applied here, so they show from the next frame.
  const uint32_t frameStart = millis();
  while (millis() - frameStart < 12) {
    processAgentEvents();
    delay(1);
  }
}
