// SPDX-License-Identifier: AGPL-3.0-only
// Stage, the home page: what the panel shows (the live preview, its name,
// its place and the next change), the running-order strip and the coding
// sessions. Built without the server; web.cpp gathers the gallery's state
// and the session table and sends the page.
//
//   pio test -e native -f test_web_stage
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/i18n.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/web_shell.cpp"
#include "../../src/posters.cpp"
#include "../../src/web_stage.cpp"

void setUp() {}
void tearDown() {}

static const Strings &EN = STRINGS[LANG_EN];
static const Strings &FR = STRINGS[LANG_FR];

static bool has(const std::string &s, const std::string &part) {
  return s.find(part) != std::string::npos;
}
static size_t at(const std::string &s, const std::string &part, size_t from = 0) {
  const size_t p = s.find(part, from);
  TEST_ASSERT_TRUE_MESSAGE(p != std::string::npos, part.c_str());
  return p;
}
static int occurrences(const std::string &s, const std::string &part) {
  int n = 0;
  for (size_t p = s.find(part); p != std::string::npos; p = s.find(part, p + 1)) n++;
  return n;
}
static std::string between(const std::string &s, const std::string &from, const std::string &to) {
  const size_t a = at(s, from) + from.size();
  return s.substr(a, at(s, to, a) - a);
}

// A firmware string as the page writes it: quotes and apostrophes escaped
static std::string esc(const char *t) { return escapeHtml(String(t)).c_str(); }

static uint8_t idx(const char *name) {
  const int i = screenFind(name);
  TEST_ASSERT_TRUE_MESSAGE(i >= 0, name);
  return (uint8_t)i;
}

static Shell shell(const Strings &t) {
  return {&t, nullptr, "carousel.local", TAB_STAGE, true, "0.0.0-test"};
}

static StageView view(const ScreenList &l, const char *current, uint16_t rotateSec = 20,
                      int32_t nextIn = 14) {
  StageView v = {};
  v.list = l;
  v.current = idx(current);
  v.portrait = false;
  v.rotateSec = rotateSec;
  v.nextIn = nextIn;
  v.cannotRotate = 0;
  v.sessions = nullptr;
  v.sessionCount = 0;
  v.level = 3;
  v.levels = 6;
  v.nightStart = 23;
  v.nightEnd = 7;
  v.nightBrightness = 25;
  v.message = "HELLO * CAROUSEL * ";
  v.notice = STAGE_NO_NOTICE;
  v.noticeName = nullptr;
  return v;
}

// What the page flushes, in one piece
static std::string flushed;
static int flushes = 0;
static void collect(String &s) {
  flushed += s.c_str();
  s = String();
  flushes++;
}

static std::string page(const Strings &t, const StageView &v) {
  flushed.clear();
  flushes = 0;
  String s;
  stagePage(s, shell(t), v, collect);
  collect(s);
  return flushed;
}

static ScreenList only(std::initializer_list<const char *> names) {
  ScreenList l = screenListAll();
  l.on = 0;
  for (const char *n : names) l.on |= 1u << idx(n);
  return l;
}

// --- The stage block -----------------------------------------------------------

static void test_stage_block() {
  const std::string html = page(FR, view(screenListAll(), "SLEEP"));
  const std::string block = between(html, "<section class=stage", "</section>");
  at(block, "<h1 id=act>DODO</h1>");
  at(block, "<img class=led src=\"/preview\" width=64 height=32 alt=\"");
  at(block, "alt=\"Ce que le panneau montre en ce moment\xE2\x80\xAF: DODO\"");
  at(block, "<p class=\"num live\">en sc\xC3\xA8ne \xC2\xB7 \xC3\xA9" "cran 4 sur 23</p>");

  // Whatever the table holds is escaped, in the heading and in the image's name
  Strings crafted = EN;
  crafted.scrClock = "CL<O\"CK&";
  crafted.stgPreviewAlt = "Now: %s 'x'";
  const std::string c = page(crafted, view(screenListAll(), "CLOCK"));
  at(c, "<h1 id=act>CL&lt;O&quot;CK&amp;</h1>");
  at(c, "alt=\"Now: CL&lt;O&quot;CK&amp; &#39;x&#39;\"");
  TEST_ASSERT_FALSE(has(c, "CL<O"));
}

static void test_place_counts_on_and_fitting() {
  StagePlace p = stagePlace(screenListAll(), idx("NIMBUS"), false);
  TEST_ASSERT_EQUAL(1, p.n);
  TEST_ASSERT_EQUAL(23, p.count);                      // CAT is drawn for portrait only
  p = stagePlace(screenListAll(), idx("CAT"), true);
  TEST_ASSERT_EQUAL(20, p.count);                      // CODE, SLEEP, RUN and TEXT do not fit
  TEST_ASSERT_EQUAL(12, p.n);                          // NIMBUS..BLACKHOLE first, less those four
  // Off screens do not count, and the list's order is the place
  ScreenList l = only({"CLOCK", "WEATHER", "TEXT"});
  l.order[0] = idx("WEATHER");
  l.order[idx("WEATHER")] = 0;
  p = stagePlace(l, idx("WEATHER"), false);
  TEST_ASSERT_EQUAL(1, p.n);
  TEST_ASSERT_EQUAL(3, p.count);
  p = stagePlace(l, idx("TEXT"), false);
  TEST_ASSERT_EQUAL(2, p.n);
  // A screen not on the list as shown (a request in flight): no place
  p = stagePlace(l, idx("FIRE"), false);
  TEST_ASSERT_EQUAL(0, p.n);
  TEST_ASSERT_EQUAL(3, p.count);
}

