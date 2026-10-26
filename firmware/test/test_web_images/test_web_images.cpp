// SPDX-License-Identifier: AGPL-3.0-only
// The Images page, the settings page's Images block (under IMAGE's
// settings, until Running order is rebuilt), the image list JSON and the
// upload gate, without the web server: markup as the validated frames, names
// escaped, the page and the block within their budgets, every early refusal
// in order.
//
//   pio test -e native -f test_web_images
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <Preferences.h>

#include "../../src/i18n.cpp"
#include "../../src/content_logic.cpp"
#include "../../src/web_shell.cpp"
#include "../../src/web_images.cpp"

// settings.cpp's lock, read by content_logic's choice
bool panelLocked() { return false; }

void setUp() {}
void tearDown() {}

// --- Helpers -------------------------------------------------------------------

static const Strings &EN = STRINGS[LANG_EN];
static const Strings &FR = STRINGS[LANG_FR];
static const Space SPACE = {212 * 1024, 168 * 1024, 380 * 1024};

static ImageInfo info(const char *name, uint32_t size, bool portrait) {
  ImageInfo i = {};
  snprintf(i.name, sizeof(i.name), "%s", name);
  i.size = size;
  i.portrait = portrait;
  return i;
}

static bool has(const std::string &s, const std::string &part) {
  return s.find(part) != std::string::npos;
}
static size_t at(const std::string &s, const std::string &part) {
  const size_t p = s.find(part);
  TEST_ASSERT_TRUE_MESSAGE(p != std::string::npos, part.c_str());
  return p;
}
static int occurrences(const std::string &s, const std::string &part) {
  int n = 0;
  for (size_t p = s.find(part); p != std::string::npos; p = s.find(part, p + 1)) n++;
  return n;
}

static ImagesNotice notice(UploadResult r, const char *sent, const char *name = "") {
  ImagesNotice n = {r, sent, 0, 0, {KIND_OTHER, 0, 0}, name, false};
  return n;
}

// Three images out of order, sorted by the index as the page gets them
static ImageIndex threeImages() {
  ImageIndex idx;
  idx.put(info("pacman.gif", 65537, false));
  idx.put(info("Rain.gif", 66 * 1024, true));
  idx.put(info("nyan.gif", 65536, false));
  return idx;
}

// --- Section -------------------------------------------------------------------

// The locked page links the same README section its text names
static void test_reinstall_link() {
  String en, fr;
  reinstallLink(en, EN);
  reinstallLink(fr, FR);
  TEST_ASSERT_EQUAL_STRING(
      "<a class=\"go lk\" href=\"https://github.com/Serein-Design/carousel-firmware#build-and-flash\">"
      "How to reinstall over USB</a>", en.c_str());
  TEST_ASSERT_EQUAL_STRING(
      "<a class=\"go lk\" href=\"https://github.com/Serein-Design/carousel-firmware#build-and-flash\">"
      "R\u00e9installer par USB</a>", fr.c_str());
}

// The locked page's title names both languages: its language setting cannot
// be read
static void test_reset_failed_heading() {
  String title;
  resetFailedHeading(title);
  TEST_ASSERT_EQUAL_STRING("Factory reset unfinished / Remise \u00e0 z\u00e9ro inachev\u00e9e",
                           title.c_str());
}

// --- The Images page -----------------------------------------------------------

static std::string flushed;
static int flushes = 0;
static void collect(String &s) {
  flushed += s.c_str();
  s = "";
  flushes++;
}

static const uint32_t GENERATION = 4242;

static ImagesPageView pageView(bool storage, const ImageInfo *list, int count, const char *choice,
                               const ImagesNotice *n) {
  return {storage, list, count, SPACE, choice, n, GENERATION};
}

static std::string page(const Strings &t, const ImagesPageView &v) {
  flushed.clear();
  flushes = 0;
  const Shell sh = {&t, nullptr, "carousel.local", TAB_NONE, true, "0.0.0-test"};
  String s;
  imagesPage(s, sh, v, collect);
  collect(s);
  return flushed;
}

