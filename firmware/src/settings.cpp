// SPDX-License-Identifier: AGPL-3.0-only
#include "settings.h"

#include <Preferences.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <nvs_flash.h>

#include "content.h"
#include "i18n.h"
#include "network.h"
#include "settings_logic.h"

Settings settings;

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// One instance per call: the web task and the HTTPS task both save.
void loadSettings() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  settings.salary         = prefs.getFloat("salaire",   settings.salary);
  settings.workStart      = prefs.getUChar("hDebut",    settings.workStart);
  settings.workEnd        = prefs.getUChar("hFin",      settings.workEnd);
  settings.workDays       = prefs.getUChar("joursMois", settings.workDays);
  settings.instaFollowers = prefs.getULong("insta", 0);
  settings.xFollowers     = prefs.getULong("x",     0);
  prefs.getString("ytCle",    settings.ytKey,     sizeof(settings.ytKey));
  prefs.getString("ytChaine", settings.ytChannel, sizeof(settings.ytChannel));
  prefs.getString("ghUser",   settings.ghUser,    sizeof(settings.ghUser));
  settings.lang = prefs.getUChar("lang", LANG_EN);
  settings.nightStart      = prefs.getUChar("nightFrom",  settings.nightStart);
  settings.nightEnd        = prefs.getUChar("nightTo",    settings.nightEnd);
  settings.nightBrightness = prefs.getUChar("nightLevel", settings.nightBrightness);
  prefs.getString("message", settings.message, sizeof(settings.message));
  settings.autoUpdate = prefs.getBool("autoUpdate", settings.autoUpdate);
  prefs.getString("mqttHost", settings.mqttHost, sizeof(settings.mqttHost));
  settings.mqttPort = prefs.getUShort("mqttPort", settings.mqttPort);
  prefs.getString("mqttUser", settings.mqttUser, sizeof(settings.mqttUser));
  prefs.getString("mqttPass", settings.mqttPass, sizeof(settings.mqttPass));
  prefs.getString("city",  settings.city,  sizeof(settings.city));
  prefs.getString("place", settings.place, sizeof(settings.place));
  settings.lat       = prefs.getFloat("lat", atof(METEO_LATITUDE));
  settings.lon       = prefs.getFloat("lon", atof(METEO_LONGITUDE));
  prefs.getString("zone", settings.zone, sizeof(settings.zone));
  settings.utcOffset = prefs.getInt("utcOffset", UTC_OFFSET_UNKNOWN);
  readScreens(prefs, settings);
  readStyles(prefs, settings);
  readRegion(prefs, settings);
  prefs.end();
  setLanguage(settings.lang);
}

// False when the settings were not stored whole: a locked panel (NVS closed,
// factory reset unfinished), storage that would not open, or a write NVS
// refused (full). With before, both the RAM settings and what was stored go
// back to it, so nothing runs or restarts with half of the form. The caller
// says so.
bool saveSettings(const Settings *before) {
  Preferences prefs;
  bool saved = !panelLocked() && prefs.begin(NVS_NAMESPACE, false);
  if (saved) {
    saved = storeSettings(prefs, settings, before);
    prefs.end();
  }
  if (!saved) {
    Serial.println("[CONFIG] settings not saved");
    if (before) settings = *before;
    return false;
  }
  setLanguage(settings.lang);
  Serial.println("[CONFIG] saved");
  return true;
}

void savePhoneRegion(const RegionChoice &phone) {
  Preferences prefs;
  if (panelLocked() || !prefs.begin(NVS_NAMESPACE, false)) return;
  if (!presetRegion(prefs, settings, phone)) Serial.println("[CONFIG] phone region not saved");
  prefs.end();
}

uint32_t countBoot() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  const uint32_t n = prefs.getULong("boots", 0) + 1;
  prefs.putULong("boots", n);
  prefs.end();
  return n;
}

// --- Factory reset ----------------------------------------------------------------
//
// It is what a panel goes through before it is sold or given away: the
// previous owner's photos and secrets must not survive it. The order and the
// stops are runFactoryReset()'s (content_logic, host-tested); here are the
// flash steps. A durable marker comes first and is cleared last, so a power
// cut anywhere in between is resumed at the next start, and the reset is
// never taken as done before every erase was read back. Kept on purpose:
// app0, app1 (public firmware images) and otadata (which slot boots), with
// the bootloader and the partition table: no user data, and erasing them
// would brick the panel.

static PanelLock lock_ = LOCK_NONE;

PanelLock panelLock() { return lock_; }
bool panelLocked() { return lock_ != LOCK_NONE; }

