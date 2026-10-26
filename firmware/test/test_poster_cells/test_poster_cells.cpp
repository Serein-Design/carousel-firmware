// SPDX-License-Identifier: AGPL-3.0-only
// The posters of every screen, of each style of CODE, CLOCK and WEATHER, and
// of the sign-in, locked and no image frames, drawn by the firmware's own
// screens: their draw functions run here on a 64x32 frame (CAT on a 32x64
// one) with fixed inputs, a seeded random and a virtual clock at 60 frames a
// second, and each cell is written to firmware/.pio/poster-cells/ as raw
// RGB565 (little-endian, row after row), "CLOCK-1.565" for CLOCK in its
// style 1. tools/simulator/posters.mjs reads them into the sheet.
// The public mascot is drawn: the private one is never compiled in here.
// IMAGE plays a sunset GIF made below, through the real decoder.
//
//   pio test -e native -f test_poster_cells
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <esp_timer.h>
#include <WiFi.h>

#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"
#include "../../src/i18n.cpp"
#include "../../src/json_lite.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/palette.cpp"
#include "../../src/clock_logic.cpp"
#include "../../src/weather_logic.cpp"
#include "../../src/code_view_logic.cpp"
#include "../../src/agent_bridge.cpp"
#include "../../src/ambiance_logic.cpp"
#include "../../src/boot_logic.cpp"
#include "../../src/info_logic.cpp"
#include "../../src/content_logic.cpp"
#include "../../src/gif_decoder.cpp"
#include "../../src/screens/mascot.cpp"
#include "../../src/screens/ambiances.cpp"
#include "../../src/screens/effects.cpp"
#include "../../src/screens/black_hole.cpp"
#include "../../src/screens/cat.cpp"
#include "../../src/screens/iss.cpp"
#include "../../src/screens/text.cpp"
#include "../../src/screens/clock.cpp"
#include "../../src/screens/weather_screen.cpp"
#include "../../src/screens/agent_monitor.cpp"
#include "../../src/panel_name.cpp"
#include "../../src/states_logic.cpp"
#include "../../src/screens/states.cpp"
#include "../../src/screens/info.cpp"
#include "../../src/screens/image.cpp"

// --- What display.cpp, gallery.cpp, network.cpp and the rest give the screens
bool Frame::allocate() {
  buffer = (uint16_t *)calloc(PIXEL_COUNT, sizeof(uint16_t));
  buffer_owned = true;
  return buffer != nullptr;
}
void Frame::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (buffer) GFXcanvas16::drawFastVLine(x, y, h, color);
}
void Frame::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (buffer) GFXcanvas16::drawFastHLine(x, y, w, color);
}
static Frame frame;
Frame *display = &frame;
uint16_t BLACK = 0, WHITE = 0xFFFF;
bool portrait = false;
NetworkState networkState = NET_SYNCED;
bool timeValid = true;
Settings settings;
Weather weather;
SocialStats socialStats;
UpdateStatus updateStatus;
const Palette &palette() { return paletteFor(false); }   // by day, a bright panel
// The bridge's way to the panel; no session is fed here
void requestShown(RequestKind, RequestSource, const char *, uint16_t, uint32_t) {}
// display.cpp's, which drives the panel and cannot be built here: the wheel
// of colours and the sine table are the same as there
uint8_t sinTab[256];
uint16_t colorWheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) return display->color565(255 - pos * 3, 0, pos * 3);
  if (pos < 170) { pos -= 85; return display->color565(0, pos * 3, 255 - pos * 3); }
  pos -= 170;
  return display->color565(pos * 3, 255 - pos * 3, 0);
}
bool panelLocked() { return false; }   // settings.cpp's, read by the image choice
// The sign-in code, the reach of auth.cpp: none asked here
uint32_t pairingCodeShown() { return 0; }
float pairingTimeLeft() { return 0; }

// --- The sunset IMAGE plays ---------------------------------------------------------
// A 64x32 GIF, one frame, made here with a plain LZW writer (a clear code every
// 250 pixels keeps the codes at 9 bits and the table small), served to the
// screen as the content module serves a stored file.
static std::vector<uint8_t> playFile;
static size_t playPos = 0;
static bool playOpen = false;