static std::string nextLine(const std::string &html) {
  return between(html, "<p class=next>", "</p>");
}

static void test_next_line_variants() {
  // Rotation on: the next name, the seconds and the interval in words
  std::string n = nextLine(page(EN, view(screenListAll(), "CLOCK", 20, 14)));
  TEST_ASSERT_EQUAL_STRING("Next: <b>WEATHER</b><span class=ni> in 14\xC2\xA0s \xC2\xB7 "
                           "a new screen every 20\xC2\xA0s</span>", n.c_str());
  n = nextLine(page(FR, view(screenListAll(), "CLOCK", 300, 299)));
  TEST_ASSERT_EQUAL_STRING("Ensuite\xE2\x80\xAF: <b>METEO</b><span class=ni> dans 299\xC2\xA0s "
                           "\xC2\xB7 un nouvel \xC3\xA9" "cran toutes les 5\xC2\xA0minutes</span>",
                           n.c_str());
  for (int c = 1; c < ROTATE_CHOICE_COUNT; c++) {
    n = nextLine(page(EN, view(screenListAll(), "CLOCK", ROTATE_CHOICES[c], 3)));
    at(n, EN.stgEvery[c - 1]);
  }
  // Off: the knob only, whatever the countdown says
  n = nextLine(page(EN, view(screenListAll(), "CLOCK", 0, -1)));
  TEST_ASSERT_EQUAL_STRING("Changes with the knob only", n.c_str());
  // Nothing else can come on in this orientation
  n = nextLine(page(EN, view(only({"CLOCK", "CAT"}), "CLOCK", 20, -1)));
  TEST_ASSERT_EQUAL_STRING("No other screen to change to in this orientation", n.c_str());
  n = nextLine(page(FR, view(only({"CLOCK"}), "CLOCK", 20, -1)));
  TEST_ASSERT_EQUAL_STRING(FR.stgNoOther, n.c_str());
}

static void test_next_skips_what_cannot_rotate() {
  const ScreenList l = only({"CLOCK", "IMAGE", "WEATHER"});
  const uint32_t image = 1u << idx("IMAGE");
  // List order: CLOCK, WEATHER, ..., IMAGE (the firmware's order)
  TEST_ASSERT_EQUAL(idx("IMAGE"), stageNext(l, idx("WEATHER"), false, 0));
  TEST_ASSERT_EQUAL(idx("CLOCK"), stageNext(l, idx("WEATHER"), false, image));
  TEST_ASSERT_EQUAL(-1, stageNext(only({"CLOCK", "IMAGE"}), idx("CLOCK"), false, image));
  TEST_ASSERT_EQUAL(-1, stageNext(only({"CLOCK", "CAT"}), idx("CLOCK"), false, 0));
  TEST_ASSERT_EQUAL(idx("CAT"), stageNext(only({"CLOCK", "CAT"}), idx("CLOCK"), true, 0));
  // On the page: IMAGE without a playable image is never announced
  StageView v = view(l, "WEATHER");
  v.cannotRotate = image;
  at(nextLine(page(EN, v)), "<b>CLOCK</b>");
  v = view(only({"CLOCK", "IMAGE"}), "CLOCK", 20, -1);
  v.cannotRotate = image;
  TEST_ASSERT_EQUAL_STRING(EN.stgNoOther, nextLine(page(EN, v)).c_str());
}

static void test_still_note_without_scripts() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string html = page(*t, view(screenListAll(), "CLOCK"));
    const std::string block = between(html, "<section class=stage", "</section>");
    const size_t img = at(block, "<img ");
    const size_t note = at(block, std::string("<noscript><small>") +
                                  escapeHtml(t->stgStill).c_str() + "</small></noscript>");
    TEST_ASSERT_TRUE(img < note);
  }
  TEST_ASSERT_EQUAL_STRING("A still from when this page opened. Reload to see it again.",
                           EN.stgStill);
}

// --- The strip -----------------------------------------------------------------

static std::string strip(const std::string &html) {
  return between(html, "<ol class=strip>", "</ol>");
}

static void stripNames(const ScreenList &l, uint8_t current, bool portrait,
                       std::initializer_list<const char *> want) {
  uint8_t out[STAGE_STRIP_MAX];
  const int n = stageStrip(l, current, portrait, out, STAGE_STRIP_MAX);
  TEST_ASSERT_EQUAL((int)want.size(), n);
  int i = 0;
  for (const char *w : want) TEST_ASSERT_EQUAL_STRING(w, screenNameEn(out[i++]));
}

