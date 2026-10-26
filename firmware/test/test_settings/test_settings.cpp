// SPDX-License-Identifier: AGPL-3.0-only
// Writing the settings to NVS without the hardware: every key, a refused
// write reported instead of swallowed (an empty text included, whose
// putString() returns 0 even when stored), and the settings form's previous
// values written back when the flash refuses the new ones; the screen list
// and the interval read at start whatever is stored, without a write.
//
//   pio test -e native -f test_settings
#include <unity.h>

#include <Preferences.h>

#include "../../src/i18n.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/settings_logic.cpp"

void setUp() {
  fakeNvs.clear();
  nvsWriteCount = 0;
  nvsFailBegin = nvsFailWrites = false;
  nvsFailKey.clear();
}
void tearDown() {}

static Settings sample() {
  Settings s;
  s.salary = 3200.5f;
  s.workStart = 8;
  s.nightBrightness = 25;
  s.mqttPort = 8883;
  strcpy(s.zone, "UTC-1:00DST-2:00,M3.5.0/2,M10.5.0/3");
  s.utcOffset = 7200;
  s.lat = 45.75f;
  strcpy(s.message, "HELLO * ");
  strcpy(s.city, "Lyon");
  s.mqttPass[0] = '\0';
  return s;
}

static void test_every_setting_is_written() {
  const Settings s = sample();
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, s));
  TEST_ASSERT_EQUAL(36, nvsWriteCount);
  TEST_ASSERT_EQUAL_FLOAT(3200.5f, fakeNvsNumber<float>("salaire"));
  TEST_ASSERT_EQUAL(8, fakeNvsNumber<uint8_t>("hDebut"));
  TEST_ASSERT_EQUAL(25, fakeNvsNumber<uint8_t>("nightLevel"));
  TEST_ASSERT_EQUAL(8883, fakeNvsNumber<uint16_t>("mqttPort"));
  TEST_ASSERT_EQUAL_STRING("UTC-1:00DST-2:00,M3.5.0/2,M10.5.0/3", fakeNvs["zone"].c_str());
  // The forecast's old offset is read at start, never written again
  TEST_ASSERT_FALSE(fakeNvs.count("utcOffset"));
  TEST_ASSERT_EQUAL_FLOAT(45.75f, fakeNvsNumber<float>("lat"));
  TEST_ASSERT_EQUAL_STRING("HELLO * ", fakeNvs["message"].c_str());
  TEST_ASSERT_EQUAL_STRING("Lyon", fakeNvs["city"].c_str());
  TEST_ASSERT_TRUE(fakeNvs.count("mqttPass"));
  TEST_ASSERT_EQUAL_STRING("", fakeNvs["mqttPass"].c_str());
}

// A full NVS refuses a value: false. The other keys are still tried, so that
// writing the previous values back reaches every one of them.
static void test_a_refused_write_is_reported() {
  nvsFailKey = "message";
  Preferences p;
  TEST_ASSERT_FALSE(writeSettings(p, sample()));
  TEST_ASSERT_TRUE(fakeNvs.count("nightLevel"));
  TEST_ASSERT_FALSE(fakeNvs.count("message"));
  TEST_ASSERT_TRUE(fakeNvs.count("autoUpdate"));
  TEST_ASSERT_EQUAL(35, nvsWriteCount);
}

static void test_a_refused_number_is_reported() {
  nvsFailKey = "mqttPort";
  Preferences p;
  TEST_ASSERT_FALSE(writeSettings(p, sample()));
}

// putString("") returns 0 stored or not: an empty text is read back
static void test_a_refused_empty_text_is_reported() {
  fakeNvs["mqttPass"] = "secret";
  nvsFailKey = "mqttPass";
  Preferences p;
  TEST_ASSERT_FALSE(writeSettings(p, sample()));
  TEST_ASSERT_EQUAL_STRING("secret", fakeNvs["mqttPass"].c_str());
}

static void test_a_refused_first_empty_text_is_reported() {
  nvsFailKey = "mqttPass";             // the key never existed
  Preferences p;
  TEST_ASSERT_FALSE(writeSettings(p, sample()));
}

static void test_store_keeps_the_new_settings() {
  Settings before;
  Preferences p;
  TEST_ASSERT_TRUE(storeSettings(p, sample(), &before));
  TEST_ASSERT_EQUAL_STRING("HELLO * ", fakeNvs["message"].c_str());
  TEST_ASSERT_EQUAL_FLOAT(3200.5f, fakeNvsNumber<float>("salaire"));
}

