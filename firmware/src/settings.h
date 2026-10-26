// SPDX-License-Identifier: AGPL-3.0-only
// Persistent user settings (NVS). Everything is set from the web page,
// nothing is hard-coded. Values survive power cuts and reflashing.
#pragma once

#include <Arduino.h>

#include "screen_list.h"
#include "screen_style.h"
#include "feedback_style.h"
#include "region.h"

// Kept as is: existing panels store their settings under these names.
#define NVS_NAMESPACE "panneau"

#define UTC_OFFSET_UNKNOWN 1    // real offsets are multiples of 15 min

struct Settings {
  float    salary          = 2000.0f;   // net monthly, in euros
  uint8_t  workStart       = 9;         // start of the working day
  uint8_t  workEnd         = 17;        // end
  uint8_t  workDays        = 21;        // working days per month
  char     ytKey[72]       = "";        // YouTube Data v3 API key
  char     ytChannel[40]   = "";        // channel id, starts with UC
  char     ghUser[40]      = "";        // GitHub user name
  uint32_t instaFollowers  = 0;         // manual entry: closed API
  uint32_t xFollowers      = 0;         // manual entry: paid API
  uint8_t  lang            = 0;         // Lang, English by default
  // Night: from nightStart to nightEnd (hours, may cross midnight), the
  // panel drops to nightBrightness (0 = off). Same hours = no night mode.
  uint8_t  nightStart      = 0;
  uint8_t  nightEnd        = 0;
  uint8_t  nightBrightness = 10;
  bool     autoUpdate      = true;      // install signed releases at night
  char     mqttHost[64]    = "";        // broker; empty = MQTT off (mqtt.h)
  uint16_t mqttPort        = 1883;
  char     mqttUser[32]    = "";
  char     mqttPass[64]    = "";
  char     message[96]     = "HELLO * CAROUSEL * ";   // TEXT screen, UTF-8 (toMessageText)
  // Location: weather, time zone and ISS distance follow it. city is what
  // the user typed or chose (or the phone's time zone city, at Wi-Fi
  // setup); place is what the city table has for it, empty until then.
  char     city[48]        = "";
  char     place[64]       = "";
  float    lat             = 0;         // set from secrets.h by loadSettings
  float    lon             = 0;
  // The place's time zone as a POSIX TZ, daylight saving included: the
  // city table's for the place (places.cpp), or the phone's at Wi-Fi setup
  // (tz_names); empty until either is known
  char     zone[48]        = "";
  // Up to v0.2.0 the forecast's UTC offset, no DST rule: read at start
  // while zone is empty, no longer written
  int32_t  utcOffset       = UTC_OFFSET_UNKNOWN;
  // Screens: the owner's list (keys "scrList", by English name) and the
  // rotation ("rotateSec"). Both absent until the settings form first saves
  // them; absent = screenListDefault(), rotation off.
  ScreenList screens       = screenListDefault();
  uint16_t   rotateSec     = 0;      // 0 = with the knob only, else one of ROTATE_CHOICES
  // Written on every save of the settings form: the keys exist and hold a
  // list used as stored, or the form sent them. False while a fill at start
  // is in force, so that only the owner's save replaces what is stored.
  bool       screensStored = false;
  // Screens the start turned on in RAM because their orientation had none:
  // the stored list is screens with these off (put back on a refused save)
  uint32_t   screensFilled = 0;
  // How CODE, CLOCK and WEATHER draw (screen_style.h): set on Running order
  uint8_t    codeStyle     = CODE_AUTO;
  uint8_t    clockStyle    = CLOCK_STONE;
  uint8_t    clockShow     = CLOCK_SHOW_DEFAULT;
  uint8_t    weatherStyle  = WEATHER_GLANCE;
  // How the panel answers (feedback_style.h): set on Panel
  uint8_t    transition    = TRANSITION_PUSH;
  uint8_t    brightnessFeedback = BRIGHTNESS_DOTS;
  uint8_t    knobFeedback  = KNOB_BAND;
  uint8_t    bootDetail    = BOOT_AUTO;   // set on Panel, under Advanced
  uint8_t    requestShow[REQUEST_KIND_COUNT] = {REQUEST_SHOW, REQUEST_SHOW, REQUEST_SHOW, REQUEST_SHOW};
  // Region (Panel page): v0.1.0 panels had °C, euros and a 24 h clock
  uint8_t    tempUnit      = TEMP_C;     // TempUnit
  uint8_t    currency      = CURRENCY_EUR;   // Currency
  bool       clock12       = false;      // 12 h clock with AM/PM
};
extern Settings settings;

// Is hour h inside [start, end)? The window may cross midnight (23 -> 7).
inline bool inHourWindow(int h, int start, int end) {
  if (start == end) return false;
  return start < end ? (h >= start && h < end) : (h >= start || h < end);
}

void loadSettings();
// False: not stored whole (panel locked, or NVS refused a write). With
// before (the settings form only), the screen keys are written too, and the
// settings and NVS go back to before on a false. Without it, the screen keys
// stay as stored.
bool saveSettings(const Settings *before = nullptr);
// The Wi-Fi setup's phone region, for each choice no key holds yet
// (presetRegion, settings_logic.h); nothing on a locked panel
void savePhoneRegion(const RegionChoice &phone);

// Boot counter kept in NVS. If it climbs by several units between two reads,
// the board is boot-looping - typically a supply that cannot hold the radio
// power-up peak.
uint32_t countBoot();

// Why the board last restarted: "power", "software", "crash", "watchdog",
// "brownout" (the supply sagged)...
const char *resetReason();

// Erases every stored image and the crash dump, then every setting (Wi-Fi
// password included); keeps the firmware slots and otadata (no user data),
// then restarts. Fail-closed: cut or failed half-way, it is resumed at the
// next start and never reported done. If its marker cannot be written,
// nothing could resume it: every erase runs anyway, even after one failed,
// and if one did, it returns with the panel locked for this start (as
// resumeFactoryReset()) instead of restarting. Setup only, from the knob,
// before the network, the web server and the secrets start.
void factoryReset();

// First thing in setup(), before any setting is read: finishes a reset left
// pending (then restarts). True = still unfinished: the panel is locked for
// this start (NVS closed, no saved Wi-Fi), and no restart loop follows.
bool resumeFactoryReset();

// How this start got locked, for the page's text: by a reset marker at boot
// (present or unreadable: every start resumes it), or by a knob reset that
// could not write its marker (KR_LOCKED, also every knob failure on a table
// without an image area: nothing resumes it, the knob must be held again).
enum PanelLock : uint8_t { LOCK_NONE, LOCK_MARKER, LOCK_KNOB };
PanelLock panelLock();
bool panelLocked();   // panelLock() != LOCK_NONE
