// SPDX-License-Identifier: AGPL-3.0-only
// The owner's screen list without the hardware: the firmware's order and
// names (stored in NVS, so a field contract), the stored text read back
// whatever it holds, an orientation left empty, and the refusals; the
// settings form's list, the API's answer for a screen and its JSON.
//
//   pio test -e native -f test_screen_list
#include <unity.h>

#include <algorithm>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "../../src/i18n.cpp"
#include "../../src/json_lite.cpp"
#include "../../src/screen_list.cpp"

void setUp() {}
void tearDown() {}

static const char *const NAMES[] = {
  "NIMBUS", "CODE", "DANCE", "SLEEP", "RUN", "LIFE", "BALL", "MATRIX", "PLASMA", "FIRE", "STARS",
  "TEXT", "RAYS", "BARS", "BLACKHOLE", "CAT", "ISS DEMO", "CLOCK", "WEATHER", "DATE", "SYSTEM",
  "MONEY", "SOCIAL", "IMAGE"};

static int idx(const char *name) { return screenFind(name); }

static std::string format(const ScreenList &l) {
  char out[SCREEN_LIST_TEXT_MAX];
  TEST_ASSERT_TRUE(screenListFormat(l, out, sizeof(out)));
  return out;
}

// --- The table ------------------------------------------------------------------

// Stored lists and the start screen name screens in English: a renamed or
// reordered entry would move or lose an owner's choice
static void test_table_keeps_the_firmware_order_and_names() {
  TEST_ASSERT_EQUAL(24, SCREEN_COUNT);
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) TEST_ASSERT_EQUAL_STRING(NAMES[i], screenNameEn(i));
}

static void test_table_orientations() {
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    const std::string n = NAMES[i];
    const Orientation want = n == "CODE" || n == "SLEEP" || n == "RUN" || n == "TEXT" ? LANDSCAPE
                           : n == "CAT" ? PORTRAIT : BOTH;
    TEST_ASSERT_EQUAL_MESSAGE(want, SCREEN_INFO[i].orientation, NAMES[i]);
  }
  TEST_ASSERT_TRUE(screenFits(idx("CODE"), false));
  TEST_ASSERT_FALSE(screenFits(idx("CODE"), true));
  TEST_ASSERT_TRUE(screenFits(idx("CAT"), true));
  TEST_ASSERT_FALSE(screenFits(idx("CAT"), false));
}

static void test_find_any_case() {
  TEST_ASSERT_EQUAL(15, screenFind("cat"));
  TEST_ASSERT_EQUAL(16, screenFind("Iss Demo"));
  TEST_ASSERT_EQUAL(-1, screenFind("CATS"));
  TEST_ASSERT_EQUAL(-1, screenFind(""));
}

// A screen's address: its English name lowercased, spaces as "-"; only
// that exact text finds it back
static void test_slug_from_the_english_name() {
  char slug[SCREEN_SLUG_MAX];
  screenSlug(idx("CLOCK"), slug, sizeof(slug));
  TEST_ASSERT_EQUAL_STRING("clock", slug);
  screenSlug(idx("ISS DEMO"), slug, sizeof(slug));
  TEST_ASSERT_EQUAL_STRING("iss-demo", slug);
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    screenSlug(i, slug, sizeof(slug));
    TEST_ASSERT_EQUAL_MESSAGE(i, screenFindSlug(slug), slug);
  }
  for (const char *other : {"CLOCK", "Clock", "iss demo", "iss_demo", "clock/", "clock?x=1",
                            "", "clocks", "../clock", "horloge"})
    TEST_ASSERT_EQUAL_MESSAGE(-1, screenFindSlug(other), other);
  TEST_ASSERT_EQUAL(-1, screenFindSlug(nullptr));
  // Cut to fit, never past the end
  char tiny[4];
  screenSlug(idx("ISS DEMO"), tiny, sizeof(tiny));
  TEST_ASSERT_EQUAL_STRING("iss", tiny);
}

// --- Defaults and the stored text -------------------------------------------------

static void test_all_is_every_screen_on_in_firmware_order() {
  const ScreenList l = screenListAll();
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    TEST_ASSERT_EQUAL(i, l.order[i]);
    TEST_ASSERT_TRUE(screenOn(l, i));
  }
  TEST_ASSERT_EQUAL_HEX32(0x00FFFFFF, l.on);
}