// The form changed the salary, the message and the city; the flash takes
// the salary and the city but refuses the message. A restart must not load
// half of each: the previous values go back.
static void test_store_writes_the_previous_settings_back() {
  Settings before;
  strcpy(before.message, "OLD * ");
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, before));
  nvsFailKey = "message";
  TEST_ASSERT_FALSE(storeSettings(p, sample(), &before));
  TEST_ASSERT_EQUAL_FLOAT(before.salary, fakeNvsNumber<float>("salaire"));
  TEST_ASSERT_EQUAL(before.workStart, fakeNvsNumber<uint8_t>("hDebut"));
  TEST_ASSERT_EQUAL(before.nightBrightness, fakeNvsNumber<uint8_t>("nightLevel"));
  TEST_ASSERT_EQUAL_STRING("OLD * ", fakeNvs["message"].c_str());
  TEST_ASSERT_EQUAL(before.mqttPort, fakeNvsNumber<uint16_t>("mqttPort"));
  TEST_ASSERT_EQUAL_STRING("", fakeNvs["city"].c_str());
  TEST_ASSERT_EQUAL_STRING("", fakeNvs["zone"].c_str());
}

// No previous values (the API, MQTT, the weather): only the answer
static void test_store_without_previous_reports_only() {
  nvsFailKey = "message";
  Preferences p;
  TEST_ASSERT_FALSE(storeSettings(p, sample(), nullptr));
  TEST_ASSERT_EQUAL_FLOAT(3200.5f, fakeNvsNumber<float>("salaire"));
}

// --- Screens -------------------------------------------------------------------

static void storeU16(const char *key, uint16_t v) { fakeNvs[key] = std::string((const char *)&v, 2); }

// A panel updated from v0.1.0 (or a v0.2.0 development build) has none of
// the new keys: every screen on, firmware order, rotation off, and nothing
// written at start; the old keys stay as they were.
static void test_v010_store_reads_the_defaults_without_writing() {
  fakeNvs["screen"] = "CAT";
  fakeNvs["salaire"] = std::string("\0\0\x48\x45", 4);
  fakeNvs["lang"] = std::string("\x01", 1);
  const auto stored = fakeNvs;
  Settings s;
  s.rotateSec = 60;
  s.screensStored = true;
  Preferences p;
  readScreens(p, s);
  TEST_ASSERT_TRUE(screenListSameView(screenListDefault(), s.screens));
  TEST_ASSERT_EQUAL(0, s.rotateSec);
  TEST_ASSERT_FALSE(s.screensStored);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
  TEST_ASSERT_TRUE(stored == fakeNvs);
}

// Until the settings form sends them, a save (the API's message, MQTT, the
// weather) writes neither key
static void test_screen_keys_written_once_sent() {
  Settings s = sample();
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, s));
  TEST_ASSERT_FALSE(fakeNvs.count("scrList"));
  TEST_ASSERT_FALSE(fakeNvs.count("rotateSec"));
  s.screensStored = true;
  s.screens.on &= ~(1u << screenFind("SLEEP"));
  s.rotateSec = 20;
  nvsWriteCount = 0;
  TEST_ASSERT_TRUE(writeSettings(p, s));
  TEST_ASSERT_EQUAL(38, nvsWriteCount);
  TEST_ASSERT_EQUAL_STRING("NIMBUS,CODE,DANCE,-SLEEP,RUN,LIFE,-BALL,-MATRIX,PLASMA,FIRE,STARS,TEXT,"
                           "-RAYS,-BARS,BLACKHOLE,CAT,ISS DEMO,CLOCK,WEATHER,DATE,SYSTEM,MONEY,SOCIAL,"
                           "IMAGE", fakeNvs["scrList"].c_str());
  TEST_ASSERT_EQUAL(20, fakeNvsNumber<uint16_t>("rotateSec"));
}

