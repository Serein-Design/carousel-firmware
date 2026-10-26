// SPDX-License-Identifier: AGPL-3.0-only
// Writes the pages built without the server into HTML files, for the browser
// checks of tools/webcheck (overflow, target sizes, contrast, axe, structure,
// focus, scripts off), which serves the folder as the panel would:
//   - wifi-<state>-<lang>.html: the Wi-Fi page, from web_access as web.cpp
//     sends it, in each state;
//   - signin-<state>-<lang>.html, locked-<variant>.html: the sign-in and
//     locked pages, from web_access as web.cpp sends them;
//   - error-<status>-<lang>.html: the error pages, from web_access as
//     web.cpp sends them, signed out (503: signed in, with the tabs);
//   - stage-<state>-<lang>.html: Stage, from web_stage as web.cpp sends it,
//     in each state;
//   - order-<state>-<lang>.html: Running order, from web_screens as web.cpp
//     sends it, in each state;
//   - screen-<slug>-<state>-<lang>.html: each screen's settings page, from
//     web_screens as web.cpp sends it: as it opens, and CLOCK's and TEXT's
//     saved and after a failed write;
//   - panel-<state>-<lang>.html: Panel, from web_panel as web.cpp sends it:
//     up to date, an update available, a failed write, saved;
//   - feedback-<state>-<lang>.html: Feedback, from web_panel as web.cpp sends
//     it: as it opens, with two requests quieted, a failed write, saved;
//   - images-<state>-<lang>.html: Images, from web_images as web.cpp sends
//     it: 24 images, empty, no storage area, and each upload's notice;
//   - image.gif: a stored image as /images/f sends it (test_gif's fixture);
//   - c.css.gz, p.png: the stylesheet's and the poster sheet's bytes as the
//     firmware serves them, at /s/<version>/c.css and /s/<version>/p.png;
//   - preview.bmp, info.json: a frame through the preview's encoder and
//     /api/info's screens for the same state, at /preview and /api/info.
//
//   pio test -e native -f test_web_pages
//   node tools/webcheck/webcheck.mjs
#include <unity.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <Preferences.h>

#include "../../src/i18n.cpp"
#include "../../src/json_lite.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/posters.cpp"
#include "../../src/content_logic.cpp"
#include "../../src/web_shell.cpp"
#include "../../src/web_images.cpp"
#include "../../src/web_screens.cpp"
#include "../../src/posters_logic.cpp"
#include "../../src/web_access.cpp"
#include "../../src/web_stage.cpp"
#include "../../src/web_panel.cpp"
#include "../../src/preview_logic.cpp"
#include "../../src/web_assets.cpp"
#include "../test_gif/fixtures.h"

// settings.cpp's lock, read by content_logic's choice
bool panelLocked() { return false; }

void setUp() {}
void tearDown() {}

// Under .pio/, which git ignores; pio runs the tests from the project folder
static const char *const OUT_DIR = ".pio/webcheck";

static const Space SPACE = {212 * 1024, 168 * 1024, 380 * 1024};

static ImageInfo info(const char *name, uint32_t size, bool portrait) {
  ImageInfo i = {};
  snprintf(i.name, sizeof(i.name), "%s", name);
  i.size = size;
  i.portrait = portrait;
  return i;
}

// The longest list the page can show: 24 images, some with a 40-character
// name without a break, which must wrap rather than widen the page.
static ImageIndex fullList() {
  ImageIndex idx;
  idx.put(info("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gif", 150 * 1024, false));
  idx.put(info("Rain-over-the-city-at-night-in-autumn.gif", 66 * 1024, true));
  for (int i = 0; idx.count < MAX_IMAGES; i++) {
    char name[IMAGE_NAME_MAX + 1];
    snprintf(name, sizeof(name), "image-%02d.gif", i);
    idx.put(info(name, 4096u * (i + 1), i % 3 == 0));
  }
  return idx;
}

// Any version: webcheck serves the stylesheet under every one
static const char *const VERSION = "0.0.0-test";