bool contentPlayLock() { return true; }
void contentPlayUnlock() {}
bool contentPlayOpen(const char *, int32_t *size) {
  playOpen = true;
  playPos = 0;
  if (size) *size = (int32_t)playFile.size();
  return true;
}
int32_t contentPlayRead(uint8_t *buf, int32_t len) {
  if (!playOpen) return 0;
  const size_t n = std::min<size_t>((size_t)len, playFile.size() - playPos);
  memcpy(buf, playFile.data() + playPos, n);
  playPos += n;
  return (int32_t)n;
}
int32_t contentPlaySeek(int32_t pos) {
  if (pos < 0 || (size_t)pos > playFile.size()) return -1;
  playPos = (size_t)pos;
  return pos;
}
bool contentPlayValid() { return playOpen; }
void contentPlayClose() { playOpen = false; }
ImagePick contentPick(bool, const char *, const char *, char *name, size_t size) {
  snprintf(name, size, "sunset");
  return {IMG_PLAY, 0};
}

// The sky warming towards the horizon, a sun, a ridge on the left and the glint on the water
static std::vector<uint8_t> sunsetGif() {
  std::vector<uint8_t> palette;                 // r, g, b, ...
  std::vector<uint8_t> index;
  auto colour = [&](int r, int g, int b) {
    for (size_t i = 0; i < palette.size(); i += 3)
      if (palette[i] == r && palette[i + 1] == g && palette[i + 2] == b) return (uint8_t)(i / 3);
    palette.insert(palette.end(), {(uint8_t)r, (uint8_t)g, (uint8_t)b});
    return (uint8_t)(palette.size() / 3 - 1);
  };
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 64; x++) {
      int r, g, b;
      if (y < 18) {
        const float t = y / 18.0f;
        r = (int)(40 + 215 * t); g = (int)(20 + 90 * t); b = (int)(90 - 40 * t);
      } else {
        const float t = (y - 18) / 14.0f;
        r = (int)(20 + 30 * (1 - t)); g = (int)(30 + 20 * (1 - t)); b = (int)(80 - 20 * t);
      }
      if (y < 18 && (x - 40) * (x - 40) + (y - 15) * (y - 15) < 36) { r = 255; g = 210; b = 90; }
      const int ridge = 12 + (int)lroundf(4 * sinf(x / 7.0f) + 2 * sinf(x / 3.0f));
      if (x < 30 && y >= ridge && y < 18) { r = 30; g = 20; b = 45; }
      if (y > 18 && y % 2 && x > 33 && x < 47 && (x + y + 10) % 5) { r = 255; g = 170; b = 70; }
      index.push_back(colour(r, g, b));
    }
  TEST_ASSERT_LESS_OR_EQUAL(256, (int)(palette.size() / 3));
  palette.resize(768);

  std::vector<uint8_t> bits;                    // the codes, 9 bits each, least significant first
  uint32_t acc = 0;
  int have = 0;
  auto put = [&](uint16_t code) {
    acc |= (uint32_t)code << have;
    for (have += 9; have >= 8; have -= 8, acc >>= 8) bits.push_back((uint8_t)acc);
  };
  for (size_t i = 0; i < index.size(); i++) {
    if (i % 250 == 0) put(256);                 // clear
    put(index[i]);
  }
  put(257);                                     // end
  if (have) bits.push_back((uint8_t)acc);

  std::vector<uint8_t> g = {'G', 'I', 'F', '8', '9', 'a', 64, 0, 32, 0, 0xF7, 0, 0};
  g.insert(g.end(), palette.begin(), palette.end());
  g.insert(g.end(), {0x2C, 0, 0, 0, 0, 64, 0, 32, 0, 0, 8});
  for (size_t at = 0; at < bits.size(); at += 255) {
    const size_t n = std::min<size_t>(255, bits.size() - at);
    g.push_back((uint8_t)n);
    g.insert(g.end(), bits.begin() + at, bits.begin() + at + n);
  }
  g.insert(g.end(), {0, 0x3B});
  return g;
}

// --- The fixed inputs -----------------------------------------------------------
static const uint32_t NOW_MS = 3600000;   // an hour after the start
static const uint32_t SEED = 0x5EED;

// Monday 19 October 2026, 14:32:20
static void setTime() {
  struct tm t = {};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 9;
  t.tm_mday = 19;
  t.tm_hour = 14;
  t.tm_min = 32;
  t.tm_sec = 20;
  const time_t s = timegm(&t);
  gmtime_r(&s, &fakeLocalTime);   // fills in the weekday and the day of the year
  fakeTimeSet = true;
  timeValid = true;
  fakeClock = NOW_MS;
}

