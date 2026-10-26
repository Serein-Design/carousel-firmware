// SPDX-License-Identifier: AGPL-3.0-only
#include "settings_logic.h"

// A field contract, as every key in NVS_NAMESPACE: panels store the owner's
// list under these names (NVS allows 15 characters), never renamed or retyped.
static const char *const KEY_SCREEN_LIST = "scrList";
static const char *const KEY_ROTATE      = "rotateSec";
// The styles (screen_style.h), one byte each, as the enums number them
static const char *const KEY_CODE_STYLE    = "codeStyle";
static const char *const KEY_CLOCK_STYLE   = "clkStyle";
static const char *const KEY_CLOCK_SHOW    = "clkShow";
static const char *const KEY_WEATHER_STYLE = "wxStyle";
// The feedback choices (feedback_style.h): one byte each, and the four
// RequestShow bytes in one blob, indexed by RequestKind
static const char *const KEY_TRANSITION = "transition";
static const char *const KEY_BRIGHTNESS = "briFb";
static const char *const KEY_KNOB       = "knobFb";
static const char *const KEY_BOOT       = "bootDetail";
static const char *const KEY_REQUESTS   = "reqShow";
// The region (region.h), one byte each: the unit and currency as the enums
// number them, the 12 h clock as 0 or 1
static const char *const KEY_TEMP_UNIT = "tempUnit";
static const char *const KEY_CURRENCY  = "currency";
static const char *const KEY_CLOCK12   = "clock12";

// put*() return the bytes written, 0 when NVS refused the write (a full NVS:
// the value it replaces stays). Each one commits on its own, so a refusal
// half-way leaves the earlier keys new and the later ones old.
static bool landed(const char *key, size_t written) {
  if (!written) Serial.printf("[CONFIG] %s not written\n", key);
  return written != 0;
}

#ifdef SCREENS_TEST_CONSOLE
// Development builds only: the serial console's "w" makes NVS refuse the
// interval, to watch the settings form put the stored list back on a real
// panel without filling its flash. Volatile: set by the loop, read by the
// web, weather and MQTT tasks.
static volatile bool refuseRotate = false;
void settingsRefuseRotateWrites() { refuseRotate = true; }
#endif

static bool put(Preferences &p, const char *key, float v)    { return landed(key, p.putFloat(key, v)); }
static bool put(Preferences &p, const char *key, bool v)     { return landed(key, p.putBool(key, v)); }
static bool put(Preferences &p, const char *key, uint8_t v)  { return landed(key, p.putUChar(key, v)); }
static bool put(Preferences &p, const char *key, uint16_t v) {
#ifdef SCREENS_TEST_CONSOLE
  if (refuseRotate && !strcmp(key, KEY_ROTATE)) return landed(key, 0);
#endif
  return landed(key, p.putUShort(key, v));
}
static bool put(Preferences &p, const char *key, uint32_t v) { return landed(key, p.putULong(key, v)); }

// putString() returns the text's length, so 0 for an empty text even when it
// was stored: that one is read back instead.
static bool put(Preferences &p, const char *key, const char *v) {
  if (*v) return landed(key, p.putString(key, v) == strlen(v));
  p.putString(key, v);
  return landed(key, p.isKey(key) && p.getString(key).length() == 0);
}

void readScreens(Preferences &p, Settings &s) {
  s.screensStored = p.isKey(KEY_SCREEN_LIST) || p.isKey(KEY_ROTATE);
  s.screens = screenListDefault();
  if (p.isKey(KEY_SCREEN_LIST)) {
    char text[SCREEN_LIST_TEXT_MAX];
    if (p.getString(KEY_SCREEN_LIST, text, sizeof(text))) s.screens = screenListParse(text);
    else Serial.println("[SCREENS] stored list unreadable: the default list");
  }
  const uint16_t r = p.getUShort(KEY_ROTATE, 0);
  s.rotateSec = rotateOffered(r) ? r : 0;
  const uint32_t stored = s.screens.on;
  s.screensFilled = 0;
  if (screenListFill(s.screens)) {
    // Not written back by the weather, MQTT or the API's message: the
    // owner's off screens stay stored until the owner saves the form
    s.screensFilled = s.screens.on & ~stored;
    s.screensStored = false;
    Serial.println("[SCREENS] an orientation had no screen on: all of its screens on until a save");
  }
}