// A panel with no stored list: the firmware's order, every screen on but
// the four demos that are not ambiances (still listed, to turn on by hand)
static void test_default_leaves_the_demos_off() {
  const ScreenList l = screenListDefault();
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    TEST_ASSERT_EQUAL(i, l.order[i]);
    const bool demo = i == idx("BALL") || i == idx("MATRIX") || i == idx("RAYS") || i == idx("BARS");
    TEST_ASSERT_EQUAL_MESSAGE(!demo, screenOn(l, i), NAMES[i]);
  }
  TEST_ASSERT_TRUE(screenListValid(l));
  TEST_ASSERT_EQUAL_STRING("NIMBUS,CODE,DANCE,SLEEP,RUN,LIFE,-BALL,-MATRIX,PLASMA,FIRE,STARS,TEXT,"
                           "-RAYS,-BARS,BLACKHOLE,CAT,ISS DEMO,CLOCK,WEATHER,DATE,SYSTEM,MONEY,"
                           "SOCIAL,IMAGE",
                           format(l).c_str());
}

// A stored list is the owner's: the demos it has on stay on
static void test_a_stored_list_keeps_its_demos() {
  const ScreenList l = screenListParse(format(screenListAll()).c_str());
  TEST_ASSERT_TRUE(screenOn(l, idx("BALL")));
  TEST_ASSERT_TRUE(screenOn(l, idx("BARS")));
  TEST_ASSERT_TRUE(screenListValid(l));
}

static void test_format_names_in_order_with_off_marked() {
  ScreenList l = screenListAll();
  l.order[1] = idx("CAT");
  l.order[idx("CAT")] = 1;
  l.on &= ~(1u << idx("SLEEP"));
  const std::string s = format(l);
  TEST_ASSERT_EQUAL_STRING("NIMBUS,CAT,DANCE,-SLEEP,RUN,LIFE,BALL,MATRIX,PLASMA,FIRE,STARS,TEXT,RAYS,"
                           "BARS,BLACKHOLE,CODE,ISS DEMO,CLOCK,WEATHER,DATE,SYSTEM,MONEY,SOCIAL,IMAGE",
                           s.c_str());
}

static void test_parse_reads_back_what_format_wrote() {
  ScreenList l = screenListAll();
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) l.order[p] = SCREEN_COUNT - 1 - p;
  l.on = 0x00A5A5A5;
  const ScreenList back = screenListParse(format(l).c_str());
  TEST_ASSERT_TRUE(screenListSameView(l, back));
}

// Every screen off, the longest stored text, fits the read buffer
static void test_longest_text_fits() {
  ScreenList l = screenListAll();
  l.on = 0;
  TEST_ASSERT_TRUE(format(l).size() < SCREEN_LIST_TEXT_MAX);
  char small[20];
  TEST_ASSERT_FALSE(screenListFormat(l, small, sizeof(small)));
  TEST_ASSERT_EQUAL_STRING("", small);
}

// A screen added by a later update: on, after the listed ones, firmware order
static void test_missing_screens_come_on_at_the_end() {
  const ScreenList l = screenListParse("CAT,-CLOCK,NIMBUS");
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[0]);
  TEST_ASSERT_EQUAL(idx("CLOCK"), l.order[1]);
  TEST_ASSERT_EQUAL(idx("NIMBUS"), l.order[2]);
  TEST_ASSERT_EQUAL(idx("CODE"), l.order[3]);
  TEST_ASSERT_EQUAL(idx("IMAGE"), l.order[SCREEN_COUNT - 1]);
  TEST_ASSERT_FALSE(screenOn(l, idx("CLOCK")));
  TEST_ASSERT_TRUE(screenOn(l, idx("IMAGE")));
  TEST_ASSERT_TRUE(screenOn(l, idx("CODE")));
}

static void test_unknown_entries_are_ignored() {
  const ScreenList l = screenListParse("CAT,NOPE,-GONE,CLOCK");
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[0]);
  TEST_ASSERT_EQUAL(idx("CLOCK"), l.order[1]);
  TEST_ASSERT_TRUE(format(l).find("NOPE") == std::string::npos);
  TEST_ASSERT_TRUE(format(l).find("GONE") == std::string::npos);
}