// The side column (title, lead, Space, Add a GIF, Panel shows), then the list
static void test_images_page_sections() {
  const ImageIndex idx = threeImages();
  const std::string s = page(EN, pageView(true, idx.items, idx.count, "", nullptr));
  const size_t h1 = at(s, "<h1>Images</h1>");
  const size_t lead = at(s, std::string("<p class=lead>") + EN.imgLead + "</p>");
  const size_t space = at(s, "<p>Space: <b class=num>212 KB used of 380 KB</b></p>"
                             "<div class=space aria-hidden=true><i style=\"width:56%\"></i></div>");
  const size_t add = at(s, "<h2 id=h-a>Add a GIF</h2>");
  const size_t form = at(s, "<form method=post action=\"/images\" enctype=multipart/form-data>"
                            "<label for=f>GIF file</label><input id=f type=file name=f "
                            "accept=image/gif><small>");
  at(s, std::string("<small>") + EN.webImgAddHint + "</small><button class=p");
  at(s, ">Upload</button></form>");
  const size_t shows = at(s, "<h2 id=h-s>Panel shows</h2>");
  const size_t pick = at(s, "<form method=post action=\"/images/show\" class=row><div>"
                            "<label for=ch>Image</label><select id=ch name=sh>"
                            "<option value=\"\" selected>All in turn</option>"
                            "<option>nyan.gif</option><option>pacman.gif</option>"
                            "<option>Rain.gif</option></select></div><button>Show</button>"
                            "</form>");
  at(s, std::string("<small>") + EN.webImgShowHint + "</small>");
  const size_t aside = at(s, "</aside>");
  const size_t list = at(s, "<h2 id=h-p>On the panel (3)</h2><ul class=imgs>");
  TEST_ASSERT_TRUE(h1 < lead && lead < space && space < add && add < form && form < shows &&
                   shows < pick && pick < aside && aside < list);
  TEST_ASSERT_EQUAL(3, occurrences(s, "<li>"));
  // Each row: name, size and orientation
  at(s, "<b>nyan.gif</b><small class=num style=\"margin-top:2px\">64×32 · Landscape "
        "· 64 KB</small>");
  at(s, "<b>Rain.gif</b><small class=num style=\"margin-top:2px\">32×64 · Portrait "
        "· 66 KB</small>");
  at(s, "<b>pacman.gif</b><small class=num style=\"margin-top:2px\">64×32 · Landscape "
        "· 65 KB</small>");
  // The old block's markup is gone from this page
  TEST_ASSERT_FALSE(has(s, "#image"));
  TEST_ASSERT_FALSE(has(s, "<h3"));
  // French: the same sections in its words
  const std::string f = page(FR, pageView(true, idx.items, idx.count, "", nullptr));
  at(f, std::string("<p>") + "Espace : <b class=num>");
  at(f, std::string(">") + FR.imgFileLabel + "</label>");
  at(f, std::string(">") + FR.imgSelectLabel + "</label>");
}

// Thumbnails: the stored file itself, loaded as it scrolls in, at an address
// that changes with every add, replace and delete
static void test_thumbnails_lazy_with_generation() {
  const ImageIndex idx = threeImages();
  const std::string s = page(EN, pageView(true, idx.items, idx.count, "", nullptr));
  at(s, "<li><img class=th src=\"/images/f?n=nyan.gif&amp;g=4242\" loading=lazy decoding=async "
        "width=64 height=32 alt=\"\"><div>");
  at(s, "<li><img class=\"th pt\" src=\"/images/f?n=Rain.gif&amp;g=4242\" loading=lazy "
        "decoding=async width=32 height=64 alt=\"\"><div>");
  TEST_ASSERT_EQUAL(3, occurrences(s, "loading=lazy"));
  ImagesPageView v = pageView(true, idx.items, idx.count, "", nullptr);
  v.generation = 4243;
  const std::string after = page(EN, v);
  TEST_ASSERT_EQUAL(3, occurrences(after, "&amp;g=4243\""));
  TEST_ASSERT_FALSE(has(after, "g=4242"));
}

// Each Delete names its image, escaped in the value and the name
static void test_delete_buttons_named() {
  const char *evil = "<script>\"'&.gif";
  const char *escaped = "&lt;script&gt;&quot;&#39;&amp;.gif";
  const ImageInfo list[] = {info(evil, 1024, false), info("b.gif", 1024, true)};
  for (const Strings *t : {&EN, &FR}) {
    const std::string s = page(*t, pageView(true, list, 2, evil, nullptr));
    TEST_ASSERT_FALSE(has(s, "<script>"));
    TEST_ASSERT_FALSE(has(s, "\"'&.gif"));
    std::string del = t->imgDeleteName;
    del.replace(del.find("%s"), 2, escaped);
    at(s, std::string("<form method=post action=\"/images/delete\"><input type=hidden name=n "
                      "value=\"") + escaped + "\"><button class=del aria-label=\"" + del + "\">" +
              t->webImgDelete + "</button></form></li>");
    at(s, std::string("src=\"/images/f?n=") + escaped + "&amp;g=");
    at(s, std::string("<b>") + escaped + "</b>");
    at(s, std::string("<option selected>") + escaped + "</option>");
    TEST_ASSERT_EQUAL(2, occurrences(s, "action=\"/images/delete\""));
  }
}