// Pages of an earlier run whose state is gone would be checked again
static void clearPages() {
  DIR *d = opendir(OUT_DIR);
  if (!d) return;
  for (dirent *e = readdir(d); e; e = readdir(d)) {
    const size_t n = strlen(e->d_name);
    if (n > 5 && !strcmp(e->d_name + n - 5, ".html"))
      unlink((std::string(OUT_DIR) + "/" + e->d_name).c_str());
  }
  closedir(d);
}

static void write(const std::string &name, const std::string &html) {
  mkdir(".pio", 0755);
  mkdir(OUT_DIR, 0755);
  const std::string path = std::string(OUT_DIR) + "/" + name;
  FILE *f = fopen(path.c_str(), "w");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, path.c_str());
  TEST_ASSERT_EQUAL(html.size(), fwrite(html.data(), 1, html.size(), f));
  TEST_ASSERT_EQUAL(0, fclose(f));
}

static size_t at(const std::string &s, const char *part, size_t from = 0) {
  const size_t p = s.find(part, from);
  TEST_ASSERT_TRUE_MESSAGE(p != std::string::npos, part);
  return p;
}

// --- Tests ---------------------------------------------------------------------

// What a page flushes, in one piece
static std::string flushed;
static void collect(String &s) {
  flushed += s.c_str();
  s = String();
}

static int screenIndex(const char *name) { return screenFind(name); }

// Every language, Running order in each state: the defaults, rotation on
// with a custom order and four screens off, a refusal, a failed write,
// saved, saved for the next restart, and the longest summaries with a
// 40-character choice
static void test_write_order_pages() {
  Settings stored;
  snprintf(stored.city, sizeof(stored.city), "Saint-Remy-en-Bouzemont-Saint-Genest-et-Isson");
  snprintf(stored.place, sizeof(stored.place),
           "Saint-Remy-en-Bouzemont-Saint-Genest-et-Isson, Grand Est, France");
  snprintf(stored.ghUser, sizeof(stored.ghUser), "a-github-user-name-of-thirty-nine-chars");
  snprintf(stored.ytChannel, sizeof(stored.ytChannel), "UC4R8DWoMoI7CAwX8_LjQHig");
  snprintf(stored.ytKey, sizeof(stored.ytKey), "stored");
  snprintf(stored.message, sizeof(stored.message), "WELCOME TO THE STUDIO, THE KETTLE IS ON * ");
  stored.salary = 123456.78f;
  stored.instaFollowers = 1240;
  stored.xFollowers = 312;

  Settings rotation = stored;
  rotation.rotateSec = 20;
  for (const char *n : {"SLEEP", "RUN", "BARS", "ISS DEMO"})
    rotation.screens.on &= ~(1u << screenIndex(n));
  const int cat = screenIndex("CAT");
  rotation.screens.order[1] = (uint8_t)cat;
  rotation.screens.order[cat] = 1;

  Settings refused = stored;
  refused.screens.on = 0;
  for (const char *n : {"CODE", "SLEEP", "RUN", "TEXT"}) refused.screens.on |= 1u << screenIndex(n);

  const char *const longChoice = "Rain-over-the-city-at-night-in-autumn.gif";
  const uint8_t weather = (uint8_t)screenFind("WEATHER");
  clearPages();   // the first test to write pages
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    const Shell sh = {&t, nullptr, "carousel.local", TAB_ORDER, true, VERSION};
    struct { const char *name; const Settings *values; ListCheck refusal; SaveResult result;
             const char *choice; } states[] = {
      {"default", &stored, LIST_OK, SAVE_NONE, ""},
      {"rotation", &rotation, LIST_OK, SAVE_NONE, "image-03.gif"},
      {"refused", &refused, LIST_NO_PORTRAIT, SAVE_REFUSED, ""},
      {"failed", &rotation, LIST_OK, SAVE_FAILED, ""},
      {"saved", &stored, LIST_OK, SAVE_DONE, ""},
      {"saved-later", &stored, LIST_OK, SAVE_LATER, ""},
      {"long", &stored, LIST_OK, SAVE_NONE, longChoice},
    };
    for (const auto &st : states) {
      OrderView v = {};
      v.screens = {st.values, &stored, st.refusal, false, {true, MAX_IMAGES, st.choice}};
      v.current = weather;
      v.portrait = false;
      v.social = {true, false, 12500, 0, 12};
      v.result = st.result;
      flushed.clear();
      String s;
      orderPage(s, sh, v, collect);
      collect(s);
      const std::string &html = flushed;
      TEST_ASSERT_TRUE(html.size() <= 24576);
      at(html, "<ol class=list data-h=");
      write(std::string("order-") + st.name + "-" + t.code + ".html", html);
    }
    // Each screen's page as it opens, with the longest values; CLOCK's and
    // TEXT's after a save and after a failed write too
    for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
      if (!screenSettings(i)) continue;
      char slug[SCREEN_SLUG_MAX];
      screenSlug(i, slug, sizeof(slug));
      const bool saves = i == screenIndex("CLOCK") || i == screenIndex("TEXT");
      for (SaveResult r : {SAVE_NONE, SAVE_DONE, SAVE_FAILED}) {
        if (r != SAVE_NONE && !saves) continue;
        OrderView v = {};
        v.screens = {&stored, &stored, LIST_OK, false, {true, MAX_IMAGES, longChoice}};
        v.current = weather;
        v.social = {true, false, 12500, 0, 12};
        v.result = r;
        flushed.clear();
        String s;
        screenPage(s, sh, v, i, collect);
        collect(s);
        TEST_ASSERT_TRUE(flushed.size() <= 24576);
        at(flushed, "<div class=sp>");
        const char *state = r == SAVE_NONE ? "default" : r == SAVE_DONE ? "saved" : "failed";
        write(std::string("screen-") + slug + "-" + state + "-" + t.code + ".html", flushed);
      }
      // WEATHER right after a save: the city typed, its place not found yet
      if (i == weather) {
        Settings searching = stored;
        searching.place[0] = '\0';
        OrderView v = {};
        v.screens = {&searching, &searching, LIST_OK, false, {true, MAX_IMAGES, longChoice}};
        v.current = weather;
        v.social = {true, false, 12500, 0, 12};
        v.result = SAVE_DONE;
        flushed.clear();
        String s;
        screenPage(s, sh, v, i, collect);
        collect(s);
        TEST_ASSERT_TRUE(flushed.size() <= 24576);
        at(flushed, "<b id=fd ");
        write(std::string("screen-") + slug + "-searching-" + t.code + ".html", flushed);
      }
    }
  }
}