static void test_duplicates_keep_their_first_place() {
  const ScreenList l = screenListParse("CLOCK,CAT,-CLOCK,CAT");
  TEST_ASSERT_EQUAL(idx("CLOCK"), l.order[0]);
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[1]);
  TEST_ASSERT_EQUAL(idx("NIMBUS"), l.order[2]);
  TEST_ASSERT_TRUE(screenOn(l, idx("CLOCK")));
}

// Stored names are matched exactly: they were written by the firmware
static void test_unreadable_entries_are_ignored() {
  const ScreenList l = screenListParse(",,-,cat, CAT,CAT ,\xff\xfe,--CLOCK,CLOCKCLOCKCLOCKCLOCK,DATE");
  TEST_ASSERT_EQUAL(idx("DATE"), l.order[0]);
  TEST_ASSERT_EQUAL(idx("NIMBUS"), l.order[1]);
  TEST_ASSERT_TRUE(screenOn(l, idx("CAT")));
  TEST_ASSERT_TRUE(screenOn(l, idx("CLOCK")));
}

// No name read: every screen comes on, as a screen added by an update does
static void test_empty_text_turns_every_screen_on() {
  TEST_ASSERT_TRUE(screenListSameView(screenListAll(), screenListParse("")));
}

// --- An orientation without any screen on -------------------------------------------

static void test_fill_turns_on_an_empty_portrait() {
  ScreenList l = screenListParse("NIMBUS,CODE");
  l.on = (1u << idx("CODE")) | (1u << idx("SLEEP")) | (1u << idx("RUN")) | (1u << idx("TEXT"));
  TEST_ASSERT_TRUE(screenListFill(l));
  for (uint8_t i = 0; i < SCREEN_COUNT; i++)
    TEST_ASSERT_EQUAL_MESSAGE(true, screenOn(l, i), NAMES[i]);   // landscape-only ones were on already
  TEST_ASSERT_EQUAL(idx("CODE"), l.order[1]);                    // the order is kept
}

static void test_fill_turns_on_an_empty_landscape() {
  ScreenList l = screenListAll();
  l.on = 1u << idx("CAT");
  TEST_ASSERT_TRUE(screenListFill(l));
  TEST_ASSERT_TRUE(screenOn(l, idx("CODE")));
  TEST_ASSERT_TRUE(screenOn(l, idx("CLOCK")));
  TEST_ASSERT_TRUE(screenOn(l, idx("CAT")));
}

static void test_fill_leaves_a_valid_list_alone() {
  ScreenList l = screenListAll();
  l.on = (1u << idx("CODE")) | (1u << idx("CAT"));
  const ScreenList before = l;
  TEST_ASSERT_FALSE(screenListFill(l));
  TEST_ASSERT_TRUE(screenListSameView(before, l));
}

// --- Refusals -----------------------------------------------------------------

static void test_check_variants() {
  ScreenList l = screenListAll();
  TEST_ASSERT_EQUAL(LIST_OK, screenListCheck(l));
  l.on = 0;
  TEST_ASSERT_EQUAL(LIST_EMPTY, screenListCheck(l));
  l.on = (1u << idx("CODE")) | (1u << idx("TEXT"));
  TEST_ASSERT_EQUAL(LIST_NO_PORTRAIT, screenListCheck(l));
  l.on = 1u << idx("CAT");
  TEST_ASSERT_EQUAL(LIST_NO_LANDSCAPE, screenListCheck(l));
  l.on = (1u << idx("CAT")) | (1u << idx("RUN"));
  TEST_ASSERT_EQUAL(LIST_OK, screenListCheck(l));
  l.on = 1u << idx("CLOCK");
  TEST_ASSERT_EQUAL(LIST_OK, screenListCheck(l));
}