static void test_strip_window_wraps() {
  // Every screen on: seven, from the one before the screen on show
  ScreenList all = screenListAll();
  stripNames(all, idx("CLOCK"), false,
             {"ISS DEMO", "CLOCK", "WEATHER", "DATE", "SYSTEM", "MONEY", "SOCIAL"});
  stripNames(all, idx("SOCIAL"), false,
             {"MONEY", "SOCIAL", "IMAGE", "NIMBUS", "CODE", "DANCE", "SLEEP"});
  // The current one first in the list: the strip starts at the last
  stripNames(all, idx("NIMBUS"), false,
             {"IMAGE", "NIMBUS", "CODE", "DANCE", "SLEEP", "RUN", "LIFE"});
  // Three on: three, wrapping
  const ScreenList three = only({"CLOCK", "WEATHER", "TEXT"});
  stripNames(three, idx("TEXT"), false, {"WEATHER", "TEXT", "CLOCK"});
  stripNames(three, idx("CLOCK"), false, {"TEXT", "CLOCK", "WEATHER"});
  // One: itself
  stripNames(only({"CLOCK"}), idx("CLOCK"), false, {"CLOCK"});
  // A max under seven is honoured
  uint8_t out[3];
  TEST_ASSERT_EQUAL(3, stageStrip(all, idx("CLOCK"), false, out, 3));
  // On the page: seven items, in that order
  const std::string s = strip(page(EN, view(all, "NIMBUS")));
  TEST_ASSERT_EQUAL(7, occurrences(s, "<li"));
  TEST_ASSERT_TRUE(at(s, "<b>IMAGE") < at(s, "<b>NIMBUS") && at(s, "<b>NIMBUS") < at(s, "<b>LIFE"));
  at(s, "<span class=num>23</span><b>IMAGE");
  at(s, "<span class=num>1</span><b>NIMBUS");
}

static void test_strip_marks_the_one_on_show() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string html = page(*t, view(screenListAll(), "CLOCK"));
    const std::string s = strip(html);
    TEST_ASSERT_EQUAL(1, occurrences(s, "aria-current"));
    // Outlined by the stylesheet's .strip [aria-current] .th, coloured, and
    // told in words to a screen reader
    char poster[64];
    snprintf(poster, sizeof(poster), "<li aria-current=true><i class=th style=\"--p:%d\"",
             idx("CLOCK"));
    at(s, poster);
    at(s, std::string("<b>") + t->scrClock + "<span class=vh>" + t->stgNowSr + "</span></b>");
    TEST_ASSERT_EQUAL(1, occurrences(s, "class=vh"));
    // Every item has its poster, its place and its name
    TEST_ASSERT_EQUAL(7, occurrences(s, "<i class=th"));
    TEST_ASSERT_EQUAL(7, occurrences(s, "<span class=num>"));
    // The link to Running order, with every screen counted
    char all[96];
    snprintf(all, sizeof(all), t->stgAllScreens, (unsigned)SCREEN_COUNT);
    at(html, std::string("<a class=go href=\"/screens\">") + all + "</a>");
  }
  TEST_ASSERT_EQUAL_STRING(", on stage now", EN.stgNowSr);
}

static void test_strip_portrait() {
  StageView v = view(screenListAll(), "CAT");
  v.portrait = true;
  const std::string html = page(EN, v);
  const std::string s = strip(html);
  TEST_ASSERT_FALSE(has(s, "<b>CODE") || has(s, "<b>TEXT") || has(s, "<b>SLEEP"));
  // BLACKHOLE, CAT, ISS DEMO: CAT stands between them in portrait only
  TEST_ASSERT_TRUE(at(s, "<b>BLACKHOLE") < at(s, "<b>CAT") && at(s, "<b>CAT") < at(s, "<b>ISS DEMO"));
  at(html, "<p class=\"num live\">on stage \xC2\xB7 screen 12 of 20</p>");
  // The preview is drawn upright: 32 wide, 64 tall
  at(html, "<img class=led src=\"/preview\" width=32 height=64 alt=\"");
  stripNames(screenListAll(), idx("CAT"), true,
             {"BLACKHOLE", "CAT", "ISS DEMO", "CLOCK", "WEATHER", "DATE", "SYSTEM"});
}

// --- The sessions --------------------------------------------------------------

static SessionRow row(const char *name, uint8_t r, uint8_t g, uint8_t b, uint8_t state,
                      uint32_t age) {
  SessionRow s = {};
  snprintf(s.name, sizeof(s.name), "%s", name);
  s.tint[0] = r;
  s.tint[1] = g;
  s.tint[2] = b;
  s.state = state;
  s.ageSec = age;
  return s;
}

static std::string sessionsBlock(const std::string &html) {
  return between(html, "<section class=sec aria-labelledby=h-c>", "</section>");
}