// The stylesheet's and the poster sheet's bytes as sent, for webcheck's server
static void test_write_stylesheet() {
  write("c.css.gz", std::string((const char *)STYLESHEET_GZ, STYLESHEET_GZ_LEN));
  write("p.png", std::string((const char *)POSTERS_PNG, POSTERS_PNG_LEN));
}

// Sign-in as it first opens and after a wrong code, in every language; the
// locked page in its two variants (English and French on one page)
static void test_write_access_pages() {
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    const Shell sh = {&t, nullptr, "carousel.local", TAB_NONE, false, VERSION};
    for (bool wrong : {false, true}) {
      String s;
      signInPage(s, sh, SignInView{wrong, "/screens"});
      const std::string html = s.c_str();
      TEST_ASSERT_TRUE(html.size() <= 24576);
      at(html, "<link rel=stylesheet href=\"/s/0.0.0-test/c.css\">");
      write(std::string("signin-") + (wrong ? "wrong" : "default") + "-" + t.code + ".html", html);
    }
  }
  const Shell sh = {&STRINGS[LANG_EN], nullptr, "carousel.local", TAB_NONE, false, VERSION};
  for (bool knob : {false, true}) {
    String s;
    lockedPage(s, sh, knob);
    const std::string html = s.c_str();
    TEST_ASSERT_TRUE(html.size() <= 24576);
    write(knob ? "locked-knob.html" : "locked-marker.html", html);
  }
}