// A development build with -DRESET_FAIL_STEP=N (1-5) fails step N
// without running it. Never set in platformio.ini, so no release has it.
static bool failForTest(int step) {
#ifdef RESET_FAIL_STEP
  if (step == RESET_FAIL_STEP) {
    Serial.printf("[CONFIG] test build: reset step %d fails\n", step);
    return true;
  }
#else
  (void)step;
#endif
  return false;
}

class FlashReset : public ResetOps {
public:
  bool marked = false;
  bool markPending() override {
    marked = !failForTest(1) && contentMarkReset();
    if (!marked) Serial.println("[CONFIG] reset marker not written");
    return marked;
  }
  bool eraseImages() override { return !failForTest(2) && contentErase(); }
  // A crash dump is a copy of RAM: the Wi-Fi password, the session and API
  // secrets, the MQTT password. A table without one has nothing to erase.
  bool eraseCrashDump() override {
    if (failForTest(3)) return false;
    const esp_partition_t *p = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    return !p || flashEraseVerified(p, 0, p->size);
  }
  // The whole partition: our settings, the saved sessions, and the copy of
  // the Wi-Fi credentials the radio driver keeps on its own. The most
  // sensitive data of all, so read back blank as the crash dump is, not
  // taken on a return code. nvs_flash_erase() closes NVS first.
  bool eraseSettings() override {
    if (failForTest(4) || nvs_flash_erase() != ESP_OK) return false;
    const esp_partition_t *p = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
    return p && flashEraseVerified(p, 0, p->size);
  }
  bool clearPending() override { return !failForTest(5) && contentClearReset(); }
};

// Closes NVS for the rest of this start: every reader gets its
// defaults and every write is refused at one place, where patching each
// reader would miss the next one. The radio must already be started.
static void lockPanel(PanelLock origin) {
  lock_ = origin;
  // IDF 4.4 returns only ESP_OK or "not initialised": both leave NVS closed.
  // Anything else and NVS may still be open: stop here rather than serve it.
  const esp_err_t closed = nvs_flash_deinit();
  if (closed != ESP_OK && closed != ESP_ERR_NVS_NOT_INITIALIZED) {
    Serial.printf("[CONFIG] NVS could not be closed (0x%x): halted\n", (unsigned)closed);
    while (true) delay(1000);
  }
}

#ifdef SCREENS_TEST_CONSOLE
// The locked pages can then be checked on a panel without a flash that
// refuses an erase: nothing is erased, no marker is written, and the
// restart ("x") unlocks
void settingsTestLock(bool knob) { lockPanel(knob ? LOCK_KNOB : LOCK_MARKER); }
#endif

void factoryReset() {
  Serial.println("[CONFIG] factory reset");
  // The radio first, while NVS is open: step 4 closes it, and the driver
  // cannot start without it, so a reset that ends locked would have no setup
  // access point. Its saved network is wiped in RAM; a restart drops it.
  networkForgetSaved();
  FlashReset steps;
  // Without a marker every erase ran, even after one of them failed.
  const bool done = runFactoryResetNow(steps);
  switch (knobResetEnd(done, steps.marked)) {
    case KR_DONE:
      Serial.println("[CONFIG] factory reset done");
      break;
    case KR_RESUME:
      Serial.println("[CONFIG] factory reset incomplete, resumed at next start");
      break;
    case KR_LOCKED:
      // No marker: a restart would be a normal start on whatever survived.
      // Locked on this start instead, as a start whose resume fails.
      Serial.println("[CONFIG] factory reset incomplete, no marker: panel locked");
      lockPanel(LOCK_KNOB);
      settings = Settings();   // read before the knob: the previous owner's
      loadSettings();          // keys and passwords; NVS closed, defaults now
      return;
  }
  Serial.flush();
  ESP.restart();
  while (true) {}
}

bool resumeFactoryReset() {
  FlashReset steps;
  const MarkerState marker = contentResetMarker();
  if (marker != MARK_ABSENT) {
    Serial.println("[CONFIG] factory reset pending, resuming");
    networkForgetSaved();   // while NVS is open, as in factoryReset()
  }
  switch (resetAtBoot(marker, steps)) {
    case RB_NORMAL:
      return false;
    case RB_DONE:
      Serial.println("[CONFIG] factory reset done");
      Serial.flush();
      ESP.restart();
      while (true) {}
    case RB_LOCKED:
      break;
  }
  // The previous owner's settings and secrets may still be in NVS.
  // Restarting instead would only loop: the flash refused once already.
  Serial.println("[CONFIG] factory reset incomplete: panel locked");
  lockPanel(LOCK_MARKER);
  return true;
}

const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "power";
    case ESP_RST_SW:        return "software";      // update, setting, serial x
    case ESP_RST_PANIC:     return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_EXT:       return "reset button";
    default:                return "other";
  }
}