static void test_sessions_rows() {
  const SessionRow rows[] = {
    row("a<&\"bcdefghij", 0xdd, 0x93, 0x4a, 0, 4),        // 13 characters
    row("serein-site", 0x7f, 0xb6, 0xd6, 1, 59),
    row("carousel-fir", 1, 2, 3, 2, 60),
    row("x", 0, 0, 0, 3, 3599),
    row("y", 0, 0, 0, 4, 3600),
    row("z", 0, 0, 0, 5, 7300),
  };
  StageView v = view(screenListAll(), "CODE");
  v.sessions = rows;
  v.sessionCount = 6;
  const std::string b = sessionsBlock(page(EN, v));
  TEST_ASSERT_EQUAL(6, occurrences(b, "<div class=st>"));
  at(b, "a&lt;&amp;&quot;bcdefghij");
  TEST_ASSERT_FALSE(has(b, "a<&"));
  at(b, "background:#dd934a");
  at(b, "background:#7fb6d6");
  at(b, "background:#010203");
  TEST_ASSERT_EQUAL(6, occurrences(b, "aria-hidden=true"));
  at(b, "<b>waiting <span class=num>\xC2\xB7 4\xC2\xA0s ago</span></b>");
  at(b, "<b>thinking <span class=num>\xC2\xB7 59\xC2\xA0s ago</span></b>");
  at(b, "<b>using a tool <span class=num>\xC2\xB7 1\xC2\xA0min ago</span></b>");
  at(b, "<b>needs you <span class=num>\xC2\xB7 59\xC2\xA0min ago</span></b>");
  at(b, "<b>error <span class=num>\xC2\xB7 1\xC2\xA0h ago</span></b>");
  at(b, "<b>finished <span class=num>\xC2\xB7 2\xC2\xA0h ago</span></b>");
  // The hint under the rows
  TEST_ASSERT_TRUE(at(b, "<div class=st>") < at(b, std::string("<small>") + EN.stgSessionsHint));
  // The seventh word, and an unknown state never reads past the table
  const SessionRow failed[] = {row("p", 0, 0, 0, 6, 0), row("q", 0, 0, 0, 200, 0)};
  v.sessions = failed;
  v.sessionCount = 2;
  const std::string f = sessionsBlock(page(FR, v));
  at(f, "<b>\xC3\xA9" "chec <span class=num>\xC2\xB7 il y a 0\xC2\xA0s</span></b>");
  TEST_ASSERT_EQUAL(2, occurrences(f, "<div class=st>"));
  static const char *const EN_STATES[] = {"waiting", "thinking", "using a tool", "needs you",
                                          "error", "finished", "failed"};
  for (int i = 0; i < 7; i++) TEST_ASSERT_EQUAL_STRING(EN_STATES[i], EN.stgStates[i]);
}

static void test_sessions_none() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string b = sessionsBlock(page(*t, view(screenListAll(), "CLOCK")));
    TEST_ASSERT_FALSE(has(b, "<div class=st>"));
    at(b, t->webWaitingHint);
    at(b, std::string("<a class=go href=\"/panel#hooks\">") + t->stgSetUpHooks + "</a>");
    TEST_ASSERT_FALSE(has(b, t->stgSessionsHint));
  }
}

static void test_sessions_heading_is_neutral() {
  const SessionRow rows[] = {row("p", 0, 0, 0, 1, 3)};
  StageView v = view(screenListAll(), "CODE");
  v.sessions = rows;
  v.sessionCount = 1;
  for (const Strings *t : {&EN, &FR}) {
    const std::string b = sessionsBlock(page(*t, v));
    at(b, std::string("<h2 id=h-c>") + t->stgSessions + "</h2>");
    TEST_ASSERT_EQUAL(1, occurrences(b, "Claude Code"));
    at(between(b, "<small>", "</small>"), "Claude Code");
  }
  TEST_ASSERT_EQUAL_STRING("Coding sessions", EN.stgSessions);
}

// --- The actions ---------------------------------------------------------------

static std::string section(const std::string &html, const char *id) {
  return between(html, std::string("<section class=sec aria-labelledby=") + id + ">", "</section>");
}

static void test_bring_on_lists_on_and_fitting_next_selected() {
  ScreenList l = only({"CLOCK", "SLEEP", "CAT", "WEATHER", "TEXT"});
  StageView v = view(l, "SLEEP");
  const std::string b = section(page(FR, v), "h-b");
  at(b, std::string("<h2 id=h-b>") + FR.stgBring + "</h2>");
  // The label names the select
  at(b, std::string("<label for=sc>") + FR.stgScreen + "</label><select id=sc name=s>");
  // On and fitting, in the list's order, English values, the page's names
  const std::string sel = between(b, "<select id=sc name=s>", "</select>");
  TEST_ASSERT_EQUAL(4, occurrences(sel, "<option"));
  TEST_ASSERT_FALSE(has(sel, "CAT"));
  const size_t sleep = at(sel, "<option value=\"SLEEP\">DODO</option>");
  const size_t text = at(sel, "value=\"TEXT\"");
  const size_t clock = at(sel, "value=\"CLOCK\"");
  const size_t weather = at(sel, "value=\"WEATHER\"");
  TEST_ASSERT_TRUE(sleep < text && text < clock && clock < weather);
  // The one after SLEEP is preselected, and only that one
  TEST_ASSERT_EQUAL(1, occurrences(sel, " selected"));
  at(sel, "<option value=\"TEXT\" selected>TEXTE</option>");
  // Show is the section's one primary button, after the field
  at(b, std::string("</select></div><button class=p>") + FR.webImgShow + "</button></form>");
  at(b, "<form method=post action=\"/show\" class=row>");
  // Wrapping: after the last, the first
  const std::string w = section(page(EN, view(l, "WEATHER")), "h-b");
  at(w, "<option value=\"SLEEP\" selected>");
  // Portrait: the portrait screens
  StageView p = view(l, "CAT");
  p.portrait = true;
  const std::string ps = between(section(page(EN, p), "h-b"), "<select", "</select>");
  at(ps, "value=\"CAT\"");
  TEST_ASSERT_FALSE(has(ps, "value=\"TEXT\""));
}