// The error pages a browser route answers: not found, too long, short of
// memory, in every language
static void test_write_error_pages() {
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    for (int code : {404, 413, 503}) {
      const Shell sh = {&t, nullptr, "carousel.local", TAB_NONE, code == 503, VERSION};
      String s;
      errorPage(s, sh, code, code == 413 ? TAB_IMAGES : TAB_STAGE);
      const std::string html = s.c_str();
      TEST_ASSERT_TRUE(html.size() <= ERROR_PAGE_RESERVE);
      write("error-" + std::to_string(code) + "-" + t.code + ".html", html);
    }
  }
}

static WifiNet wifiNet(const char *ssid, int16_t rssi, bool open) {
  WifiNet n = {};
  snprintf(n.ssid, sizeof(n.ssid), "%s", ssid);
  n.rssi = rssi;
  n.open = open;
  return n;
}

// Every state of the Wi-Fi page, in every language: setup with 30 long names
// (some without a break, which must wrap) and a refused last try, the home
// network (known and in range), a first setup (nothing saved), the saved
// network out of range, a scan not done in time, and each message
static void test_write_wifi_pages() {
  WifiNet crowded[WIFI_ROWS_MAX];
  for (int i = 0; i < WIFI_ROWS_MAX; i++) {
    char name[33];
    if (i % 3 == 0) snprintf(name, sizeof(name), "NETWORK-WITHOUT-ANY-BREAK-%06d", i);
    else            snprintf(name, sizeof(name), "Freebox de la famille Martin %03d", i);
    crowded[i] = wifiNet(name, (int16_t)(-38 - 2 * i), i % 4 == 1);
  }
  const WifiNet home[] = {wifiNet("Maison-2.4G", -52, false), wifiNet("Livebox-7F2A", -67, false),
                          wifiNet("Freebox_Invite", -71, true), wifiNet("DIRECT-roku-829-TV", -83, false)};
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    struct { const char *name; WifiPageView v; } states[] = {
      {"setup", {crowded[7].ssid, LINK_SETUP, "", TRY_REFUSED, crowded, WIFI_ROWS_MAX, nullptr, nullptr}},
      {"home", {"Maison-2.4G", LINK_CONNECTED, "192.168.1.20", TRY_NONE, home, 4, nullptr, nullptr}},
      {"first", {"", LINK_SETUP, "", TRY_NONE, home, 4, nullptr, nullptr}},
      {"away", {"Bureau-2.4G", LINK_SETUP, "", TRY_NOT_FOUND, home, 4, nullptr, nullptr}},
      {"scanning", {"Maison-2.4G", LINK_SETUP, "", TRY_NOT_FOUND, home, 0, nullptr, nullptr}},
      {"retry", {"Maison-2.4G", LINK_SETUP, "", TRY_REFUSED, home, 4, t.wifiMsgRetry, nullptr}},
      {"bad-name", {"Maison-2.4G", LINK_SETUP, "", TRY_NONE, home, 4, t.wifiMsgBadSsid, nullptr}},
      {"bad-password", {"Maison-2.4G", LINK_CONNECTED, "192.168.1.20", TRY_NONE, home, 4,
                        t.wifiMsgBadPassword, nullptr}},
      {"saved", {"Maison-2.4G", LINK_SETUP, "", TRY_NONE, home, 4, t.wifiMsgSaved,
                 crowded[0].ssid}},
    };
    for (const auto &st : states) {
      const Shell sh = {&t, nullptr, "carousel.local", TAB_NONE, true, VERSION};
      String s;
      wifiPage(s, sh, st.v);
      const std::string html = s.c_str();
      TEST_ASSERT_TRUE(html.size() <= 24576);
      at(html, "<link rel=stylesheet href=\"/s/0.0.0-test/c.css\">");
      write(std::string("wifi-") + st.name + "-" + t.code + ".html", html);
    }
  }
}