// The tag marks the chosen image only; all in turn marks none
static void test_chosen_tag() {
  const ImageIndex idx = threeImages();
  const std::string s = page(EN, pageView(true, idx.items, idx.count, "nyan.gif", nullptr));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<span class=tag>"));
  at(s, "<b>nyan.gif</b><small class=num style=\"margin-top:2px\">64×32 · Landscape "
        "· 64 KB</small><span class=tag>Panel shows this</span></div>");
  at(s, "<option value=\"\">All in turn</option>");
  at(s, "<option selected>nyan.gif</option>");
  TEST_ASSERT_EQUAL(1, occurrences(s, " selected"));
  const std::string all = page(FR, pageView(true, idx.items, idx.count, "", nullptr));
  TEST_ASSERT_FALSE(has(all, "class=tag"));
  TEST_ASSERT_EQUAL(1, occurrences(all, " selected"));
  // No choice (nullptr): all in turn, no tag
  const std::string none = page(EN, pageView(true, idx.items, idx.count, nullptr, nullptr));
  TEST_ASSERT_FALSE(has(none, "class=tag"));
  at(none, "<option value=\"\" selected>All in turn</option>");
  // A saved choice the list lacks (an index read in part): still its own
  // option, selected and escaped, so pressing Show keeps it; no tag
  const std::string partial = page(EN, pageView(true, idx.items, idx.count, "a&b.gif", nullptr));
  TEST_ASSERT_FALSE(has(partial, "class=tag"));
  at(partial, "<option value=\"\">All in turn</option>");
  at(partial, "<option selected>a&amp;b.gif</option></select>");
  TEST_ASSERT_EQUAL(1, occurrences(partial, " selected"));
}

static void test_images_empty() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string s = page(*t, pageView(true, nullptr, 0, "", nullptr));
    at(s, "<h2 id=h-a>");
    at(s, "action=\"/images\" enctype=multipart/form-data>");
    TEST_ASSERT_FALSE(has(s, "<select"));
    TEST_ASSERT_FALSE(has(s, "id=h-s"));
    TEST_ASSERT_FALSE(has(s, "<ul"));
    TEST_ASSERT_FALSE(has(s, "/images/f"));
    char heading[64];
    snprintf(heading, sizeof(heading), t->webImgOnPanel, 0);
    char frame[64];
    snprintf(frame, sizeof(frame), "<i class=th style=\"--p:%d\" aria-hidden=true></i>",
             POSTER_NO_IMAGE);
    at(s, std::string("<h2 id=h-p>") + heading + "</h2><div class=empty>" + frame + "<p><b>" +
              t->webImgNone + "</b></p><small style=\"margin:8px auto 0\">" + t->imgEmptyHint +
              "</small></div></section>");
  }
  // No area at all (total 0): no division by zero
  ImagesPageView v = pageView(true, nullptr, 0, "", nullptr);
  v.space = {0, 0, 0};
  TEST_ASSERT_TRUE(has(page(EN, v), "<i style=\"width:0%\">"));
}

// Without a storage area: the existing words and the reinstall link, no form
static void test_images_no_storage() {
  const ImageIndex idx = threeImages();
  const ImagesNotice n = notice(UP_NO_STORAGE, "a.gif");
  for (const Strings *t : {&EN, &FR}) {
    const std::string s = page(*t, pageView(false, idx.items, idx.count, "", &n));
    at(s, "<h1>Images</h1>");
    at(s, std::string("<p class=lead>") + t->imgLead + "</p>");
    at(s, std::string("<b>") + t->webImgNa + "</b>");
    at(s, std::string("<p>") + t->webImgNaText + "</p>");
    at(s, "href=\"https://github.com/Serein-Design/carousel-firmware#build-and-flash\">");
    TEST_ASSERT_FALSE(has(s, "<form"));
    TEST_ASSERT_FALSE(has(s, "/images/f"));
    TEST_ASSERT_FALSE(has(s, "role=alert"));
    TEST_ASSERT_FALSE(has(s, "pacman.gif"));
  }
}