// Paris, 14° and partly cloudy, the afternoon clearing then rain at night
static void setWeather() {
  strcpy(settings.city, "Paris");
  strcpy(settings.place, "Paris");
  settings.lat = 48.85f;
  settings.lon = 2.35f;
  weather.valid = true;
  weather.updatedAt = NOW_MS - 60000;   // a minute ago: fresh, no foot band
  weather.temp = 14.2f;
  weather.tMin = 9;
  weather.tMax = 16;
  weather.cond = WX_PARTLY;
  weather.day = true;
  static const WxCond CONDS[WEATHER_HOURS] = {
      WX_PARTLY, WX_PARTLY, WX_MOSTLY_CLEAR, WX_MOSTLY_CLEAR, WX_CLEAR, WX_OVERCAST,
      WX_OVERCAST, WX_RAIN, WX_RAIN, WX_RAIN, WX_RAIN, WX_OVERCAST};
  HourlyForecast &h = weather.hourly;
  h.count = WEATHER_HOURS;
  for (uint8_t i = 0; i < WEATHER_HOURS; i++) {
    h.hour[i] = (uint8_t)((14 + i) % 24);
    h.temp[i] = 14.2f + (i < 3 ? i * 0.6f : 1.8f - (i - 3) * 0.9f);
    h.cond[i] = CONDS[i];
    h.day[i] = 14 + i < 19;
  }
}

// A panel on the Wi-Fi with a good signal, 142 KB free of 320, up for 3 h 12
static void setSystem() {
  WiFi.link = WL_CONNECTED;
  WiFi.rssi = -58;
  ESP.freeHeap = 142 * 1024;
  ESP.heapSize = 320 * 1024;
  fakeUptimeUs = (3 * 3600 + 12 * 60) * 1000000LL;
}

// The four counters, two from their APIs and two typed in on the settings page
static void setSocial() {
  strcpy(settings.ytKey, "AIzaFixtureKeyFixtureKey");
  strcpy(settings.ytChannel, "UCfixturechannelid00");
  settings.instaFollowers = 4210;
  settings.xFollowers = 987;
  socialStats = SocialStats();
  socialStats.ytOk = socialStats.ghOk = true;
  socialStats.ytSubscribers = 12480;
  socialStats.ghFollowers = 321;
  socialStats.updatedAt = NOW_MS - 60000;
}

static void session(int i, const char *name, AgentState st, uint32_t minutes, uint8_t tint) {
  AgentSession &s = sessions[i];
  s = AgentSession();
  snprintf(s.id, sizeof(s.id), "poster%d", i);
  strncpy(s.name, name, sizeof(s.name) - 1);
  s.state = st;
  s.enteredAt = NOW_MS - minutes * 60000 - 200;   // 200 ms into a second: the beat is lit
  s.lastSeen = s.enteredAt;
  s.tint = tint;
  s.active = true;
}

static void noSessions() {
  for (auto &s : sessions) s = AgentSession();
  focusedSession = -1;
}

// The cloud: one session, and it needs the owner
static void setOneSession() {
  noSessions();
  session(0, "carousel", AG_ATTENTION, 4, 0);
}

// The list: four sessions, one needs the owner
static void setFourSessions() {
  noSessions();
  session(0, "carousel", AG_TOOL, 12, 0);
  session(1, "site", AG_ATTENTION, 3, 1);
  session(2, "notes", AG_THINKING, 7, 2);
  session(3, "api", AG_IDLE, 25, 3);
}

// --- Drawing and writing ----------------------------------------------------------
typedef void (*Screen)(uint32_t f, bool init);

// The three frames nobody chooses, in the screens' signature
static void signIn(uint32_t, bool) { drawSignInCode("------", 0.6f); }   // the page never shows the code
static void locked(uint32_t, bool) { drawLockedState(STRINGS[LANG_EN]); }
static void noImage(uint32_t, bool) { drawNoImage(); }