static SessionRow sessionRow(const char *name, uint8_t tint, uint8_t state, uint32_t age) {
  SessionRow r = {};
  snprintf(r.name, sizeof(r.name), "%s", name);
  r.tint[0] = (uint8_t)(0x40 + 0x30 * tint);
  r.tint[1] = (uint8_t)(0xd0 - 0x20 * tint);
  r.tint[2] = 0x80;
  r.state = state;
  r.ageSec = age;
  return r;
}

// Stage in every state, in every language: the rotation running, off,
// nothing else to change to, portrait, no session, six sessions with
// 13-character names, a Show refused and the three Send notices; the
// preview and /api/info its script reads
static void test_write_stage_pages() {
  const ScreenList all = screenListDefault();
  ScreenList alone = all;
  alone.on = (1u << screenFind("CLOCK")) | (1u << screenFind("CAT"));
  const SessionRow two[] = {sessionRow("carousel-firm", 0, AG_TOOL, 4),
                            sessionRow("serein-site", 1, AG_IDLE, 130)};
  SessionRow six[6];
  for (int i = 0; i < 6; i++)
    six[i] = sessionRow(i % 2 ? "<&\"very-long>" : "PROJECTWITHOU", (uint8_t)i, (uint8_t)i,
                        7u + 900u * i);
  const uint8_t weather = (uint8_t)screenFind("WEATHER"), cat = (uint8_t)screenFind("CAT");
  struct { const char *name; ScreenList list; uint8_t current; bool portrait; uint16_t rotateSec;
           int32_t nextIn; const SessionRow *rows; int count; StageNotice notice; } states[] = {
    {"rotation", all, weather, false, 20, 14, two, 2, STAGE_NO_NOTICE},
    {"knob", all, weather, false, 0, -1, two, 2, STAGE_NO_NOTICE},
    {"alone", alone, (uint8_t)screenFind("CLOCK"), false, 20, -1, two, 2, STAGE_NO_NOTICE},
    {"portrait", all, cat, true, 30, 7, two, 2, STAGE_NO_NOTICE},
    {"no-session", all, weather, false, 20, 14, nullptr, 0, STAGE_NO_NOTICE},
    {"sessions", all, weather, false, 300, 299, six, 6, STAGE_NO_NOTICE},
    {"show-refused", all, weather, false, 20, 14, two, 2, STAGE_SHOW_OTHER},
    {"text-off", all, weather, false, 20, 14, two, 2, STAGE_TEXT_OFF},
    {"text-portrait", all, cat, true, 30, 7, two, 2, STAGE_TEXT_PORTRAIT},
    {"text-failed", all, weather, false, 20, 14, two, 2, STAGE_TEXT_FAILED},
  };
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    const Shell sh = {&t, nullptr, "carousel.local", TAB_STAGE, true, VERSION};
    for (const auto &st : states) {
      StageView v = {};
      v.list = st.list;
      v.current = st.current;
      v.portrait = st.portrait;
      v.rotateSec = st.rotateSec;
      v.nextIn = st.nextIn;
      v.sessions = st.rows;
      v.sessionCount = st.count;
      // Night dims to 10 %; the knob's level, one step from the top
      v.level = 4;
      v.levels = 6;
      v.nightStart = 23;
      v.nightEnd = 7;
      v.nightBrightness = 25;
      v.message = "Back at 2 pm, caf\xC3\xA9 & \"cr\xC3\xA8me\" * ";
      v.notice = st.notice;
      v.noticeName = t.scrCat;   // drawn for portrait only
      flushed.clear();
      String s;
      stagePage(s, sh, v, collect);
      collect(s);
      const std::string html = flushed;
      TEST_ASSERT_TRUE(html.size() <= 24576);
      at(html, "<img class=led src=\"/preview\"");
      write(std::string("stage-") + st.name + "-" + t.code + ".html", html);
    }
  }
  // A frame with something in every corner, as the panel would hold it
  uint16_t frame[PREVIEW_W * PREVIEW_H];
  for (int y = 0; y < PREVIEW_H; y++)
    for (int x = 0; x < PREVIEW_W; x++)
      frame[y * PREVIEW_W + x] = (uint16_t)(((x * 31 / 63) << 11) | ((y * 63 / 31) << 5) |
                                            ((x + y) % 2 ? 0x1F : 0));
  std::string bmp(PREVIEW_BMP_SIZE, '\0');
  previewBmp(frame, 0, false, (uint8_t *)&bmp[0]);
  write("preview.bmp", bmp);
  // The same frame tilted, 32 x 64 as /preview sends it in portrait: the
  // portrait pages are laid out around the image they really get
  previewBmp(frame, 1, false, (uint8_t *)&bmp[0]);
  write("preview-portrait.bmp", bmp);
  // /api/info as web.cpp builds it, for the rotation state
  String j = "{\"device\":\"carousel\",\"version\":\"0.0.0-test\",\"screen\":\"WEATHER\","
             "\"orientation\":\"landscape\",\"brightness\":3,\"brightness_levels\":6";
  screensInfoJson(j, all, 20, 14, false);
  j += '}';
  at(j.c_str(), "\"rotate_next_in\":14");
  write("info.json", j.c_str());
}