// A list that crosses from the web task into the loop indexes a table of
// draw functions: every screen once, no bit past the last screen, and a
// screen on for each orientation
static void test_valid_needs_every_screen_once_and_a_passing_check() {
  TEST_ASSERT_TRUE(screenListValid(screenListAll()));
  ScreenList l = screenListAll();
  l.order[0] = l.order[1] = 1;   // NIMBUS gone, CODE twice
  TEST_ASSERT_FALSE(screenListValid(l));
  l = screenListAll();
  l.order[0] = 40;               // past the table
  l.on = 0;
  TEST_ASSERT_FALSE(screenListValid(l));
  l = screenListAll();
  l.order[SCREEN_COUNT - 1] = SCREEN_COUNT;
  TEST_ASSERT_FALSE(screenListValid(l));
  l = screenListAll();
  l.on |= 1u << SCREEN_COUNT;    // a screen the firmware does not have
  TEST_ASSERT_FALSE(screenListValid(l));
  l = screenListAll();
  l.on = 1u << idx("CAT");       // nothing for landscape
  TEST_ASSERT_FALSE(screenListValid(l));
  l = screenListAll();
  l.order[0] = idx("CAT");
  l.order[idx("CAT")] = 0;       // reordered, still every screen once
  TEST_ASSERT_TRUE(screenListValid(l));
}

static void test_same_view_compares_order_and_on() {
  ScreenList a = screenListAll(), b = screenListAll();
  TEST_ASSERT_TRUE(screenListSameView(a, b));
  b.on &= ~1u;
  TEST_ASSERT_FALSE(screenListSameView(a, b));
  b = a;
  b.order[0] = 1;
  b.order[1] = 0;
  TEST_ASSERT_FALSE(screenListSameView(a, b));
}

static void test_rotate_offered() {
  for (long s : {0L, 10L, 20L, 30L, 60L, 120L, 300L}) TEST_ASSERT_TRUE(rotateOffered(s));
  for (long s : {-10L, 1L, 45L, 299L, 301L, 65536L + 10}) TEST_ASSERT_FALSE(rotateOffered(s));
}

// --- The settings form -----------------------------------------------------------

// Feeds the form the arguments a browser posts, in order
static ScreensForm post(const ScreenList &current, uint16_t rotate,
                        std::initializer_list<std::pair<const char *, const char *>> args) {
  ScreensForm f(current, rotate);
  for (const auto &a : args) f.arg(a.first, a.second);
  return f;
}

static ScreenList offList(std::initializer_list<const char *> off) {
  ScreenList l = screenListAll();
  for (const char *n : off) l.on &= ~(1u << idx(n));
  return l;
}

// The page names every screen by its English name: a page opened before an
// update that inserted a screen still moves and ticks the right ones
static void test_form_order_and_boxes_by_name() {
  const ScreensForm f = post(screenListAll(), 0, {
    {"o", "CAT"}, {"on", "CAT"}, {"o", "CLOCK"}, {"o", "NIMBUS"}, {"on", "NIMBUS"}, {"city", "x"}});
  TEST_ASSERT_TRUE(f.hasList());
  const ScreenList l = f.list();
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[0]);
  TEST_ASSERT_EQUAL(idx("CLOCK"), l.order[1]);
  TEST_ASSERT_EQUAL(idx("NIMBUS"), l.order[2]);
  TEST_ASSERT_TRUE(screenOn(l, idx("CAT")));
  TEST_ASSERT_FALSE(screenOn(l, idx("CLOCK")));   // posted without its box: off
  TEST_ASSERT_TRUE(screenOn(l, idx("NIMBUS")));
  TEST_ASSERT_EQUAL(idx("CODE"), l.order[3]);      // the rest follow, as they were
  TEST_ASSERT_TRUE(screenOn(l, idx("CODE")));
}

static void test_form_unknown_names_ignored() {
  const ScreensForm f = post(screenListAll(), 0, {
    {"o", "NOPE"}, {"on", "NOPE"}, {"o", "cat"}, {"o", "CAT"}, {"on", "CAT"}, {"o", ""},
    {"on", "GONE"}});
  const ScreenList l = f.list();
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[0]);       // "cat" is not a name the page writes
  TEST_ASSERT_EQUAL(idx("NIMBUS"), l.order[1]);
  TEST_ASSERT_EQUAL_HEX32(0, l.on & ~screenListAll().on);
  ScreensForm none = post(screenListAll(), 0, {{"o", "NOPE"}, {"on", "NOPE"}});
  TEST_ASSERT_FALSE(none.hasList());
}