static void test_show_outcome() {
  ScreenList l = only({"CLOCK", "SLEEP", "CAT"});
  TEST_ASSERT_EQUAL(SHOW_OK, showOutcome(l, "CLOCK", false));
  TEST_ASSERT_EQUAL(SHOW_OK, showOutcome(l, "sleep", false));
  TEST_ASSERT_EQUAL(SHOW_OFF, showOutcome(l, "WEATHER", false));
  TEST_ASSERT_EQUAL(SHOW_OFF, showOutcome(l, "WEATHER", true));     // off before orientation
  TEST_ASSERT_EQUAL(SHOW_OTHER, showOutcome(l, "CAT", false));
  TEST_ASSERT_EQUAL(SHOW_OTHER, showOutcome(l, "SLEEP", true));
  TEST_ASSERT_EQUAL(SHOW_OK, showOutcome(l, "CLOCK", true));
  TEST_ASSERT_EQUAL(SHOW_OK, showOutcome(l, "CAT", true));
  TEST_ASSERT_EQUAL(SHOW_UNKNOWN, showOutcome(l, "NOPE", false));
  TEST_ASSERT_EQUAL(SHOW_UNKNOWN, showOutcome(l, "", false));
  TEST_ASSERT_EQUAL(SHOW_UNKNOWN, showOutcome(l, "CLOCKCLOCKCLOCKCLOCKCLOCK", false));
}

static void test_show_refusal_notices() {
  StageView v = view(screenListAll(), "CLOCK");
  v.notice = STAGE_SHOW_OFF;
  v.noticeName = "ME<T\"EO";
  const std::string b = section(page(FR, v), "h-b");
  // Inside its section, above the form, as an alert, the name escaped
  const size_t n = at(b, "<div class=er role=alert>Pas affich\xC3\xA9\xE2\x80\xAF: ME&lt;T&quot;EO est");
  TEST_ASSERT_TRUE(n < at(b, "<form"));
  TEST_ASSERT_FALSE(has(b, "ME<T"));
  v.notice = STAGE_SHOW_OTHER;
  v.noticeName = "CAT";
  at(section(page(EN, v), "h-b"), "Not shown: CAT is drawn for the other orientation.");
  v.notice = STAGE_SHOW_UNKNOWN;
  v.noticeName = nullptr;
  at(section(page(EN, v), "h-b"), esc(EN.stgShowUnknown));
  // A Show notice says nothing in Say something, and none shows unasked
  TEST_ASSERT_FALSE(has(section(page(EN, v), "h-m"), "class=er"));
  v.notice = STAGE_NO_NOTICE;
  const std::string none = page(EN, v);
  TEST_ASSERT_FALSE(has(none, "class=er"));
  TEST_ASSERT_FALSE(has(none, "class=ok"));
}

static void test_house_lights_steps_words_and_ends() {
  StageView v = view(screenListAll(), "CLOCK");
  const std::string l = section(page(EN, v), "h-l");
  at(l, std::string("<h2 id=h-l>") + EN.stgLights + "</h2>");
  // With scripts, the form posts in place and answers in the region above it
  at(l, "<div aria-live=polite></div><form method=post action=\"/brightness\" class=lv>");
  // Level 3 of 0..5: four of six lit, said in words too
  const std::string steps = between(l, "<span class=steps role=img aria-label=\"", "</span>");
  at(steps, "Brightness 4 of 6\">");
  TEST_ASSERT_EQUAL(6, occurrences(steps, "<i"));
  TEST_ASSERT_EQUAL(4, occurrences(steps, "<i class=on>"));
  // Each button posts its target, named in words
  at(l, "<button name=b value=\"2\" aria-label=\"Dimmer\">");
  at(l, "<button class=pl name=b value=\"4\" aria-label=\"Brighter\">");
  TEST_ASSERT_FALSE(has(l, "disabled"));
  // The ends
  v.level = 0;
  const std::string low = section(page(EN, v), "h-l");
  at(low, "<button name=b value=\"0\" aria-label=\"Dimmer\" disabled>");
  at(low, "Brightness 1 of 6");
  TEST_ASSERT_EQUAL(1, occurrences(low, "<i class=on>"));
  TEST_ASSERT_EQUAL(1, occurrences(low, "disabled"));
  v.level = 5;
  const std::string high = section(page(FR, v), "h-l");
  at(high, "<button class=pl name=b value=\"5\" aria-label=\"Plus fort\" disabled>");
  at(high, "Luminosit\xC3\xA9 6 sur 6");
  TEST_ASSERT_EQUAL(6, occurrences(high, "<i class=on>"));
  TEST_ASSERT_EQUAL(1, occurrences(high, "disabled"));
}