static void test_stored_list_and_interval_read_back() {
  fakeNvs["scrList"] = "CAT,-CLOCK,NIMBUS";
  storeU16("rotateSec", 300);
  Settings s;
  Preferences p;
  readScreens(p, s);
  TEST_ASSERT_TRUE(s.screensStored);
  TEST_ASSERT_EQUAL(screenFind("CAT"), s.screens.order[0]);
  TEST_ASSERT_FALSE(screenOn(s.screens, screenFind("CLOCK")));
  TEST_ASSERT_TRUE(screenOn(s.screens, screenFind("IMAGE")));    // not named: on, at the end
  TEST_ASSERT_EQUAL(screenFind("IMAGE"), s.screens.order[SCREEN_COUNT - 1]);
  TEST_ASSERT_EQUAL(300, s.rotateSec);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// Only a development build or corruption can store these
static void test_unknown_interval_reads_off() {
  for (uint16_t bad : {(uint16_t)45, (uint16_t)1, (uint16_t)65535}) {
    storeU16("rotateSec", bad);
    Settings s;
    s.rotateSec = 10;
    Preferences p;
    readScreens(p, s);
    TEST_ASSERT_EQUAL(0, s.rotateSec);
  }
  fakeNvs["rotateSec"] = "20";                                    // another type
  Settings s;
  Preferences p;
  readScreens(p, s);
  TEST_ASSERT_EQUAL(0, s.rotateSec);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// Every landscape screen off: corruption, a development build, or an update
// that turned a screen to the other orientation
static const char *const NO_LANDSCAPE =
    "CODE,SLEEP,RUN,TEXT,-NIMBUS,-DANCE,-LIFE,-BALL,-MATRIX,-PLASMA,-FIRE,-STARS,-RAYS,-BARS,"
    "-BLACKHOLE,-CAT,-ISS DEMO,-CLOCK,-WEATHER,-DATE,-SYSTEM,-MONEY,-SOCIAL,-IMAGE";

static void test_empty_orientation_is_filled_without_a_write() {
  fakeNvs["scrList"] = NO_LANDSCAPE;
  Settings s;
  Preferences p;
  readScreens(p, s);
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) TEST_ASSERT_TRUE(screenOn(s.screens, i));
  TEST_ASSERT_EQUAL(screenFind("CODE"), s.screens.order[0]);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// Longer than any list the firmware writes: read as absent
static void test_overlong_list_reads_as_the_default() {
  fakeNvs["scrList"] = std::string(SCREEN_LIST_TEXT_MAX, 'A');
  Settings s;
  Preferences p;
  readScreens(p, s);
  TEST_ASSERT_TRUE(s.screensStored);
  TEST_ASSERT_TRUE(screenListSameView(screenListDefault(), s.screens));
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// The flash takes the list but refuses the interval: the previous list goes
// back, so order and interval stay a pair after a restart
static void test_refused_interval_puts_the_list_back() {
  Settings before;
  before.screensStored = true;
  before.rotateSec = 60;
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, before));
  const std::string oldList = fakeNvs["scrList"];
  Settings next = before;
  next.screens.on &= ~(1u << screenFind("CAT"));
  next.rotateSec = 10;
  nvsFailKey = "rotateSec";
  TEST_ASSERT_FALSE(storeSettings(p, next, &before));
  TEST_ASSERT_EQUAL_STRING(oldList.c_str(), fakeNvs["scrList"].c_str());
  TEST_ASSERT_EQUAL(60, fakeNvsNumber<uint16_t>("rotateSec"));
}

// The first list the form sends fails half-way: the keys it wrote go, so a
// restart still follows the firmware's list
static void test_refused_first_list_leaves_no_key() {
  Settings before;
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, before));
  Settings next = before;
  next.screensStored = true;
  next.rotateSec = 10;
  nvsFailKey = "rotateSec";
  TEST_ASSERT_FALSE(storeSettings(p, next, &before));
  TEST_ASSERT_FALSE(fakeNvs.count("scrList"));
  TEST_ASSERT_FALSE(fakeNvs.count("rotateSec"));
}

// The weather, MQTT or the API's message save after a fill at start: the
// owner's list stays as stored, its off screens included
static void test_filled_list_kept_by_other_saves() {
  fakeNvs["scrList"] = NO_LANDSCAPE;
  storeU16("rotateSec", 60);
  Settings s;
  Preferences p;
  readScreens(p, s);
  const auto stored = fakeNvs;
  strcpy(s.zone, "UTC-1:00");
  TEST_ASSERT_TRUE(storeSettings(p, s, nullptr));
  TEST_ASSERT_EQUAL_STRING(NO_LANDSCAPE, fakeNvs["scrList"].c_str());
  TEST_ASSERT_TRUE(stored.at("rotateSec") == fakeNvs["rotateSec"]);
}

// The form's save after a fill at start is refused half-way: the list goes
// back as it was stored, not filled, and neither key is removed
static void test_refused_save_puts_back_the_list_filled_at_start() {
  fakeNvs["scrList"] = NO_LANDSCAPE;
  storeU16("rotateSec", 60);
  Settings before;
  Preferences p;
  readScreens(p, before);
  Settings next = before;
  next.screensStored = true;
  next.screens.on &= ~(1u << screenFind("CAT"));
  next.rotateSec = 10;
  nvsFailKey = "rotateSec";
  TEST_ASSERT_FALSE(storeSettings(p, next, &before));
  TEST_ASSERT_EQUAL_STRING(NO_LANDSCAPE, fakeNvs["scrList"].c_str());
  TEST_ASSERT_EQUAL(60, fakeNvsNumber<uint16_t>("rotateSec"));
}