// A later firmware may offer more styles: this one draws its default for those
static uint8_t styleRead(Preferences &p, const char *key, uint8_t count, uint8_t fallback) {
  const uint8_t v = p.getUChar(key, fallback);
  return v < count ? v : fallback;
}

void readStyles(Preferences &p, Settings &s) {
  s.codeStyle    = styleRead(p, KEY_CODE_STYLE, CODE_STYLE_COUNT, CODE_AUTO);
  s.clockStyle   = styleRead(p, KEY_CLOCK_STYLE, CLOCK_STYLE_COUNT, CLOCK_STONE);
  s.clockShow    = p.getUChar(KEY_CLOCK_SHOW, CLOCK_SHOW_DEFAULT) & CLOCK_SHOW_ALL;
  s.weatherStyle = styleRead(p, KEY_WEATHER_STYLE, WEATHER_STYLE_COUNT, WEATHER_GLANCE);
  s.transition = styleRead(p, KEY_TRANSITION, TRANSITION_COUNT, TRANSITION_PUSH);
  s.brightnessFeedback = styleRead(p, KEY_BRIGHTNESS, BRIGHTNESS_FEEDBACK_COUNT, BRIGHTNESS_DOTS);
  s.knobFeedback = styleRead(p, KEY_KNOB, KNOB_FEEDBACK_COUNT, KNOB_BAND);
  s.bootDetail = styleRead(p, KEY_BOOT, BOOT_DETAIL_COUNT, BOOT_AUTO);
  // A blob of another length is not ours: every kind shows
  uint8_t req[REQUEST_KIND_COUNT];
  const bool whole = p.getBytesLength(KEY_REQUESTS) == sizeof(req) &&
                     p.getBytes(KEY_REQUESTS, req, sizeof(req)) == sizeof(req);
  for (int k = 0; k < REQUEST_KIND_COUNT; k++)
    s.requestShow[k] = whole && req[k] < REQUEST_SHOW_COUNT ? req[k] : (uint8_t)REQUEST_SHOW;
}

void readRegion(Preferences &p, Settings &s) {
  s.tempUnit = styleRead(p, KEY_TEMP_UNIT, TEMP_UNIT_COUNT, TEMP_C);
  s.currency = styleRead(p, KEY_CURRENCY, CURRENCY_COUNT, CURRENCY_EUR);
  s.clock12  = styleRead(p, KEY_CLOCK12, 2, 0) == 1;
}

bool presetRegion(Preferences &p, Settings &s, const RegionChoice &phone) {
  bool ok = true;
  auto fill = [&](const char *key, int8_t value, uint8_t &field) {
    if (value < 0 || p.isKey(key)) return;
    if (put(p, key, (uint8_t)value)) field = (uint8_t)value;
    else ok = false;
  };
  fill(KEY_TEMP_UNIT, phone.unit, s.tempUnit);
  fill(KEY_CURRENCY, phone.currency, s.currency);
  uint8_t clock12 = s.clock12 ? 1 : 0;
  fill(KEY_CLOCK12, phone.clock12, clock12);
  s.clock12 = clock12 == 1;
  return ok;
}

static bool putScreens(Preferences &p, const ScreenList &l, uint16_t rotateSec) {
  char text[SCREEN_LIST_TEXT_MAX];
  bool ok = screenListFormat(l, text, sizeof(text)) ? put(p, KEY_SCREEN_LIST, (const char *)text)
                                                    : landed(KEY_SCREEN_LIST, 0);
  ok &= put(p, KEY_ROTATE, rotateSec);
  return ok;
}