// Every upload's answer: its notice in Add a GIF, above the form, a refusal
// tied to the file field
static void test_notice_above_add_a_gif() {
  const ImageIndex idx = threeImages();
  const UploadResult all[] = {UP_ADDED, UP_REPLACED, UP_NO_FILE, UP_NO_STORAGE, UP_TOO_BIG,
                              UP_NOT_GIF, UP_WRONG_SIZE, UP_NO_ROOM, UP_TOO_MANY, UP_FAILED};
  for (const Strings *t : {&EN, &FR})
    for (UploadResult r : all) {
      ImagesNotice n = notice(r, "pac man.gif", "pacman.gif");
      n.header = {KIND_PNG, 10, 10};
      const std::string s = page(*t, pageView(true, idx.items, idx.count, "", &n));
      const bool ok = r == UP_ADDED || r == UP_REPLACED;
      const size_t heading = at(s, "<h2 id=h-a>");
      const size_t box = at(s, ok ? "<div class=ok role=status>" : "<div class=er role=alert id=e1>");
      const size_t form = at(s, "<form method=post action=\"/images\"");
      TEST_ASSERT_TRUE_MESSAGE(heading < box && box < form, std::to_string(r).c_str());
      TEST_ASSERT_EQUAL(1, occurrences(s, "role=alert") + occurrences(s, "role=status"));
      if (ok) {
        TEST_ASSERT_FALSE(has(s, "aria-invalid"));
      } else {
        at(s, std::string("<b>") + t->webImgNotAdded + "</b> ");
        at(s, "<input id=f type=file name=f accept=image/gif aria-invalid=true "
              "aria-describedby=e1 autofocus>");
      }
    }
  // No notice: nothing above the form
  const std::string plain = page(EN, pageView(true, idx.items, idx.count, "", nullptr));
  TEST_ASSERT_FALSE(has(plain, "role=alert"));
  TEST_ASSERT_FALSE(has(plain, "role=status"));
  const ImagesNotice pending = notice(UP_PENDING, "");
  TEST_ASSERT_FALSE(has(page(EN, pageView(true, idx.items, idx.count, "", &pending)), "role="));
}

// An image stored while IMAGE is off says where to turn it on
static void test_image_off_names_running_order() {
  const ImageIndex idx = threeImages();
  ImagesNotice added = notice(UP_ADDED, "a.gif", "a.gif");
  added.imageOff = true;
  at(page(EN, pageView(true, idx.items, idx.count, "", &added)),
     "<div class=ok role=status><b>a.gif</b> added. IMAGE is off in Running order, so the panel "
     "does not show it.</div>");
  at(page(FR, pageView(true, idx.items, idx.count, "", &added)),
     std::string(" ") + FR.webImgOff + "</div>");
  TEST_ASSERT_NOT_NULL(strstr(FR.webImgOff, "l'ordre de passage"));
}

static void test_one_h1_one_main() {
  const ImageIndex idx = threeImages();
  for (const Strings *t : {&EN, &FR})
    for (bool storage : {true, false})
      for (int count : {0, 3}) {
        const std::string s = page(*t, pageView(storage, idx.items, count, "", nullptr));
        TEST_ASSERT_EQUAL(1, occurrences(s, "<h1"));
        TEST_ASSERT_EQUAL(1, occurrences(s, "<main"));
        at(s, std::string("<title>") + t->webTabs[TAB_IMAGES] + " \xC2\xB7 Carousel</title>");
        at(s, "<a href=\"/images\" aria-current=page>");
        TEST_ASSERT_EQUAL(occurrences(s, "<form"), occurrences(s, "<form method=post action=\""));
      }
}