// The weather, MQTT, the API's message and the Wi-Fi page save without the
// form: the screen keys stay as stored, whatever the RAM holds, and the
// list's text is never built on their tasks' smaller stacks
static void test_background_save_leaves_the_screen_keys() {
  Settings s = sample();
  s.screensStored = true;
  s.rotateSec = 60;
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, s));
  const std::string list = fakeNvs["scrList"];
  s.screens.on &= ~(1u << screenFind("CAT"));
  s.rotateSec = 10;
  strcpy(s.zone, "UTC-1:00");
  nvsWriteCount = 0;
  TEST_ASSERT_TRUE(storeSettings(p, s, nullptr));
  TEST_ASSERT_EQUAL(36, nvsWriteCount);
  TEST_ASSERT_EQUAL_STRING(list.c_str(), fakeNvs["scrList"].c_str());
  TEST_ASSERT_EQUAL(60, fakeNvsNumber<uint16_t>("rotateSec"));
  TEST_ASSERT_EQUAL_STRING("UTC-1:00", fakeNvs["zone"].c_str());
}

// A background save lands while a refused form save still holds the posted
// list in RAM (the put-back of RAM comes after the flash's): the refused
// list is not written again, and the removed keys stay removed
static void test_background_save_after_a_refused_form_save() {
  Settings before;
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, before));
  Settings next = before;
  next.screensStored = true;
  next.screens.on &= ~(1u << screenFind("CAT"));
  next.rotateSec = 10;
  nvsFailKey = "rotateSec";
  TEST_ASSERT_FALSE(storeSettings(p, next, &before));
  nvsFailKey.clear();
  TEST_ASSERT_TRUE(storeSettings(p, next, nullptr));
  TEST_ASSERT_FALSE(fakeNvs.count("scrList"));
  TEST_ASSERT_FALSE(fakeNvs.count("rotateSec"));
}

// --- Styles --------------------------------------------------------------------

static void storeU8(const char *key, uint8_t v) { fakeNvs[key] = std::string((const char *)&v, 1); }

static Settings styled() {
  Settings s = sample();
  s.codeStyle = CODE_LIST;
  s.clockStyle = CLOCK_CYCLE;
  s.clockShow = CLOCK_SHOW_DATE | CLOCK_SHOW_WEEK | CLOCK_SHOW_OUTSIDE;
  s.weatherStyle = WEATHER_DAY;
  return s;
}

// Each style under its own key, read back as written, without a write
static void test_styles_written_and_read_back() {
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, styled()));
  TEST_ASSERT_EQUAL(CODE_LIST, fakeNvsNumber<uint8_t>("codeStyle"));
  TEST_ASSERT_EQUAL(CLOCK_CYCLE, fakeNvsNumber<uint8_t>("clkStyle"));
  TEST_ASSERT_EQUAL(CLOCK_SHOW_DATE | CLOCK_SHOW_WEEK | CLOCK_SHOW_OUTSIDE,
                    fakeNvsNumber<uint8_t>("clkShow"));
  TEST_ASSERT_EQUAL(WEATHER_DAY, fakeNvsNumber<uint8_t>("wxStyle"));
  Settings s;
  nvsWriteCount = 0;
  readStyles(p, s);
  TEST_ASSERT_EQUAL(CODE_LIST, s.codeStyle);
  TEST_ASSERT_EQUAL(CLOCK_CYCLE, s.clockStyle);
  TEST_ASSERT_EQUAL(CLOCK_SHOW_DATE | CLOCK_SHOW_WEEK | CLOCK_SHOW_OUTSIDE, s.clockShow);
  TEST_ASSERT_EQUAL(WEATHER_DAY, s.weatherStyle);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// A panel updated from a firmware without styles has none of the keys: the