// Screens the post does not name (a page opened before an update) keep their
// state, after the posted ones, in their current list order
static void test_form_missing_screens_keep_their_state() {
  ScreenList current = offList({"DANCE", "CODE"});
  current.order[0] = idx("DANCE");
  current.order[idx("DANCE")] = 0;   // DANCE first, NIMBUS at DANCE's place
  const ScreenList l = post(current, 0, {{"o", "CLOCK"}, {"on", "CLOCK"}, {"o", "CAT"}}).list();
  TEST_ASSERT_EQUAL(idx("CLOCK"), l.order[0]);
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[1]);
  TEST_ASSERT_EQUAL(idx("DANCE"), l.order[2]);
  TEST_ASSERT_EQUAL(idx("CODE"), l.order[3]);
  TEST_ASSERT_EQUAL(idx("NIMBUS"), l.order[4]);
  TEST_ASSERT_FALSE(screenOn(l, idx("DANCE")));
  TEST_ASSERT_FALSE(screenOn(l, idx("CODE")));
  TEST_ASSERT_TRUE(screenOn(l, idx("NIMBUS")));
  TEST_ASSERT_TRUE(screenOn(l, idx("CLOCK")));
  TEST_ASSERT_FALSE(screenOn(l, idx("CAT")));
  uint32_t seen = 0;
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) seen |= 1u << l.order[p];
  TEST_ASSERT_EQUAL_HEX32(screenListAll().on, seen);
}

// A post without any list (a page from before the update, a script): the
// boxes alone change nothing
static void test_form_without_a_list_leaves_it() {
  const ScreenList current = offList({"CODE"});
  const ScreensForm f = post(current, 20, {{"on", "CODE"}, {"msg", "hi"}, {"mv", "CAT:u"}});
  TEST_ASSERT_FALSE(f.hasList());
  TEST_ASSERT_FALSE(f.hasRotate());
  TEST_ASSERT_TRUE(screenListSameView(current, f.list()));
  TEST_ASSERT_EQUAL(20, f.rotateSec());
}

// A refused list is shown back as posted, boxes included
static void test_form_keeps_posted_boxes() {
  std::vector<std::pair<std::string, std::string>> args;
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    args.push_back({"o", NAMES[i]});
    if (SCREEN_INFO[i].orientation == LANDSCAPE) args.push_back({"on", NAMES[i]});
  }
  ScreensForm f(screenListAll(), 0);
  for (const auto &a : args) f.arg(a.first.c_str(), a.second.c_str());
  const ScreenList l = f.list();
  TEST_ASSERT_EQUAL(LIST_NO_PORTRAIT, screenListCheck(l));
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    TEST_ASSERT_EQUAL(i, l.order[i]);
    TEST_ASSERT_EQUAL_MESSAGE(SCREEN_INFO[i].orientation == LANDSCAPE, screenOn(l, i), NAMES[i]);
  }
}

// Without scripts, an arrow is a submit button: the save moves that screen
// one place, and an arrow at an end changes nothing
static void test_form_arrow_moves_one_place() {
  auto withArrow = [](const char *mv) {
    ScreensForm f(screenListAll(), 0);
    for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
      f.arg("o", NAMES[i]);
      f.arg("on", NAMES[i]);
    }
    f.arg("mv", mv);
    return f.list();
  };
  ScreenList l = withArrow("CAT:u");
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[14]);
  TEST_ASSERT_EQUAL(idx("BLACKHOLE"), l.order[15]);
  l = withArrow("CAT:d");
  TEST_ASSERT_EQUAL(idx("ISS DEMO"), l.order[15]);
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[16]);
  TEST_ASSERT_TRUE(screenListSameView(screenListAll(), withArrow("NIMBUS:u")));
  TEST_ASSERT_TRUE(screenListSameView(screenListAll(), withArrow("IMAGE:d")));
  TEST_ASSERT_TRUE(screenOn(withArrow("CAT:u"), idx("CAT")));
}

static void test_form_bad_arrows_ignored() {
  for (const char *mv : {"CAT", "CAT:x", "NOPE:u", "", ":u", "CAT:uu", "cat:u", "CAT:"}) {
    ScreensForm f(screenListAll(), 0);
    for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
      f.arg("o", NAMES[i]);
      f.arg("on", NAMES[i]);
    }
    f.arg("mv", mv);
    TEST_ASSERT_TRUE_MESSAGE(screenListSameView(screenListAll(), f.list()), mv);
  }
}