// Every key but the screen list's
static bool writeFields(Preferences &p, const Settings &s) {
  bool ok = true;
  ok &= put(p, "salaire",    s.salary);
  ok &= put(p, "hDebut",     s.workStart);
  ok &= put(p, "hFin",       s.workEnd);
  ok &= put(p, "joursMois",  s.workDays);
  ok &= put(p, "insta",      s.instaFollowers);
  ok &= put(p, "x",          s.xFollowers);
  ok &= put(p, "ytCle",      s.ytKey);
  ok &= put(p, "ytChaine",   s.ytChannel);
  ok &= put(p, "ghUser",     s.ghUser);
  ok &= put(p, "lang",       s.lang);
  ok &= put(p, "nightFrom",  s.nightStart);
  ok &= put(p, "nightTo",    s.nightEnd);
  ok &= put(p, "nightLevel", s.nightBrightness);
  ok &= put(p, "message",    s.message);
  ok &= put(p, "autoUpdate", s.autoUpdate);
  ok &= put(p, "mqttHost",   s.mqttHost);
  ok &= put(p, "mqttPort",   s.mqttPort);
  ok &= put(p, "mqttUser",   s.mqttUser);
  ok &= put(p, "mqttPass",   s.mqttPass);
  ok &= put(p, "city",       s.city);
  ok &= put(p, "place",      s.place);
  ok &= put(p, "lat",        s.lat);
  ok &= put(p, "lon",        s.lon);
  ok &= put(p, "zone",       s.zone);
  ok &= put(p, KEY_CODE_STYLE,    s.codeStyle);
  ok &= put(p, KEY_CLOCK_STYLE,   s.clockStyle);
  ok &= put(p, KEY_CLOCK_SHOW,    s.clockShow);
  ok &= put(p, KEY_WEATHER_STYLE, s.weatherStyle);
  ok &= put(p, KEY_TRANSITION, s.transition);
  ok &= put(p, KEY_BRIGHTNESS, s.brightnessFeedback);
  ok &= put(p, KEY_KNOB,       s.knobFeedback);
  ok &= put(p, KEY_BOOT,       s.bootDetail);
  ok &= put(p, KEY_TEMP_UNIT, s.tempUnit);
  ok &= put(p, KEY_CURRENCY,  s.currency);
  ok &= put(p, KEY_CLOCK12,   (uint8_t)(s.clock12 ? 1 : 0));
  ok &= landed(KEY_REQUESTS, p.putBytes(KEY_REQUESTS, s.requestShow, sizeof(s.requestShow)));
  return ok;
}

bool writeSettings(Preferences &p, const Settings &s) {
  bool ok = writeFields(p, s);
  // Only once the settings form has sent them: until then a panel follows
  // the firmware's list, new screens included in their own place.
  if (s.screensStored) ok &= putScreens(p, s.screens, s.rotateSec);
  return ok;
}

bool storeSettings(Preferences &p, const Settings &next, const Settings *before) {
  // Without before: the weather, MQTT, the API's message or the Wi-Fi page,
  // none of which changes the list. Leaving its keys alone means a save that
  // lands while a refused form save still holds the posted list in RAM
  // cannot write that list back, and the list's 256-byte text is built only
  // on the web task, never on their smaller stacks (esp-mqtt's 6 KB).
  if (!before) return writeFields(p, next);
  if (writeSettings(p, next)) return true;
  // Best effort: on a full NVS the old values may not all fit back either
  bool back = writeSettings(p, *before);
  if (next.screensStored && !before->screensStored) {
    if (before->screensFilled) {
      // Filled at start: the list goes back as stored, not as shown
      ScreenList stored = before->screens;
      stored.on &= ~before->screensFilled;
      back &= putScreens(p, stored, before->rotateSec);
    } else {
      // The form's first list: before had none, so neither key may stay
      for (const char *key : {KEY_SCREEN_LIST, KEY_ROTATE})
        if (p.isKey(key) && !p.remove(key)) back = false;
    }
  }
  if (!back) Serial.println("[CONFIG] previous settings not written back");
  return false;
}

void keepLocation(Settings &posted, const Settings &now, bool moved) {
  if (moved) return;
  memcpy(posted.place, now.place, sizeof(posted.place));
  memcpy(posted.zone, now.zone, sizeof(posted.zone));
  posted.lat = now.lat;
  posted.lon = now.lon;
}