// The worst case: 24 images with 40-character names, the longest notice,
// French, sent in pieces: under 24 KB as sent
static void test_page_fits_24k() {
  ImageInfo list[MAX_IMAGES];
  for (int i = 0; i < MAX_IMAGES; i++) {
    char name[IMAGE_NAME_MAX + 1];
    snprintf(name, sizeof(name), "%02d-%s", i, "abcdefghijklmnopqrstuvwxyz0123456.gif");
    list[i] = info(name, MAX_IMAGE_BYTES, i % 2);
  }
  const std::string sent(60, '"');
  const std::string name(IMAGE_NAME_MAX, '"');
  const Space big = {4000000000u, 4000000000u, 4000000000u};
  size_t longest = 0;
  for (int l = 0; l < LANG_COUNT; l++)
    for (UploadResult r : {UP_ADDED, UP_REPLACED, UP_NO_FILE, UP_TOO_BIG, UP_NOT_GIF,
                           UP_WRONG_SIZE, UP_NO_ROOM, UP_TOO_MANY, UP_FAILED}) {
      ImagesNotice n = {r, sent.c_str(), 4000000000u, 4000000000u, {KIND_WEBP, 65535, 65535},
                        name.c_str(), true};
      ImagesPageView v = {true, list, MAX_IMAGES, big, list[23].name, &n, 4294967295u};
      const std::string s = page(STRINGS[l], v);
      TEST_ASSERT_EQUAL(MAX_IMAGES, occurrences(s, "<li>"));
      TEST_ASSERT_TRUE(flushes >= 3);   // sent in pieces, never one whole block
      if (s.size() > longest) longest = s.size();
    }
  printf("Images page, longest: %u of 24576 bytes\n", (unsigned)longest);
  TEST_ASSERT_TRUE_MESSAGE(longest <= 24576, std::to_string(longest).c_str());
}

// --- JSON ----------------------------------------------------------------------

static void test_json_list() {
  const ImageInfo one[] = {info("pacman.gif", 66560, false)};
  const Space sp = {66560, 321536, 388096};
  String s;
  imagesJson(s, true, one, 1, sp, "");
  TEST_ASSERT_EQUAL_STRING(
      "{\"images\":[{\"name\":\"pacman.gif\",\"orientation\":\"landscape\",\"size\":66560}],"
      "\"count\":1,\"max\":24,\"used\":66560,\"left\":321536,\"total\":388096,"
      "\"choice\":\"all\"}",
      s.c_str());

  const ImageIndex idx = threeImages();
  String m;
  imagesJson(m, true, idx.items, idx.count, SPACE, "Rain.gif");
  const std::string j = m.c_str();
  TEST_ASSERT_TRUE(at(j, "\"nyan.gif\"") < at(j, "\"pacman.gif\""));
  TEST_ASSERT_TRUE(has(j, "{\"name\":\"Rain.gif\",\"orientation\":\"portrait\",\"size\":67584}]"));
  TEST_ASSERT_TRUE(has(j, "\"count\":3,"));
  TEST_ASSERT_TRUE(has(j, "\"choice\":\"Rain.gif\"}"));

  String e;
  imagesJson(e, true, nullptr, 0, {0, 0, 0}, "");
  TEST_ASSERT_EQUAL_STRING("{\"images\":[],\"count\":0,\"max\":24,\"used\":0,\"left\":0,"
                           "\"total\":0,\"choice\":\"all\"}",
                           e.c_str());

  // Names JSON-escaped, wherever they come from
  const ImageInfo odd[] = {info("a\"b\\c\x01.gif", 1, false)};
  String o;
  imagesJson(o, true, odd, 1, sp, odd[0].name);
  TEST_ASSERT_TRUE(has(o.c_str(), "\"name\":\"a\\\"b\\\\c\\u0001.gif\""));
  TEST_ASSERT_TRUE(has(o.c_str(), "\"choice\":\"a\\\"b\\\\c\\u0001.gif\"}"));

  String n;
  imagesJson(n, false, one, 1, sp, "");
  TEST_ASSERT_EQUAL_STRING("{\"error\":\"no storage area\"}", n.c_str());
}

// --- Gate ----------------------------------------------------------------------

static GateInput owner(bool api) {
  GateInput in = {api, true, false, api, !api, 2048};
  return in;
}

static void test_gate_refusals_before_write() {
  for (int api = 0; api < 2; api++) {
    TEST_ASSERT_EQUAL(GATE_ACCEPT, uploadGate(owner(api)));
    GateInput in = owner(api);
    in.announced = 0;   // unknown: counted while it arrives
    TEST_ASSERT_EQUAL(GATE_ACCEPT, uploadGate(in));

    // Unknown host and cross-site come first, whatever else the request says
    for (int signedIn = 0; signedIn < 2; signedIn++) {
      for (uint32_t len : {2048u, 200u * 1024, 5u * 1024 * 1024}) {
        GateInput h = {(bool)api, false, false, (bool)signedIn, (bool)signedIn, len};
        TEST_ASSERT_EQUAL(GATE_CLOSE_403, uploadGate(h));
        GateInput c = {(bool)api, true, true, (bool)signedIn, (bool)signedIn, len};
        TEST_ASSERT_EQUAL(GATE_CLOSE_403, uploadGate(c));
      }
    }
    // Then authorisation, before the length is looked at
    for (uint32_t len : {2048u, 200u * 1024, 5u * 1024 * 1024}) {
      GateInput n = {(bool)api, true, false, false, false, len};
      TEST_ASSERT_EQUAL(api ? GATE_CLOSE_401 : GATE_SIGN_IN, uploadGate(n));
    }
  }
  // The API takes the token or the session; the page only the session
  GateInput a = {true, true, false, false, true, 2048};
  TEST_ASSERT_EQUAL(GATE_ACCEPT, uploadGate(a));
  GateInput p = {false, true, false, true, false, 2048};
  TEST_ASSERT_EQUAL(GATE_SIGN_IN, uploadGate(p));
}