static void test_night_line_variants() {
  StageView v = view(screenListAll(), "CLOCK");
  const std::string dims = between(section(page(EN, v), "h-l"), "<small>", "</small>");
  at(dims, "Level 4 of 6. At night, from 23:00 to 7:00, the panel dims to 10\xC2\xA0%. ");
  at(dims, std::string("<a href=\"/panel#night\">") + EN.webNightMode + "</a>");
  v.nightBrightness = 0;
  v.nightStart = 22;
  v.nightEnd = 6;
  at(section(page(FR, v), "h-l"),
     "Niveau 4 sur 6. La nuit, de 22:00 \xC3\xA0 6:00, le panneau s&#39;\xC3\xA9teint.");
  v.nightStart = v.nightEnd = 8;   // same hours: no night mode
  v.nightBrightness = 25;
  const std::string none = section(page(EN, v), "h-l");
  at(none, "Level 4 of 6. No night mode. <a href=\"/panel#night\">Night mode</a>");
  TEST_ASSERT_FALSE(has(none, "At night"));
}

static void test_say_something_escaped() {
  StageView v = view(screenListAll(), "CLOCK");
  v.message = "A\"><script>&'";
  const std::string m = section(page(EN, v), "h-m");
  at(m, std::string("<h2 id=h-m>") + EN.stgSay + "</h2>");
  at(m, "<div aria-live=polite></div><form method=post action=\"/text\" class=row>");
  at(m, std::string("<label for=tx>") + EN.webMessage + "</label>");
  at(m, "<input id=tx name=msg maxlength=95 value=\"A&quot;&gt;&lt;script&gt;&amp;&#39;\">");
  TEST_ASSERT_FALSE(has(m, "<script>"));
  at(m, std::string("<button>") + EN.stgSend + "</button></form>");
  at(m, std::string("<small>") + EN.stgSayHint + "</small>");
}

static void test_text_notices() {
  StageView v = view(screenListAll(), "CLOCK");
  v.notice = STAGE_TEXT_OFF;
  const std::string off = section(page(EN, v), "h-m");
  const size_t n = at(off, "<div class=ok>" + esc(EN.stgTextOff) + "</div>");
  TEST_ASSERT_TRUE(n < at(off, "<form"));
  v.notice = STAGE_TEXT_PORTRAIT;
  at(section(page(FR, v), "h-m"), "<div class=ok>" + esc(FR.stgTextPortrait) + "</div>");
  v.notice = STAGE_TEXT_FAILED;
  const std::string failed = section(page(EN, v), "h-m");
  at(failed, "<div class=er role=alert>" + esc(EN.stgTextFailed) + "</div>");
  TEST_ASSERT_FALSE(has(failed, "class=ok"));
  // Off and portrait together: the off notice, which web.cpp picks
  TEST_ASSERT_EQUAL(STAGE_TEXT_OFF, textNotice(false, false));
  TEST_ASSERT_EQUAL(STAGE_TEXT_OFF, textNotice(false, true));
  TEST_ASSERT_EQUAL(STAGE_TEXT_PORTRAIT, textNotice(true, false));
  TEST_ASSERT_EQUAL(STAGE_NO_NOTICE, textNotice(true, true));
  // Not saved never reads as saved, in any language
  for (const Strings *t : {&EN, &FR}) {
    TEST_ASSERT_NULL(strstr(t->stgTextFailed, t == &EN ? "Saved" : "Enregistr\xC3\xA9."));
    TEST_ASSERT_EQUAL(0, strncmp(t->stgTextOff, t == &EN ? "Saved." : "Enregistr\xC3\xA9.", 6));
  }
}

static void test_sections_in_order() {
  const std::string html = page(EN, view(screenListAll(), "CLOCK"));
  const size_t stage = at(html, "<section class=stage");
  const size_t pair = at(html, "<div class=q>", stage);
  const size_t bring = at(html, "aria-labelledby=h-b", pair);
  const size_t lights = at(html, "aria-labelledby=h-l", bring);
  const size_t order = at(html, "aria-labelledby=h-o", lights);
  const size_t pair2 = at(html, "<div class=q>", order);
  const size_t say = at(html, "aria-labelledby=h-m", pair2);
  const size_t sessions = at(html, "aria-labelledby=h-c", say);
  TEST_ASSERT_TRUE(sessions > say);
}

// --- /preview's headers --------------------------------------------------------

static std::string header(const StageHeader *h, int n, const char *name) {
  for (int k = 0; k < n; k++)
    if (!strcmp(h[k].name, name)) return h[k].value;
  return "(none)";
}

// The state of the frame, in English as /api/info names it
static void test_headers_name_the_frame() {
  ScreenList l = only({"CLOCK", "SLEEP", "IMAGE", "CAT"});
  StageView v = view(l, "SLEEP", 20, 14);
  v.level = 2;
  StageHeader h[STAGE_HEADER_MAX];
  int n = stageHeaders(v, h);
  TEST_ASSERT_EQUAL_INT(4, n);
  TEST_ASSERT_EQUAL_STRING("SLEEP", header(h, n, "X-Screen").c_str());
  TEST_ASSERT_EQUAL_STRING("2", header(h, n, "X-Brightness").c_str());
  TEST_ASSERT_EQUAL_STRING("CLOCK", header(h, n, "X-Next").c_str());
  TEST_ASSERT_EQUAL_STRING("14", header(h, n, "X-Next-In").c_str());
  // As the rotation skips it: IMAGE with nothing to play
  v.current = idx("CLOCK");
  v.cannotRotate = 1u << idx("IMAGE");
  n = stageHeaders(v, h);
  TEST_ASSERT_EQUAL_STRING("SLEEP", header(h, n, "X-Next").c_str());
  // A screen the page's list does not hold is still named
  v.current = idx("WEATHER");
  n = stageHeaders(v, h);
  TEST_ASSERT_EQUAL_STRING("WEATHER", header(h, n, "X-Screen").c_str());
}