// owner's defaults, and nothing written at start
static void test_absent_styles_read_as_the_defaults() {
  Settings s = styled();
  Preferences p;
  readStyles(p, s);
  TEST_ASSERT_EQUAL(CODE_AUTO, s.codeStyle);
  TEST_ASSERT_EQUAL(CLOCK_STONE, s.clockStyle);
  TEST_ASSERT_EQUAL(CLOCK_SHOW_DEFAULT, s.clockShow);
  TEST_ASSERT_EQUAL(WEATHER_GLANCE, s.weatherStyle);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// Only a later firmware, a development build or corruption stores these: a
// style this firmware does not know reads as the default, unknown ClockShow
// bits are dropped, a value of another type reads as absent
static void test_out_of_range_styles_read_as_the_defaults() {
  storeU8("codeStyle", CODE_STYLE_COUNT);
  storeU8("clkStyle", 200);
  storeU8("clkShow", 0xF0 | CLOCK_SHOW_WEEK);
  storeU8("wxStyle", 255);
  Settings s = styled();
  Preferences p;
  readStyles(p, s);
  TEST_ASSERT_EQUAL(CODE_AUTO, s.codeStyle);
  TEST_ASSERT_EQUAL(CLOCK_STONE, s.clockStyle);
  TEST_ASSERT_EQUAL(CLOCK_SHOW_WEEK, s.clockShow);
  TEST_ASSERT_EQUAL(WEATHER_GLANCE, s.weatherStyle);
  storeU16("codeStyle", CODE_LIST);
  storeU16("clkShow", CLOCK_SHOW_WEEK);
  storeU8("wxStyle", WEATHER_SCENE);
  s = styled();
  readStyles(p, s);
  TEST_ASSERT_EQUAL(CODE_AUTO, s.codeStyle);
  TEST_ASSERT_EQUAL(CLOCK_SHOW_DEFAULT, s.clockShow);
  TEST_ASSERT_EQUAL(WEATHER_SCENE, s.weatherStyle);
  // Nothing besides the time is a choice, kept
  storeU8("clkShow", 0);
  readStyles(p, s);
  TEST_ASSERT_EQUAL(0, s.clockShow);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// The flash refuses one style: the previous styles go back with the rest of
// the form, so a restart does not load half of it
static void test_refused_style_puts_the_previous_styles_back() {
  Settings before = sample();
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, before));
  nvsFailKey = "wxStyle";
  TEST_ASSERT_FALSE(storeSettings(p, styled(), &before));
  TEST_ASSERT_EQUAL(CODE_AUTO, fakeNvsNumber<uint8_t>("codeStyle"));
  TEST_ASSERT_EQUAL(CLOCK_STONE, fakeNvsNumber<uint8_t>("clkStyle"));
  TEST_ASSERT_EQUAL(CLOCK_SHOW_DEFAULT, fakeNvsNumber<uint8_t>("clkShow"));
  TEST_ASSERT_EQUAL(WEATHER_GLANCE, fakeNvsNumber<uint8_t>("wxStyle"));
}

// --- Feedback choices ------------------------------------------------------------

static Settings chosen() {
  Settings s = sample();
  s.transition = TRANSITION_WIPE;
  s.brightnessFeedback = BRIGHTNESS_GAUGE;
  s.knobFeedback = KNOB_DOTS;
  s.bootDetail = BOOT_DETAILED;
  s.requestShow[REQUEST_MESSAGE] = REQUEST_OFF;
  s.requestShow[REQUEST_IMAGE] = REQUEST_DISCREET;
  s.requestShow[REQUEST_COMMAND] = REQUEST_SHOW;
  s.requestShow[REQUEST_AGENT] = REQUEST_OFF;
  return s;
}

static void test_feedback_written_and_read_back() {
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, chosen()));
  TEST_ASSERT_EQUAL(TRANSITION_WIPE, fakeNvsNumber<uint8_t>("transition"));
  TEST_ASSERT_EQUAL(BRIGHTNESS_GAUGE, fakeNvsNumber<uint8_t>("briFb"));
  TEST_ASSERT_EQUAL(KNOB_DOTS, fakeNvsNumber<uint8_t>("knobFb"));
  TEST_ASSERT_EQUAL(BOOT_DETAILED, fakeNvsNumber<uint8_t>("bootDetail"));
  TEST_ASSERT_EQUAL(REQUEST_KIND_COUNT, fakeNvs["reqShow"].size());
  Settings s;
  nvsWriteCount = 0;
  readStyles(p, s);
  TEST_ASSERT_EQUAL(TRANSITION_WIPE, s.transition);
  TEST_ASSERT_EQUAL(BRIGHTNESS_GAUGE, s.brightnessFeedback);
  TEST_ASSERT_EQUAL(KNOB_DOTS, s.knobFeedback);
  TEST_ASSERT_EQUAL(BOOT_DETAILED, s.bootDetail);
  TEST_ASSERT_EQUAL(REQUEST_OFF, s.requestShow[REQUEST_MESSAGE]);
  TEST_ASSERT_EQUAL(REQUEST_DISCREET, s.requestShow[REQUEST_IMAGE]);
  TEST_ASSERT_EQUAL(REQUEST_SHOW, s.requestShow[REQUEST_COMMAND]);
  TEST_ASSERT_EQUAL(REQUEST_OFF, s.requestShow[REQUEST_AGENT]);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

static void test_absent_feedback_reads_as_the_defaults() {
  Settings s = chosen();
  Preferences p;
  readStyles(p, s);
  TEST_ASSERT_EQUAL(TRANSITION_PUSH, s.transition);
  TEST_ASSERT_EQUAL(BRIGHTNESS_DOTS, s.brightnessFeedback);
  TEST_ASSERT_EQUAL(KNOB_BAND, s.knobFeedback);
  TEST_ASSERT_EQUAL(BOOT_AUTO, s.bootDetail);
  for (int k = 0; k < REQUEST_KIND_COUNT; k++) TEST_ASSERT_EQUAL(REQUEST_SHOW, s.requestShow[k]);
}