// A screen's first frame, then `frames` more at 60 a second on the virtual
// clock; styled: the setting that holds its style. A tall cell is drawn on a
// 32x64 frame, written as such.
struct PosterCell {
  const char *name;
  uint8_t style;
  Screen screen;
  uint8_t Settings::*styled;
  uint16_t frames;
  bool tall;
  uint16_t px[PIXEL_COUNT];
};
static PosterCell cells[] = {
    {"NIMBUS", 0, screenMascotWalk, nullptr, 120, false, {}},
    {"CODE", CODE_CLOUD, screenAgentMonitor, &Settings::codeStyle, 0, false, {}},
    {"CODE", CODE_LIST, screenAgentMonitor, &Settings::codeStyle, 0, false, {}},
    {"DANCE", 0, screenMascotDance, nullptr, 120, false, {}},
    {"SLEEP", 0, screenMascotSleep, nullptr, 120, false, {}},
    {"RUN", 0, screenMascotRun, nullptr, 120, false, {}},
    {"LIFE", 0, screenLife, nullptr, 120, false, {}},
    {"BALL", 0, screenBall, nullptr, 300, false, {}},
    {"MATRIX", 0, screenMatrix, nullptr, 120, false, {}},
    {"PLASMA", 0, screenPlasma, nullptr, 120, false, {}},
    {"FIRE", 0, screenFire, nullptr, 120, false, {}},
    {"STARS", 0, screenStars, nullptr, 120, false, {}},
    {"TEXT", 0, screenText, nullptr, 160, false, {}},   // the default message, its left edge in view
    {"RAYS", 0, screenRays, nullptr, 120, false, {}},
    {"BARS", 0, screenBars, nullptr, 120, false, {}},
    {"BLACKHOLE", 0, screenBlackHole, nullptr, 120, false, {}},
    {"CAT", 0, screenCat, nullptr, 120, true, {}},
    {"ISS", 0, screenIss, nullptr, 120, false, {}},
    {"CLOCK", CLOCK_STONE, screenClock, &Settings::clockStyle, 0, false, {}},
    {"CLOCK", CLOCK_DIAL, screenClock, &Settings::clockStyle, 0, false, {}},
    {"CLOCK", CLOCK_CYCLE, screenClock, &Settings::clockStyle, 0, false, {}},
    {"WEATHER", WEATHER_GLANCE, screenWeather, &Settings::weatherStyle, 0, false, {}},
    {"WEATHER", WEATHER_SCENE, screenWeather, &Settings::weatherStyle, 0, false, {}},
    {"WEATHER", WEATHER_DAY, screenWeather, &Settings::weatherStyle, 0, false, {}},
    {"DATE", 0, screenDate, nullptr, 0, false, {}},
    {"SYSTEM", 0, screenSystem, nullptr, 120, false, {}},   // two seconds: the frame rate is measured
    {"MONEY", 0, screenMoney, nullptr, 0, false, {}},
    {"SOCIAL", 0, screenSocial, nullptr, 0, false, {}},
    {"IMAGE", 0, screenImage, nullptr, 60, false, {}},   // the sunset GIF
    {"SIGNIN", 0, signIn, nullptr, 0, false, {}},
    {"LOCKED", 0, locked, nullptr, 0, false, {}},
    {"NOIMAGE", 0, noImage, nullptr, 0, false, {}},
};
static const int CELL_COUNT = sizeof(cells) / sizeof(cells[0]);

static void draw(PosterCell &c) {
  settings = Settings();
  setTime();
  setWeather();
  setSystem();
  setSocial();
  playFile = sunsetGif();
  playOpen = false;
  srand(SEED);   // the random of the ambiances and the mascots' blinks: the same each time
  if (c.styled) settings.*c.styled = c.style;
  if (c.screen == screenAgentMonitor) {
    if (c.style == CODE_LIST) setFourSessions();
    else setOneSession();
  }
  if (c.tall) display->setRotation(1);
  portrait = c.tall;
  display->clearScreen();
  for (uint32_t f = 0; f <= c.frames; f++) {
    fakeClock = NOW_MS + f * 1000 / 60;
    c.screen(f, f == 0);
  }
  // The frame as the screen saw it, row after row: W x H of the orientation
  for (int y = 0; y < H(); y++)
    for (int x = 0; x < W(); x++) c.px[y * W() + x] = display->getPixel(x, y);
  display->setRotation(0);
  portrait = false;
  screenImageLeave();
}