// A crafted post with 200 entries: the first SCREEN_COUNT + 8 are looked
// at, the rest ignored, and the list stays every screen once
static void test_form_reads_a_bounded_number_of_entries() {
  TEST_ASSERT_EQUAL(32, SCREEN_COUNT + 8);
  ScreensForm f(screenListAll(), 0);
  for (int k = 0; k < 31; k++) f.arg("o", "NOPE");
  f.arg("o", "CAT");                       // the 32nd: read
  f.arg("o", "CLOCK");                     // the 33rd: not read
  for (int k = 0; k < 167; k++) f.arg("o", "DATE");
  for (int k = 0; k < 32; k++) f.arg("on", "NOPE");
  f.arg("on", "CAT");                      // past the bound
  const ScreenList l = f.list();
  TEST_ASSERT_TRUE(f.hasList());
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[0]);
  TEST_ASSERT_EQUAL(idx("NIMBUS"), l.order[1]);
  TEST_ASSERT_FALSE(screenOn(l, idx("CAT")));
  TEST_ASSERT_TRUE(screenOn(l, idx("CLOCK")));   // not posted: kept
  uint32_t seen = 0;
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) seen |= 1u << l.order[p];
  TEST_ASSERT_EQUAL_HEX32(screenListAll().on, seen);
}

static void test_form_interval_offered_only() {
  for (const char *rt : {"45", "-1", "abc", "", "30x", "65566", " 30"}) {
    const ScreensForm f = post(screenListAll(), 20, {{"rt", rt}});
    TEST_ASSERT_FALSE_MESSAGE(f.hasRotate(), rt);
    TEST_ASSERT_EQUAL_MESSAGE(20, f.rotateSec(), rt);
  }
  ScreensForm f = post(screenListAll(), 20, {{"rt", "300"}});
  TEST_ASSERT_TRUE(f.hasRotate());
  TEST_ASSERT_EQUAL(300, f.rotateSec());
  f = post(screenListAll(), 20, {{"rt", "0"}});
  TEST_ASSERT_TRUE(f.hasRotate());
  TEST_ASSERT_EQUAL(0, f.rotateSec());
  TEST_ASSERT_FALSE(f.hasList());
}

static void test_form_repeated_names_first_wins() {
  const ScreenList l = post(screenListAll(), 0, {
    {"o", "CLOCK"}, {"o", "CAT"}, {"o", "CLOCK"}, {"on", "CAT"}, {"on", "CAT"}}).list();
  TEST_ASSERT_EQUAL(idx("CLOCK"), l.order[0]);
  TEST_ASSERT_EQUAL(idx("CAT"), l.order[1]);
  TEST_ASSERT_EQUAL(idx("NIMBUS"), l.order[2]);
  TEST_ASSERT_FALSE(screenOn(l, idx("CLOCK")));
  TEST_ASSERT_TRUE(screenOn(l, idx("CAT")));
}

// --- The API -------------------------------------------------------------------

// 409 before 404: a screen that is off says so even in the other orientation
static void test_request_status() {
  const ScreenList l = offList({"CLOCK", "CAT"});
  TEST_ASSERT_EQUAL(404, screenRequestStatus(l, "NOPE", false));
  TEST_ASSERT_EQUAL(404, screenRequestStatus(l, "", false));
  TEST_ASSERT_EQUAL(200, screenRequestStatus(l, "date", false));
  TEST_ASSERT_EQUAL(200, screenRequestStatus(l, "CODE", false));
  TEST_ASSERT_EQUAL(409, screenRequestStatus(l, "clock", false));
  TEST_ASSERT_EQUAL(409, screenRequestStatus(l, "CLOCK", true));
  TEST_ASSERT_EQUAL(404, screenRequestStatus(l, "CODE", true));
  TEST_ASSERT_EQUAL(409, screenRequestStatus(l, "CAT", false));
  TEST_ASSERT_EQUAL(409, screenRequestStatus(l, "CAT", true));
}

static std::string value(const std::string &obj, const char *key) {
  char out[64];
  TEST_ASSERT_TRUE_MESSAGE(jsonValue(obj.c_str(), key, out, sizeof(out)), key);
  return out;
}