// Images in every state, in every language: 24 images (40-character names,
// both orientations, one chosen), empty, no storage area, then the answer
// to each upload, its notice above Add a GIF; the thumbnails' file
static void test_write_images_pages() {
  const ImageIndex full = fullList();
  const Space empty = {0, 380 * 1024, 380 * 1024};
  const std::string sent = "Very-long-name-of-a-picture-from-the-phone.png";
  struct { const char *name; UploadResult result; } notices[] = {
    {"added", UP_ADDED}, {"replaced", UP_REPLACED}, {"no-file", UP_NO_FILE},
    {"no-storage", UP_NO_STORAGE}, {"too-big", UP_TOO_BIG}, {"not-gif", UP_NOT_GIF},
    {"wrong-size", UP_WRONG_SIZE}, {"no-room", UP_NO_ROOM}, {"too-many", UP_TOO_MANY},
    {"failed", UP_FAILED},
  };
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    const Shell sh = {&t, nullptr, "carousel.local", TAB_IMAGES, true, VERSION};
    auto send = [&](const std::string &state, const ImagesPageView &v) {
      flushed.clear();
      String s;
      imagesPage(s, sh, v, collect);
      collect(s);
      TEST_ASSERT_TRUE(flushed.size() <= 24576);
      write("images-" + state + "-" + t.code + ".html", flushed);
    };
    send("full", {true, full.items, full.count, SPACE, "image-03.gif", nullptr, 7});
    send("empty", {true, nullptr, 0, empty, "", nullptr, 0});
    send("unavailable", {false, nullptr, 0, {0, 0, 0}, "", nullptr, 0});
    for (const auto &n : notices) {
      // The image off in Running order on a stored one: the longest answer
      const ImagesNotice notice = {n.result, sent.c_str(), 120 * 1024, 20 * 1024,
                                   {KIND_PNG, 128, 128}, "image-03.gif", true};
      send(std::string("notice-") + n.name,
           {true, full.items, full.count, SPACE, "image-03.gif", &notice, 8});
    }
  }
  write("image.gif", std::string((const char *)GIF_ANIMATED, sizeof(GIF_ANIMATED)));
}