// Rotation off: no next at all; on with none other or no countdown: the
// countdown alone, as the next line tells them apart
static void test_headers_without_a_next() {
  StageHeader h[STAGE_HEADER_MAX];
  StageView v = view(only({"CLOCK", "SLEEP"}), "CLOCK", 0, -1);
  int n = stageHeaders(v, h);
  TEST_ASSERT_EQUAL_INT(2, n);
  TEST_ASSERT_EQUAL_STRING("(none)", header(h, n, "X-Next-In").c_str());
  v = view(only({"CLOCK"}), "CLOCK", 20, 14);
  n = stageHeaders(v, h);
  TEST_ASSERT_EQUAL_INT(3, n);
  TEST_ASSERT_EQUAL_STRING("(none)", header(h, n, "X-Next").c_str());
  TEST_ASSERT_EQUAL_STRING("14", header(h, n, "X-Next-In").c_str());
  v = view(only({"CLOCK", "SLEEP"}), "CLOCK", 20, -1);
  n = stageHeaders(v, h);
  TEST_ASSERT_EQUAL_STRING("-1", header(h, n, "X-Next-In").c_str());
}

// --- The script ----------------------------------------------------------------

static std::string attr(const std::string &html, const char *name) {
  return between(html, std::string(" ") + name + "=\"", "\"");
}

static void test_script_data_attributes() {
  ScreenList l = only({"CLOCK", "SLEEP", "IMAGE", "CAT"});
  StageView v = view(l, "CLOCK");
  v.cannotRotate = 1u << idx("IMAGE");
  const std::string html = page(FR, v);
  const std::string block = between(html, "<section class=stage", ">");
  // On and fitting, list order: English for /api/info, French for the page
  TEST_ASSERT_EQUAL_STRING("SLEEP,CLOCK,IMAGE", attr(block, "data-o").c_str());
  TEST_ASSERT_EQUAL_STRING("DODO,HORLOGE,IMAGE", attr(block, "data-l").c_str());
  // The next screen comes from /preview's headers, not from the page
  TEST_ASSERT_FALSE(has(block, "data-r="));
  TEST_ASSERT_EQUAL_STRING(FR.stgPlace, attr(block, "data-p").c_str());
  TEST_ASSERT_EQUAL_STRING(FR.stgNext, attr(block, "data-x").c_str());
  TEST_ASSERT_EQUAL_STRING(FR.stgEvery[1], attr(block, "data-e").c_str());   // 20 s
  TEST_ASSERT_EQUAL_STRING(FR.stgKnobOnly, attr(block, "data-k").c_str());
  TEST_ASSERT_EQUAL_STRING(FR.stgNoOther, attr(block, "data-z").c_str());
  TEST_ASSERT_EQUAL_STRING(FR.stgPreviewAlt, attr(block, "data-a").c_str());
  // What a press in place says: sent, or did not go through
  TEST_ASSERT_EQUAL_STRING(esc(FR.stgSent).c_str(), attr(block, "data-s").c_str());
  TEST_ASSERT_EQUAL_STRING(esc(FR.stgFailed).c_str(), attr(block, "data-f").c_str());
  // Rotation off: no interval word to give
  TEST_ASSERT_EQUAL_STRING("", attr(page(FR, view(l, "CLOCK", 0, -1)), "data-e").c_str());
  // Escaped, every one
  Strings crafted = FR;
  crafted.scrSleep = "DO\"<DO";
  crafted.stgKnobOnly = "a\"b<c>&'";
  crafted.stgNext = "\" onclick=\"x %s %lu";
  const std::string c = between(page(crafted, v), "<section class=stage", ">");
  TEST_ASSERT_EQUAL_STRING("DO&quot;&lt;DO,HORLOGE,IMAGE", attr(c, "data-l").c_str());
  TEST_ASSERT_EQUAL_STRING("a&quot;b&lt;c&gt;&amp;&#39;", attr(c, "data-k").c_str());
  TEST_ASSERT_FALSE(has(c, "onclick=\"x"));
}