static std::string cellDir() {
  std::string here = __FILE__;   // .../firmware/test/test_poster_cells/test_poster_cells.cpp
  const size_t cut = here.rfind("test/test_poster_cells/");
  TEST_ASSERT_TRUE_MESSAGE(cut != std::string::npos, here.c_str());
  return here.substr(0, cut) + ".pio/poster-cells/";
}

static std::string label(const PosterCell &c) {
  return std::string(c.name) + ":" + std::to_string(c.style);
}

void setUp() {}
void tearDown() {}

void test_every_screen_draws_a_picture() {
  for (PosterCell &c : cells) {
    draw(c);
    int lit = 0;
    for (uint16_t p : c.px) lit += p != 0;
    TEST_ASSERT_TRUE_MESSAGE(lit > 40, label(c).c_str());
  }
}

// Each style of a screen looks unlike the others: the picker can tell them apart
void test_the_styles_of_a_screen_differ() {
  for (int a = 0; a < CELL_COUNT; a++)
    for (int b = a + 1; b < CELL_COUNT; b++)
      if (!strcmp(cells[a].name, cells[b].name))
        TEST_ASSERT_TRUE(memcmp(cells[a].px, cells[b].px, sizeof(cells[a].px)) != 0);
}

// The same inputs draw the same cell, animated or not: the sheet changes only
// with the screens
void test_drawing_again_draws_the_same() {
  static PosterCell again;
  for (int i = 0; i < CELL_COUNT; i++) {
    again = cells[i];
    draw(again);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(cells[i].px, again.px, sizeof(again.px), label(cells[i]).c_str());
  }
}

// Every screen is its own picture, but for the ones that share one on purpose
void test_screens_look_unlike_each_other() {
  for (int a = 0; a < CELL_COUNT; a++)
    for (int b = a + 1; b < CELL_COUNT; b++)
      if (strcmp(cells[a].name, cells[b].name))
        TEST_ASSERT_TRUE_MESSAGE(memcmp(cells[a].px, cells[b].px, sizeof(cells[a].px)) != 0,
                                 (label(cells[a]) + " " + label(cells[b])).c_str());
}

// IMAGE plays the sunset through the decoder: its sky is the picture's, not a state's words
void test_image_plays_the_gif() {
  const PosterCell *image = nullptr;
  for (const PosterCell &c : cells) if (!strcmp(c.name, "IMAGE")) image = &c;
  TEST_ASSERT_NOT_NULL(image);
  int colours = 0;
  uint16_t seen[64];
  for (uint16_t p : image->px) {
    bool known = false;
    for (int i = 0; i < colours; i++) known |= seen[i] == p;
    if (!known && colours < 64) seen[colours++] = p;
  }
  TEST_ASSERT_GREATER_THAN(20, colours);   // a gradient, a sun, a ridge: not a state's few colours
}

void test_cells_are_written() {
  const std::string dir = cellDir();
  const std::string mk = "mkdir -p '" + dir + "'";
  TEST_ASSERT_EQUAL(0, system(mk.c_str()));
  for (const PosterCell &c : cells) {
    char name[32];
    snprintf(name, sizeof(name), "%s-%u.565", c.name, (unsigned)c.style);
    const std::string path = dir + name;
    FILE *f = fopen(path.c_str(), "wb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path.c_str());
    uint8_t bytes[PIXEL_COUNT * 2];
    for (int i = 0; i < PIXEL_COUNT; i++) {
      bytes[2 * i] = (uint8_t)(c.px[i] & 0xFF);
      bytes[2 * i + 1] = (uint8_t)(c.px[i] >> 8);
    }
    TEST_ASSERT_EQUAL(sizeof(bytes), fwrite(bytes, 1, sizeof(bytes), f));
    TEST_ASSERT_EQUAL(0, fclose(f));
  }
}

int main() {
  TEST_ASSERT_TRUE(display->allocate());
  for (int i = 0; i < 256; i++) sinTab[i] = (uint8_t)(127.5f + 127.4f * sinf(i * 2.0f * PI / 256.0f));
  UNITY_BEGIN();
  RUN_TEST(test_every_screen_draws_a_picture);
  RUN_TEST(test_the_styles_of_a_screen_differ);
  RUN_TEST(test_drawing_again_draws_the_same);
  RUN_TEST(test_screens_look_unlike_each_other);
  RUN_TEST(test_image_plays_the_gif);
  RUN_TEST(test_cells_are_written);
  return UNITY_END();
}