// Every language, Panel in each state: up to date, an update available, a
// failed write (the posted hours shown), saved, hooks accepted without a
// token, three hook calls refused
static void test_write_panel_pages() {
  Settings stored;
  stored.nightStart = 23;
  stored.nightEnd = 7;
  stored.nightBrightness = 25;
  snprintf(stored.mqttHost, sizeof(stored.mqttHost), "homeassistant.local");
  snprintf(stored.mqttUser, sizeof(stored.mqttUser), "carousel");
  snprintf(stored.mqttPass, sizeof(stored.mqttPass), "stored");
  Settings posted = stored;
  posted.nightStart = 22;
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    const Shell sh = {&t, nullptr, "carousel.local", TAB_PANEL, true, VERSION};
    struct { const char *name; const Settings *values; SaveResult result; UpdateState update;
             bool hookOpen; uint32_t hookRefusals; } states[] = {
      {"default", &stored, SAVE_NONE, UPD_UP_TO_DATE, false, 0},
      {"update-available", &stored, SAVE_NONE, UPD_AVAILABLE, false, 0},
      {"failed", &posted, SAVE_FAILED, UPD_IDLE, false, 0},
      {"saved", &stored, SAVE_DONE, UPD_UP_TO_DATE, false, 0},
      {"hooks-open", &stored, SAVE_NONE, UPD_UP_TO_DATE, true, 0},
      {"hooks-refused", &stored, SAVE_NONE, UPD_UP_TO_DATE, false, 3},
    };
    for (const auto &st : states) {
      PanelView v = {};
      v.values = st.values;
      v.stored = &stored;
      v.result = st.result;
      v.refusal = LIST_OK;
      v.update = st.update;
      v.latest = "0.2.0";
      v.updateError = "";
      v.ssid = "Maison-2.4G";
      v.rssi = -58;
      v.connected = true;
      v.mqtt = MQTT_UP;
      v.sessionCount = 2;
      v.ip = "192.168.1.50";
      v.token = "c7f2a91e5b0d4836af1e27c9d04b6e13";
      v.hookToken = "5e0b7d19a4c3482f9d61b8a2e7f40c35";
      v.hookOpen = st.hookOpen;
      v.hookRefusals = st.hookRefusals;
      v.freeKb = 142;
      v.minFreeKb = 98;
      v.resetReason = "power on";
      v.upSec = 3 * 3600;
      flushed.clear();
      String s;
      panelPage(s, sh, v, collect);
      collect(s);
      TEST_ASSERT_TRUE(flushed.size() <= 24576);
      at(flushed, "<section id=adv ");
      at(flushed, "id=hkc");
      TEST_ASSERT_EQUAL(st.hookOpen, flushed.find(t.pnlHookOpenOn) != std::string::npos);
      TEST_ASSERT_EQUAL(st.hookRefusals > 0, flushed.find("id=hkr") != std::string::npos);
      write(std::string("panel-") + st.name + "-" + t.code + ".html", flushed);
    }
  }
}

// Feedback's page in each language: as loaded, one request kind off, a
// failed write (the posted choices shown), saved
static void test_write_feedback_pages() {
  Settings stored;
  Settings posted = stored;
  posted.transition = TRANSITION_WIPE;
  posted.requestShow[REQUEST_COMMAND] = REQUEST_OFF;
  Settings quiet = stored;
  quiet.requestShow[REQUEST_IMAGE] = REQUEST_DISCREET;
  quiet.requestShow[REQUEST_AGENT] = REQUEST_OFF;
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    const Shell sh = {&t, nullptr, "carousel.local", TAB_PANEL, true, VERSION};
    struct { const char *name; const Settings *values; SaveResult result; } states[] = {
      {"default", &stored, SAVE_NONE},
      {"quiet", &quiet, SAVE_NONE},
      {"failed", &posted, SAVE_FAILED},
      {"saved", &quiet, SAVE_DONE},
    };
    for (const auto &st : states) {
      PanelView v = {};
      v.values = st.values;
      v.stored = &stored;
      v.result = st.result;
      v.refusal = LIST_OK;
      flushed.clear();
      String s;
      feedbackPage(s, sh, v, collect);
      collect(s);
      TEST_ASSERT_TRUE(flushed.size() <= 24576);
      at(flushed, "<h2 id=rq>");
      write(std::string("feedback-") + st.name + "-" + t.code + ".html", flushed);
    }
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_write_order_pages);
  RUN_TEST(test_write_stylesheet);
  RUN_TEST(test_write_wifi_pages);
  RUN_TEST(test_write_access_pages);
  RUN_TEST(test_write_error_pages);
  RUN_TEST(test_write_stage_pages);
  RUN_TEST(test_write_images_pages);
  RUN_TEST(test_write_panel_pages);
  RUN_TEST(test_write_feedback_pages);
  return UNITY_END();
}