static void test_script_is_static_and_small() {
  const size_t n = strlen(STAGE_SCRIPT);
  TEST_ASSERT_TRUE_MESSAGE(n > 0 && n <= 1900, std::to_string(n).c_str());
  TEST_ASSERT_NULL(strchr(STAGE_SCRIPT, '%'));
  TEST_ASSERT_NULL(strstr(STAGE_SCRIPT, "</"));
  // It reads the API (signed out: a 401 JSON, never a code on the panel)
  // and never the page itself
  TEST_ASSERT_NOT_NULL(strstr(STAGE_SCRIPT, "'/api/info'"));
  TEST_ASSERT_NOT_NULL(strstr(STAGE_SCRIPT, "fetch('/preview?t='"));
  // The frame and its state come in one answer, once a second: no poll of
  // /api/info beside it (only a refusal reads it)
  TEST_ASSERT_EQUAL_INT(1, occurrences(STAGE_SCRIPT, "'/api/info'"));
  for (const char *part : {"'Screen'", "'Brightness'", "'Next'", "'Next-In'", "createObjectURL",
                           "revokeObjectURL"})
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(STAGE_SCRIPT, part), part);
  TEST_ASSERT_NULL(strstr(STAGE_SCRIPT, "fetch('/'"));
  TEST_ASSERT_NOT_NULL(strstr(STAGE_SCRIPT, "visibilitychange"));
  TEST_ASSERT_NULL(strstr(STAGE_SCRIPT, "aria-live"));
  // No data in it: the page is the same script for every state
  TEST_ASSERT_NULL(strstr(STAGE_SCRIPT, "CLOCK"));
  const std::string html = page(EN, view(screenListAll(), "CLOCK"));
  at(html, std::string("<script>") + STAGE_SCRIPT + "</script></body></html>");
}

// Dimmer, Brighter and Send post in place, as the forms would (the same
// route and body, the redirect not followed), and the poll moves the steps
static void test_script_posts_in_place() {
  for (const char *part : {"onsubmit", "preventDefault()", "redirect:'manual'", "F.action",
                           "new FormData(F)", "'.lv'", ".brightness", "DOMParser"})
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(STAGE_SCRIPT, part), part);
  // Bring on stays a plain post
  TEST_ASSERT_NULL(strstr(STAGE_SCRIPT, "/show"));
}

// The script moves the level by the first number of the steps' words and of
// the night line: in every language, the level comes first
static void test_level_is_the_first_number() {
  for (int g = 0; g < LANG_COUNT; g++) {
    const Strings &t = STRINGS[g];
    for (const char *f : {t.stgSteps, t.stgNightDims, t.stgNightDark, t.stgNightNone}) {
      const char *first = f + strcspn(f, "%0123456789");
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, strncmp(first, "%d", 2), f);
    }
  }
}

static void test_one_h1_one_main() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string html = page(*t, view(screenListAll(), "CLOCK"));
    TEST_ASSERT_EQUAL(1, occurrences(html, "<h1"));
    TEST_ASSERT_EQUAL(1, occurrences(html, "<main"));
    at(html, std::string("<title>") + t->webTabs[0] + " \xC2\xB7 Carousel</title>");
    at(html, "<a href=\"/\" aria-current=page>");
    // Three forms, each to its own route, posting: a reload never sends one
    TEST_ASSERT_EQUAL(3, occurrences(html, "<form"));
    TEST_ASSERT_EQUAL(3, occurrences(html, "<form method=post action="));
    at(html, "action=\"/show\"");
    at(html, "action=\"/brightness\"");
    at(html, "action=\"/text\"");
  }
}

// The worst case: 24 screens on, six sessions with 13-character names that
// all need escaping, French, every piece flushed: under 24 KB as sent
static void test_page_fits_24k() {
  SessionRow rows[6];
  for (int i = 0; i < 6; i++) rows[i] = row("<<<<<<<<<<<<<", 255, 255, 255, 2, 7200);
  StageView v = view(screenListAll(), "CLOCK");
  v.sessions = rows;
  v.sessionCount = 6;
  // With the actions: a message that needs escaping all through, a notice
  const std::string quotes(95, '"');
  v.message = quotes.c_str();
  v.notice = STAGE_SHOW_OTHER;
  v.noticeName = "<<<<<<<<<<<<<<<<<<<<<<<";
  const std::string html = page(FR, v);
  TEST_ASSERT_TRUE(has(html, "action=\"/text\""));
  TEST_ASSERT_TRUE_MESSAGE(html.size() <= 24576, std::to_string(html.size()).c_str());
  TEST_ASSERT_TRUE(flushes >= 2);   // sent in pieces, never one whole block
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_stage_block);
  RUN_TEST(test_place_counts_on_and_fitting);
  RUN_TEST(test_next_line_variants);
  RUN_TEST(test_next_skips_what_cannot_rotate);
  RUN_TEST(test_still_note_without_scripts);
  RUN_TEST(test_strip_window_wraps);
  RUN_TEST(test_strip_marks_the_one_on_show);
  RUN_TEST(test_strip_portrait);
  RUN_TEST(test_sessions_rows);
  RUN_TEST(test_sessions_none);
  RUN_TEST(test_sessions_heading_is_neutral);
  RUN_TEST(test_bring_on_lists_on_and_fitting_next_selected);
  RUN_TEST(test_show_outcome);
  RUN_TEST(test_show_refusal_notices);
  RUN_TEST(test_house_lights_steps_words_and_ends);
  RUN_TEST(test_night_line_variants);
  RUN_TEST(test_say_something_escaped);
  RUN_TEST(test_text_notices);
  RUN_TEST(test_sections_in_order);
  RUN_TEST(test_script_data_attributes);
  RUN_TEST(test_headers_name_the_frame);
  RUN_TEST(test_headers_without_a_next);
  RUN_TEST(test_script_is_static_and_small);
  RUN_TEST(test_script_posts_in_place);
  RUN_TEST(test_level_is_the_first_number);
  RUN_TEST(test_one_h1_one_main);
  RUN_TEST(test_page_fits_24k);
  return UNITY_END();
}