static void test_gate_oversize() {
  const uint32_t cap = MAX_IMAGE_BYTES + MULTIPART_SLACK;
  for (int api = 0; api < 2; api++) {
    GateInput in = owner(api);
    in.announced = cap;
    TEST_ASSERT_EQUAL(GATE_ACCEPT, uploadGate(in));
    in.announced = cap + 1;
    TEST_ASSERT_EQUAL(api ? GATE_CLOSE_413 : GATE_DRAIN_413, uploadGate(in));
    in.announced = PAGE_DRAIN_MAX;
    TEST_ASSERT_EQUAL(api ? GATE_CLOSE_413 : GATE_DRAIN_413, uploadGate(in));
    in.announced = PAGE_DRAIN_MAX + 1;
    TEST_ASSERT_EQUAL(GATE_CLOSE_413, uploadGate(in));
    in.announced = UINT32_MAX;
    TEST_ASSERT_EQUAL(GATE_CLOSE_413, uploadGate(in));
  }
}

// No storage area: the gate lets the owner through, the upload answers 503
// before anything is written.
struct NoStore : ContentStore {
  int calls = 0;
  bool beginTemp() override { calls++; return false; }
  bool writeTemp(const uint8_t *, size_t) override { calls++; return false; }
  void dropTemp() override { calls++; }
  bool commitTemp(const char *) override { calls++; return false; }
  bool remove(const char *) override { calls++; return false; }
  uint32_t freeBlocks() override { return 0; }
  uint32_t blockSize() override { return 4096; }
};

static void test_gate_no_storage_503() {
  for (int api = 0; api < 2; api++) {
    TEST_ASSERT_EQUAL(GATE_ACCEPT, uploadGate(owner(api)));
    NoStore store;
    ImageIndex idx;
    ImageUpload up;
    const UploadResult r = up.start(store, idx, false, "a.gif", 2048);
    TEST_ASSERT_EQUAL(UP_NO_STORAGE, r);
    TEST_ASSERT_EQUAL(503, uploadStatus(r));
    const uint8_t gif[] = {'G', 'I', 'F', '8', '9', 'a', 64, 0, 32, 0, 0, 0};
    TEST_ASSERT_EQUAL(UP_NO_STORAGE, up.write(store, gif, sizeof(gif)));
    TEST_ASSERT_EQUAL(0, store.calls);
  }
}

// Only the first file part of a body is stored, however many follow.
// A byte counter wrapped at 256, so the 257th part was the first again.
static void test_only_first_file_part() {
  FileParts parts;
  TEST_ASSERT_FALSE(parts.first());
  parts.next();
  TEST_ASSERT_TRUE(parts.first());
  for (int n = 2; n <= 1000; n++) {
    parts.next();
    TEST_ASSERT_FALSE_MESSAGE(parts.first(), std::to_string(n).c_str());
  }
  parts.reset();   // the next request
  TEST_ASSERT_FALSE(parts.first());
  parts.next();
  TEST_ASSERT_TRUE(parts.first());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_reinstall_link);
  RUN_TEST(test_reset_failed_heading);
  RUN_TEST(test_images_page_sections);
  RUN_TEST(test_thumbnails_lazy_with_generation);
  RUN_TEST(test_delete_buttons_named);
  RUN_TEST(test_chosen_tag);
  RUN_TEST(test_images_empty);
  RUN_TEST(test_images_no_storage);
  RUN_TEST(test_notice_above_add_a_gif);
  RUN_TEST(test_image_off_names_running_order);
  RUN_TEST(test_one_h1_one_main);
  RUN_TEST(test_page_fits_24k);
  RUN_TEST(test_json_list);
  RUN_TEST(test_gate_refusals_before_write);
  RUN_TEST(test_gate_oversize);
  RUN_TEST(test_gate_no_storage_503);
  RUN_TEST(test_only_first_file_part);
  return UNITY_END();
}