static void test_out_of_range_feedback_reads_as_the_defaults() {
  storeU8("transition", TRANSITION_COUNT);
  storeU8("briFb", 200);
  storeU8("knobFb", KNOB_FEEDBACK_COUNT);
  storeU8("bootDetail", BOOT_DETAIL_COUNT);
  const uint8_t req[REQUEST_KIND_COUNT] = {REQUEST_OFF, REQUEST_SHOW_COUNT, REQUEST_DISCREET, 255};
  fakeNvs["reqShow"] = std::string((const char *)req, sizeof(req));
  Settings s = chosen();
  Preferences p;
  readStyles(p, s);
  TEST_ASSERT_EQUAL(TRANSITION_PUSH, s.transition);
  TEST_ASSERT_EQUAL(BRIGHTNESS_DOTS, s.brightnessFeedback);
  TEST_ASSERT_EQUAL(KNOB_BAND, s.knobFeedback);
  TEST_ASSERT_EQUAL(BOOT_AUTO, s.bootDetail);
  // Each kind on its own: the good bytes stay
  TEST_ASSERT_EQUAL(REQUEST_OFF, s.requestShow[REQUEST_MESSAGE]);
  TEST_ASSERT_EQUAL(REQUEST_SHOW, s.requestShow[REQUEST_IMAGE]);
  TEST_ASSERT_EQUAL(REQUEST_DISCREET, s.requestShow[REQUEST_COMMAND]);
  TEST_ASSERT_EQUAL(REQUEST_SHOW, s.requestShow[REQUEST_AGENT]);
}

// A blob of another length (an older or later firmware) reads as the defaults
static void test_short_or_long_request_blob_reads_as_the_defaults() {
  Preferences p;
  for (size_t len : {(size_t)1, (size_t)REQUEST_KIND_COUNT - 1, (size_t)REQUEST_KIND_COUNT + 1}) {
    fakeNvs["reqShow"] = std::string(len, (char)REQUEST_OFF);
    Settings s = chosen();
    readStyles(p, s);
    for (int k = 0; k < REQUEST_KIND_COUNT; k++) TEST_ASSERT_EQUAL(REQUEST_SHOW, s.requestShow[k]);
  }
}

static void test_refused_feedback_puts_the_previous_choices_back() {
  Settings before = sample();
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, before));
  nvsFailKey = "reqShow";
  TEST_ASSERT_FALSE(storeSettings(p, chosen(), &before));
  TEST_ASSERT_EQUAL(TRANSITION_PUSH, fakeNvsNumber<uint8_t>("transition"));
  TEST_ASSERT_EQUAL(BRIGHTNESS_DOTS, fakeNvsNumber<uint8_t>("briFb"));
  TEST_ASSERT_EQUAL(KNOB_BAND, fakeNvsNumber<uint8_t>("knobFb"));
  TEST_ASSERT_EQUAL(REQUEST_SHOW, (uint8_t)fakeNvs["reqShow"][0]);
}

// --- Region ------------------------------------------------------------------------

static Settings abroad() {
  Settings s = sample();
  s.tempUnit = TEMP_F;
  s.currency = CURRENCY_GBP;
  s.clock12 = true;
  return s;
}

// Each choice under its own key (new, 15 characters at most), read back as
// written, without a write
static void test_region_written_and_read_back() {
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, abroad()));
  TEST_ASSERT_EQUAL(TEMP_F, fakeNvsNumber<uint8_t>("tempUnit"));
  TEST_ASSERT_EQUAL(CURRENCY_GBP, fakeNvsNumber<uint8_t>("currency"));
  TEST_ASSERT_EQUAL(1, fakeNvsNumber<uint8_t>("clock12"));
  Settings s;
  nvsWriteCount = 0;
  readRegion(p, s);
  TEST_ASSERT_EQUAL(TEMP_F, s.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_GBP, s.currency);
  TEST_ASSERT_TRUE(s.clock12);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// A v0.1.0 panel has none of the keys: degrees Celsius, euros, 24 h