static void test_info_json() {
  ScreenList l = offList({"CODE"});
  l.order[0] = idx("CAT");
  l.order[idx("CAT")] = 0;
  String j;
  screensInfoJson(j, l, 20, 7, false);
  const std::string s = j.c_str();
  TEST_ASSERT_EQUAL(0, (int)s.find(",\"screens\":[{\"name\":\"NIMBUS\",\"available\":true,"
                                   "\"enabled\":true},{\"name\":\"CODE\",\"available\":true,"
                                   "\"enabled\":false},"));
  TEST_ASSERT_TRUE(s.find("{\"name\":\"CAT\",\"available\":false,\"enabled\":true}") !=
                   std::string::npos);
  TEST_ASSERT_TRUE(s.find("{\"name\":\"IMAGE\",\"available\":true,\"enabled\":true}],") !=
                   std::string::npos);
  TEST_ASSERT_TRUE(s.find(",\"order\":[\"CAT\",\"CODE\",\"DANCE\",") != std::string::npos);
  TEST_ASSERT_TRUE(s.find("\"BLACKHOLE\",\"NIMBUS\",\"ISS DEMO\"") != std::string::npos);
  const std::string obj = "{\"device\":\"carousel\"" + s + "}";
  TEST_ASSERT_EQUAL_STRING("20", value(obj, "rotate_every").c_str());
  TEST_ASSERT_EQUAL_STRING("7", value(obj, "rotate_next_in").c_str());
  TEST_ASSERT_EQUAL('}', obj[obj.size() - 1]);

  String off;
  screensInfoJson(off, screenListAll(), 0, -1, true);
  const std::string o = "{" + std::string(off.c_str() + 1) + "}";
  TEST_ASSERT_EQUAL_STRING("0", value(o, "rotate_every").c_str());
  TEST_ASSERT_EQUAL_STRING("null", value(o, "rotate_next_in").c_str());
  TEST_ASSERT_TRUE(o.find("{\"name\":\"CAT\",\"available\":true,\"enabled\":true}") !=
                   std::string::npos);
  TEST_ASSERT_TRUE(o.find("{\"name\":\"CODE\",\"available\":false,\"enabled\":true}") !=
                   std::string::npos);
  TEST_ASSERT_EQUAL(SCREEN_COUNT, (int)std::count(o.begin(), o.end(), '{') - 1);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_table_keeps_the_firmware_order_and_names);
  RUN_TEST(test_table_orientations);
  RUN_TEST(test_find_any_case);
  RUN_TEST(test_slug_from_the_english_name);
  RUN_TEST(test_all_is_every_screen_on_in_firmware_order);
  RUN_TEST(test_default_leaves_the_demos_off);
  RUN_TEST(test_a_stored_list_keeps_its_demos);
  RUN_TEST(test_format_names_in_order_with_off_marked);
  RUN_TEST(test_parse_reads_back_what_format_wrote);
  RUN_TEST(test_longest_text_fits);
  RUN_TEST(test_missing_screens_come_on_at_the_end);
  RUN_TEST(test_unknown_entries_are_ignored);
  RUN_TEST(test_duplicates_keep_their_first_place);
  RUN_TEST(test_unreadable_entries_are_ignored);
  RUN_TEST(test_empty_text_turns_every_screen_on);
  RUN_TEST(test_fill_turns_on_an_empty_portrait);
  RUN_TEST(test_fill_turns_on_an_empty_landscape);
  RUN_TEST(test_fill_leaves_a_valid_list_alone);
  RUN_TEST(test_check_variants);
  RUN_TEST(test_valid_needs_every_screen_once_and_a_passing_check);
  RUN_TEST(test_same_view_compares_order_and_on);
  RUN_TEST(test_rotate_offered);
  RUN_TEST(test_form_order_and_boxes_by_name);
  RUN_TEST(test_form_unknown_names_ignored);
  RUN_TEST(test_form_missing_screens_keep_their_state);
  RUN_TEST(test_form_without_a_list_leaves_it);
  RUN_TEST(test_form_keeps_posted_boxes);
  RUN_TEST(test_form_arrow_moves_one_place);
  RUN_TEST(test_form_bad_arrows_ignored);
  RUN_TEST(test_form_reads_a_bounded_number_of_entries);
  RUN_TEST(test_form_interval_offered_only);
  RUN_TEST(test_form_repeated_names_first_wins);
  RUN_TEST(test_request_status);
  RUN_TEST(test_info_json);
  return UNITY_END();
}