static void test_absent_region_reads_as_the_defaults() {
  Settings s = abroad();
  Preferences p;
  readRegion(p, s);
  TEST_ASSERT_EQUAL(TEMP_C, s.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_EUR, s.currency);
  TEST_ASSERT_FALSE(s.clock12);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// A later firmware may offer more units or currencies: this one reads its
// defaults for those, and for a value of another type
static void test_out_of_range_region_reads_as_the_defaults() {
  storeU8("tempUnit", TEMP_UNIT_COUNT);
  storeU8("currency", CURRENCY_COUNT);
  storeU8("clock12", 2);
  Settings s = abroad();
  Preferences p;
  readRegion(p, s);
  TEST_ASSERT_EQUAL(TEMP_C, s.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_EUR, s.currency);
  TEST_ASSERT_FALSE(s.clock12);
  storeU16("tempUnit", TEMP_F);
  storeU8("currency", 255);
  storeU8("clock12", 1);
  s = abroad();
  readRegion(p, s);
  TEST_ASSERT_EQUAL(TEMP_C, s.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_EUR, s.currency);
  TEST_ASSERT_TRUE(s.clock12);
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

// The flash refuses one choice: the previous region goes back with the rest
static void test_refused_region_puts_the_previous_choices_back() {
  Settings before = sample();
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, before));
  nvsFailKey = "clock12";
  TEST_ASSERT_FALSE(storeSettings(p, abroad(), &before));
  TEST_ASSERT_EQUAL(TEMP_C, fakeNvsNumber<uint8_t>("tempUnit"));
  TEST_ASSERT_EQUAL(CURRENCY_EUR, fakeNvsNumber<uint8_t>("currency"));
  TEST_ASSERT_EQUAL(0, fakeNvsNumber<uint8_t>("clock12"));
}

// At Wi-Fi setup the phone's units are stored where no key is: a first
// setup, a reset panel or one updated from v0.1.0 takes them
static void test_phone_region_fills_absent_keys() {
  Settings s;
  Preferences p;
  TEST_ASSERT_TRUE(presetRegion(p, s, RegionChoice{TEMP_F, CURRENCY_USD, 1}));
  TEST_ASSERT_EQUAL(TEMP_F, fakeNvsNumber<uint8_t>("tempUnit"));
  TEST_ASSERT_EQUAL(CURRENCY_USD, fakeNvsNumber<uint8_t>("currency"));
  TEST_ASSERT_EQUAL(1, fakeNvsNumber<uint8_t>("clock12"));
  TEST_ASSERT_EQUAL(TEMP_F, s.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_USD, s.currency);
  TEST_ASSERT_TRUE(s.clock12);
  TEST_ASSERT_EQUAL(3, nvsWriteCount);
}

// A stored choice is the owner's, even one equal to the default: the phone
// never replaces it, in NVS or in RAM
static void test_phone_region_never_replaces_a_choice() {
  Preferences p;
  TEST_ASSERT_TRUE(writeSettings(p, sample()));
  Settings s = sample();
  nvsWriteCount = 0;
  TEST_ASSERT_TRUE(presetRegion(p, s, RegionChoice{TEMP_F, CURRENCY_GBP, 1}));
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
  TEST_ASSERT_EQUAL(TEMP_C, fakeNvsNumber<uint8_t>("tempUnit"));
  TEST_ASSERT_EQUAL(CURRENCY_EUR, fakeNvsNumber<uint8_t>("currency"));
  TEST_ASSERT_EQUAL(0, fakeNvsNumber<uint8_t>("clock12"));
  TEST_ASSERT_EQUAL(TEMP_C, s.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_EUR, s.currency);
  TEST_ASSERT_FALSE(s.clock12);
  // Key by key: only the absent one is filled
  fakeNvs.erase("currency");
  TEST_ASSERT_TRUE(presetRegion(p, s, RegionChoice{TEMP_F, CURRENCY_GBP, 1}));
  TEST_ASSERT_EQUAL(1, nvsWriteCount);
  TEST_ASSERT_EQUAL(CURRENCY_GBP, fakeNvsNumber<uint8_t>("currency"));
  TEST_ASSERT_EQUAL(TEMP_C, s.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_GBP, s.currency);
  TEST_ASSERT_FALSE(s.clock12);
}

// Nothing posted (no script) or a value the form refused: the key stays
// absent and the default stays, so a later setup can still fill it
static void test_phone_region_not_sent_stores_nothing() {
  Settings s;
  Preferences p;
  TEST_ASSERT_TRUE(presetRegion(p, s, RegionChoice{}));
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
  TEST_ASSERT_FALSE(p.isKey("tempUnit") || p.isKey("currency") || p.isKey("clock12"));
  TEST_ASSERT_TRUE(presetRegion(p, s, RegionChoice{-1, CURRENCY_CHF, -1}));
  TEST_ASSERT_FALSE(p.isKey("tempUnit") || p.isKey("clock12"));
  TEST_ASSERT_EQUAL(CURRENCY_CHF, fakeNvsNumber<uint8_t>("currency"));
  TEST_ASSERT_EQUAL(TEMP_C, s.tempUnit);
  TEST_ASSERT_FALSE(s.clock12);
}

// A refused write is reported, and RAM keeps what NVS keeps
static void test_phone_region_refused_write_is_reported() {
  Settings s;
  Preferences p;
  nvsFailKey = "currency";
  TEST_ASSERT_FALSE(presetRegion(p, s, RegionChoice{TEMP_F, CURRENCY_USD, 1}));
  TEST_ASSERT_EQUAL(CURRENCY_EUR, s.currency);
  TEST_ASSERT_EQUAL(TEMP_F, s.tempUnit);
  TEST_ASSERT_TRUE(s.clock12);
}

// The settings form saves a copy taken when its request came in; the
// HTTPS task may have found the zone (or the place) meanwhile
static void test_form_keeps_the_location_found_meanwhile() {
  Settings posted;
  strcpy(posted.city, "Paris");
  strcpy(posted.place, "Paris, France");
  posted.lat = 48.85f;
  posted.lon = 2.35f;
  Settings now = posted;
  strcpy(now.zone, "CET-1CEST,M3.5.0,M10.5.0/3");
  posted.nightStart = 22;                    // what the form changed
  keepLocation(posted, now, false);
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", posted.zone);
  TEST_ASSERT_EQUAL(22, posted.nightStart);
  // A place found meanwhile, with its coordinates
  strcpy(posted.place, "");
  strcpy(now.place, "Lyon, France");
  now.lat = 45.75f;
  now.lon = 4.85f;
  keepLocation(posted, now, false);
  TEST_ASSERT_EQUAL_STRING("Lyon, France", posted.place);
  TEST_ASSERT_EQUAL_FLOAT(45.75f, posted.lat);
  TEST_ASSERT_EQUAL_FLOAT(4.85f, posted.lon);
  // The form changed the city: its own, to be looked up
  Settings moved = posted;
  strcpy(moved.city, "Nice");
  moved.place[0] = '\0';
  keepLocation(moved, now, true);
  TEST_ASSERT_EQUAL_STRING("Nice", moved.city);
  TEST_ASSERT_EQUAL_STRING("", moved.place);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_phone_region_fills_absent_keys);
  RUN_TEST(test_phone_region_never_replaces_a_choice);
  RUN_TEST(test_phone_region_not_sent_stores_nothing);
  RUN_TEST(test_phone_region_refused_write_is_reported);
  RUN_TEST(test_region_written_and_read_back);
  RUN_TEST(test_absent_region_reads_as_the_defaults);
  RUN_TEST(test_out_of_range_region_reads_as_the_defaults);
  RUN_TEST(test_refused_region_puts_the_previous_choices_back);
  RUN_TEST(test_feedback_written_and_read_back);
  RUN_TEST(test_absent_feedback_reads_as_the_defaults);
  RUN_TEST(test_out_of_range_feedback_reads_as_the_defaults);
  RUN_TEST(test_short_or_long_request_blob_reads_as_the_defaults);
  RUN_TEST(test_refused_feedback_puts_the_previous_choices_back);
  RUN_TEST(test_every_setting_is_written);
  RUN_TEST(test_a_refused_write_is_reported);
  RUN_TEST(test_a_refused_number_is_reported);
  RUN_TEST(test_a_refused_empty_text_is_reported);
  RUN_TEST(test_a_refused_first_empty_text_is_reported);
  RUN_TEST(test_store_keeps_the_new_settings);
  RUN_TEST(test_store_writes_the_previous_settings_back);
  RUN_TEST(test_store_without_previous_reports_only);
  RUN_TEST(test_v010_store_reads_the_defaults_without_writing);
  RUN_TEST(test_screen_keys_written_once_sent);
  RUN_TEST(test_stored_list_and_interval_read_back);
  RUN_TEST(test_unknown_interval_reads_off);
  RUN_TEST(test_empty_orientation_is_filled_without_a_write);
  RUN_TEST(test_overlong_list_reads_as_the_default);
  RUN_TEST(test_refused_interval_puts_the_list_back);
  RUN_TEST(test_refused_first_list_leaves_no_key);
  RUN_TEST(test_filled_list_kept_by_other_saves);
  RUN_TEST(test_refused_save_puts_back_the_list_filled_at_start);
  RUN_TEST(test_background_save_leaves_the_screen_keys);
  RUN_TEST(test_background_save_after_a_refused_form_save);
  RUN_TEST(test_styles_written_and_read_back);
  RUN_TEST(test_absent_styles_read_as_the_defaults);
  RUN_TEST(test_out_of_range_styles_read_as_the_defaults);
  RUN_TEST(test_refused_style_puts_the_previous_styles_back);
  RUN_TEST(test_form_keeps_the_location_found_meanwhile);
  return UNITY_END();
}
