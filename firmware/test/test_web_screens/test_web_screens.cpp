// SPDX-License-Identifier: AGPL-3.0-only
// Running order without the web server: the owner's list as the validated
// board draws it, screens named by their English name in the form, a poster
// per row, each screen's own settings built from its declaration alone,
// user text escaped, secrets never sent, the form carrying only its own
// fields, and the page within its pieces and its 24 KB.
//
//   pio test -e native -f test_web_screens
#include <unity.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include <Preferences.h>

#include "../../src/i18n.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/content_logic.cpp"
#include "../../src/web_shell.cpp"
#include "../../src/web_stage.cpp"
#include "../../src/web_screens.cpp"
#include "../../src/posters.cpp"
#include "../../src/posters_logic.cpp"

// settings.cpp's lock, read by content_logic's choice
bool panelLocked() { return false; }

void setUp() {}
void tearDown() {}

// --- Helpers -------------------------------------------------------------------

static const Strings &EN = STRINGS[LANG_EN];
static const Strings &FR = STRINGS[LANG_FR];

// What flush sent, and the largest piece
static std::string sent;
static size_t largest = 0;
static int flushes = 0;
static void collect(String &s) {
  sent += s.c_str();
  largest = std::max(largest, (size_t)s.length());
  flushes++;
  s = String();
}

static int idx(const char *name) { return screenFind(name); }

// The stored settings are the shown ones; CLOCK on stage, landscape; six
// images, all in turn; both counts known
static OrderView view(const Settings &values) {
  OrderView v = {};
  v.screens.values = &values;
  v.screens.stored = &values;
  v.screens.refusal = LIST_OK;
  v.screens.locateFailed = false;
  v.screens.images = {true, 6, ""};
  v.current = (uint8_t)idx("CLOCK");
  v.portrait = false;
  v.social = {true, true, 1240, 386, 12};
  v.result = SAVE_NONE;
  return v;
}

static std::string render(const Strings &t, const OrderView &v) {
  sent.clear();
  largest = 0;
  flushes = 0;
  const Shell sh = {&t, nullptr, "carousel.local", TAB_NONE, false, "0.0.0-test"};
  String s;
  s.reserve(ORDER_PAGE_RESERVE);
  orderPage(s, sh, v, collect);
  if (s.length()) collect(s);
  return sent;
}

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

// The row of one screen, from its start to the next row's (or the list's end)
static std::string row(const std::string &s, const char *nameShown) {
  const size_t a = at(s, std::string("<li data-n=\"") + nameShown + "\"");
  size_t b = s.find("<li data-n=", a + 1);
  if (b == std::string::npos) b = at(s, "</ol>", a);
  return s.substr(a, b - a);
}

// A screen's own page, by its English name
static std::string renderScreen(const Strings &t, const OrderView &v, const char *nameEn) {
  sent.clear();
  largest = 0;
  flushes = 0;
  const Shell sh = {&t, nullptr, "carousel.local", TAB_ORDER, true, "0.0.0-test"};
  String s;
  s.reserve(ORDER_PAGE_RESERVE);
  screenPage(s, sh, v, (uint8_t)idx(nameEn), collect);
  if (s.length()) collect(s);
  return sent;
}

// The settings on a screen's page: from the column after the side one to
// the live region (or, on a page without a form, the column's end)
static std::string fields(const std::string &page) {
  const size_t a = at(page, "<div class=sp>") + 14;
  size_t b = page.find("<p id=ann", a);
  if (b == std::string::npos) b = at(page, "</div></div></div></main>", a);
  return page.substr(a, b - a);
}

// The settings of one screen, from its page
static std::string block(const OrderView &v, const char *nameEn, const Strings &t = STRINGS[LANG_EN]) {
  return fields(renderScreen(t, v, nameEn));
}

// A row's one-line summary; "" when it has none
static std::string summary(const std::string &s, const char *nameShown) {
  const std::string r = row(s, nameShown);
  const size_t a = r.find("<small class=sm>");
  if (a == std::string::npos) return "";
  return r.substr(a + 16, r.find("</small>", a) - a - 16);
}

// The side column
static std::string aside(const std::string &s) {
  const size_t a = at(s, "<aside>");
  return s.substr(a, at(s, "</aside>", a) + 8 - a);
}

// A style choice as the page draws it: one card per option, the default
// first, the style's poster (posterFor), its radio, its name and its
// description; the chosen one checked
static std::string styleRows(const Strings &t, const char *name, const char *const *names,
                             const char *const *hints, int count, int chosen) {
  const int screen = idx(!strcmp(name, "cs") ? "CODE" : !strcmp(name, "ks") ? "CLOCK" : "WEATHER");
  std::string s = std::string("<fieldset><legend>") + t.webStyle + "</legend><div class=cards>";
  for (int i = 0; i < count; i++) {
    s += std::string("<label class=card><i class=\"th b\" style=\"--p:") +
         std::to_string(posterFor((uint8_t)screen, (uint8_t)i)) +
         "\" aria-hidden=true></i><input type=radio name=" + name + " value=" + std::to_string(i) +
         (i == chosen ? " form=sv checked><span>" : " form=sv><span>") + names[i] + "<small>" +
         hints[i] + "</small></span></label>";
  }
  return s + "</div></fieldset>";
}

static Settings filled() {
  Settings s;
  snprintf(s.message, sizeof(s.message), "BACK AT 2 PM * ");
  snprintf(s.city, sizeof(s.city), "Lyon");
  snprintf(s.place, sizeof(s.place), "Lyon, Auvergne-Rhône-Alpes, France");
  s.salary = 2850.0f;
  s.workStart = 9;
  s.workEnd = 18;
  s.workDays = 21;
  snprintf(s.ytChannel, sizeof(s.ytChannel), "UC4R8DWoMoI7CAwX8_LjQHig");
  snprintf(s.ghUser, sizeof(s.ghUser), "serein-design");
  s.instaFollowers = 1240;
  s.xFollowers = 312;
  return s;
}

// --- The page around the list ------------------------------------------------------

// The side column: the title, the lead, the form with its page name and the
// hidden default Save, the interval and its hint, then Save with the count;
// beside it the list, its count and hint, one row per screen in the list's
// order, and Save again for the phone
static void test_section_order_and_controls() {
  Settings values;
  values.rotateSec = 20;
  values.screens.order[0] = idx("CAT");
  values.screens.order[idx("CAT")] = 0;
  const std::string s = render(EN, view(values));

  const std::string side = aside(s);
  TEST_ASSERT_EQUAL(0, (int)at(side, std::string("<aside><h1>Running order</h1><p class=lead>") +
                                     EN.ordLead + "</p>"
                                     "<form id=sv method=post action=\"/sauver\">"
                                     "<input type=hidden name=pg value=screens>"
                                     "<button hidden>Save</button></form>"
                                     "<label for=rt>Change screen</label>"
                                     "<select id=rt name=rt form=sv>"));
  const size_t opts = at(side, "<option value=0>With the knob only</option>"
                               "<option value=10>Every 10\u00A0s</option>"
                               "<option value=20 selected>Every 20\u00A0s</option>"
                               "<option value=30>Every 30\u00A0s</option>"
                               "<option value=60>Every minute</option>"
                               "<option value=120>Every 2\u00A0minutes</option>"
                               "<option value=300>Every 5\u00A0minutes</option></select>");
  const size_t hint = at(side, std::string("</select><small>") + EN.webRotateHint + "</small>"
                               "<div class=\"sv dk\"><p class=u hidden>Unsaved changes</p><button class=p form=sv>Save</button><button type=button class=dc form=sv hidden>Discard</button>"
                               "<small class=num>20 of 24 on</small></div></aside>");
  TEST_ASSERT_TRUE(opts < hint);
  TEST_ASSERT_EQUAL(1, occurrences(side, " selected>"));
  TEST_ASSERT_TRUE(has(s, std::string("</aside><section class=sec aria-labelledby=h-i>"
                                      "<h2 id=h-i>In order, 20 of 24 on</h2>"
                                      "<small style=\"margin:-4px 0 12px\">") + EN.webOrderHint +
                          "</small><ol class=list data-h=\"Move *, position * of *\" data-l=\"on stage\"><li data-n="));
  TEST_ASSERT_EQUAL(SCREEN_COUNT, occurrences(s, "<li data-n="));

  // Rows in the list's order, numbered from 1
  const size_t cat = at(s, "data-n=\"CAT\""), nimbus = at(s, "data-n=\"NIMBUS\"");
  const size_t code = at(s, "data-n=\"CODE\""), image = at(s, "data-n=\"IMAGE\"");
  TEST_ASSERT_TRUE(cat < code && code < nimbus && nimbus < image);
  TEST_ASSERT_TRUE(has(row(s, "CAT"), "<span class=n aria-hidden=true>1</span>"));
  TEST_ASSERT_TRUE(has(row(s, "NIMBUS"), "<span class=n aria-hidden=true>16</span>"));
  TEST_ASSERT_TRUE(has(row(s, "IMAGE"), "<span class=n aria-hidden=true>24</span>"));

  TEST_ASSERT_TRUE(has(s, "</ol><p id=ann class=vh aria-live=polite></p>"
                          "<div class=\"sv mb\"><p class=u hidden>Unsaved changes</p><button class=p form=sv>Save</button><button type=button class=dc form=sv hidden>Discard</button>"
                          "<small class=num>20 of 24 on</small></div></section></div>"));
  TEST_ASSERT_TRUE(at(s, "data-n=\"IMAGE\"") < at(s, "<p id=ann"));

  // Off by default: the knob only
  const std::string d = render(EN, view(Settings()));
  TEST_ASSERT_TRUE(has(d, "<option value=0 selected>With the knob only</option>"));
}

// The lead, a poster per row (its cell of the sheet, all the same size), and
// the screen on show marked by a dot the CSS draws (class=at) and a hidden
// "on stage" for screen readers, in the row's own markup: nothing else
// differs from a row that is not on show
static void test_lead_thumbnails_and_live_dot() {
  const std::string s = render(EN, view(Settings()));
  TEST_ASSERT_TRUE(has(s, "<p class=lead>What the panel plays, in this order. The knob and Home "
                          "Assistant follow this list.</p>"));
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    char poster[160];
    snprintf(poster, sizeof(poster), "<span class=n aria-hidden=true>%u</span><i class=th "
             "style=\"--p:%u\" aria-hidden=true></i><span class=nm>", (unsigned)i + 1,
             (unsigned)posterFor(i, i == (uint8_t)idx("CODE") ? CODE_AUTO
                                    : i == (uint8_t)idx("CLOCK") ? CLOCK_STONE
                                    : i == (uint8_t)idx("WEATHER") ? WEATHER_GLANCE : 0xFF));
    TEST_ASSERT_TRUE_MESSAGE(has(row(s, screenNameEn(i)), poster), screenNameEn(i));
  }
  TEST_ASSERT_EQUAL(0, occurrences(s, "\"th b\""));
  TEST_ASSERT_EQUAL(0, occurrences(s, "class=now"));
  TEST_ASSERT_EQUAL(0, occurrences(s, "class=\"live num\""));
  TEST_ASSERT_EQUAL(0, occurrences(s, "screen 1 of"));
  TEST_ASSERT_TRUE(has(s, "<ol class=list data-h=\"Move *, position * of *\" data-l=\"on stage\">"));

  // CLOCK on show: its row, and only its row, carries the dot and the text
  TEST_ASSERT_TRUE(has(s, "<li data-n=\"CLOCK\" class=at><div class=sr>"));
  TEST_ASSERT_TRUE(has(row(s, "CLOCK"), "<span class=nm>CLOCK<span class=vh> on stage</span>"
                                        "<small class=sm>"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "class=at>"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<span class=vh> on stage</span>"));
  // Same markup as a row that is not on show, but for the dot and the text
  std::string clock = row(s, "CLOCK");
  clock.erase(clock.find(" class=at"), 9);
  clock.erase(clock.find("<span class=vh> on stage</span>"), 31);
  const Settings defaults;
  OrderView elsewhere = view(defaults);
  elsewhere.current = (uint8_t)idx("CAT");   // portrait only: none on show in landscape
  const std::string plain = row(render(EN, elsewhere), "CLOCK");
  TEST_ASSERT_EQUAL_STRING(clock.c_str(), plain.c_str());

  // The place comes from the stored list, not from a refused post
  Settings stored, posted;
  posted.screens.on = 1u << idx("CLOCK");
  OrderView v = view(posted);
  v.screens.stored = &stored;
  v.screens.refusal = LIST_NO_PORTRAIT;
  TEST_ASSERT_TRUE(has(render(EN, v), "<li data-n=\"CLOCK\" class=at>"));

  // Portrait: CAT on show, among the screens drawn upright
  OrderView c = view(stored);
  c.current = (uint8_t)idx("CAT");
  c.portrait = true;
  const std::string up = render(EN, c);
  TEST_ASSERT_TRUE(has(up, "<li data-n=\"CAT\" class=at>"));
  TEST_ASSERT_EQUAL(1, occurrences(up, "class=at>"));

  // A screen not among those that play (off, or a request in flight): no
  // row marked, no text
  Settings off;
  off.screens.on &= ~(1u << idx("CLOCK"));
  const std::string o = render(EN, view(off));
  TEST_ASSERT_FALSE(has(o, "class=at"));
  TEST_ASSERT_FALSE(has(o, "<span class=vh>"));

  // French
  const std::string f = render(FR, view(Settings()));
  TEST_ASSERT_TRUE(has(f, "data-l=\"en scène\""));
  TEST_ASSERT_TRUE(has(row(f, "HORLOGE"), "<span class=nm>HORLOGE<span class=vh> en scène</span>"));
}

// Save and the count of screens on, in the side column and under the list
static void test_save_bar_count() {
  Settings values;
  values.screens.on &= ~((1u << idx("CLOCK")) | (1u << idx("CODE")) | (1u << idx("CAT")));
  const std::string s = render(EN, view(values));
  TEST_ASSERT_EQUAL(2, occurrences(s, "<button type=button class=dc form=sv hidden>Discard</button>"
                                      "<small class=num>17 of 24 on</small></div>"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=\"sv dk\">"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=\"sv mb\">"));
  TEST_ASSERT_TRUE(has(aside(s), "<div class=\"sv dk\">"));
  TEST_ASSERT_TRUE(at(s, "</ol>") < at(s, "<div class=\"sv mb\">"));
  const std::string f = render(FR, view(values));
  TEST_ASSERT_EQUAL(2, occurrences(f, "<button type=button class=dc form=sv hidden>Ignorer</button>"
                                      "<small class=num>17 sur 24 allumés</small></div>"));
}

// A refused or a failed save shows the posted values, not the stored ones:
// both bars up as sent, and form sv marked dirty, Discard opening /screens
static void test_unsaved_after_a_failed_save() {
  const Settings values;
  OrderView refused = view(values);
  refused.screens.refusal = LIST_NO_PORTRAIT;
  refused.result = SAVE_REFUSED;
  OrderView failed = view(values);
  failed.result = SAVE_FAILED;
  for (const OrderView *v : {&refused, &failed}) {
    const std::string s = render(EN, *v);
    TEST_ASSERT_TRUE(has(s, "<form id=sv data-d=/screens method=post action=\"/sauver\">"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=\"sv dk on\">"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=\"sv mb on\">"));
  }
  for (SaveResult r : {SAVE_NONE, SAVE_DONE, SAVE_LATER}) {
    OrderView v = view(values);
    v.result = r;
    const std::string s = render(EN, v);
    TEST_ASSERT_TRUE(has(s, "<form id=sv method=post"));
    TEST_ASSERT_FALSE(has(s, " on\">"));
  }
}

// One page heading and one main: the shell's, around the page's own
static void test_one_h1_one_main() {
  const std::string s = render(EN, view(filled()));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<h1"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<main"));
  TEST_ASSERT_TRUE(at(s, "<main") < at(s, "<h1>Running order</h1>"));
  TEST_ASSERT_TRUE(has(s, "<title>"));
  TEST_ASSERT_EQUAL(0, (int)at(s, "<!doctype html>"));
  TEST_ASSERT_TRUE(has(s, "</html>"));
}

// A Running order save stores only the list and the screens' own settings:
// no field of the Panel page is in its form
static void test_order_form_carries_no_panel_field() {
  Settings values = filled();
  OrderView v = view(values);
  v.screens.refusal = LIST_EMPTY;
  for (const std::string &s : {render(EN, view(values)), render(FR, v)}) {
    for (const char *f : {"lang", "nde", "nto", "nlv", "auh", "au", "mqh", "mqp", "mqu", "mqw"}) {
      TEST_ASSERT_FALSE_MESSAGE(has(s, std::string("name=") + f + " "), f);
      TEST_ASSERT_FALSE_MESSAGE(has(s, std::string("name=") + f + ">"), f);
      TEST_ASSERT_FALSE_MESSAGE(has(s, std::string("name=\"") + f + "\""), f);
    }
    TEST_ASSERT_EQUAL(1, occurrences(s, "<form"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "name=pg value=screens>"));
  }
}

// A save answers the page its form came from: pg=screens Running order;
// anything else, or none (a script, the Panel page), Panel
static void test_answer_page_by_origin() {
  uint8_t screen = 200;
  TEST_ASSERT_EQUAL(SAVE_TO_ORDER, saveAnswerPage("screens", &screen));
  // Feedback's own page, by its exact name only
  TEST_ASSERT_EQUAL(SAVE_TO_FEEDBACK, saveAnswerPage("panel/feedback", &screen));
  for (const char *other : {"panel/", "panel/Feedback", "panel/feedback/", "panel/feedbackx",
                            "/panel/feedback", "panel/feedback?x=1", "panel/other"})
    TEST_ASSERT_EQUAL_MESSAGE(SAVE_TO_PANEL, saveAnswerPage(other, &screen), other);
  for (const char *other : {"panel", "", "Screens", "screens ", "screensx", "screens/",
                            "screens/nope", "screens/CLOCK", "screens/clock/", "/screens/clock",
                            "screens/clock?x=1", "screens/date", "screens/iss-demo",
                            "https://evil.example/screens/clock"}) {
    TEST_ASSERT_EQUAL_MESSAGE(SAVE_TO_PANEL, saveAnswerPage(other, &screen), other);
  }
  TEST_ASSERT_EQUAL(SAVE_TO_PANEL, saveAnswerPage(nullptr, &screen));
  TEST_ASSERT_EQUAL(200, screen);   // untouched but for a screen's page
  // A screen's page, only of a screen that has one: the page comes back
  // from a whitelist, never from the posted text
  for (const char *n : {"CODE", "TEXT", "CLOCK", "WEATHER", "MONEY", "SOCIAL", "IMAGE"}) {
    char pg[40], slug[SCREEN_SLUG_MAX];
    screenSlug((uint8_t)idx(n), slug, sizeof(slug));
    snprintf(pg, sizeof(pg), "screens/%s", slug);
    TEST_ASSERT_EQUAL_MESSAGE(SAVE_TO_SCREEN, saveAnswerPage(pg, &screen), pg);
    TEST_ASSERT_EQUAL_MESSAGE(idx(n), screen, pg);
  }
}

// GET /screens/<slug>: a screen with settings has its page; any other slug
// (unknown, another case, a screen without settings) is the house 404
static void test_screen_page_route() {
  TEST_ASSERT_EQUAL(idx("CLOCK"), screenPageFind("clock"));
  TEST_ASSERT_EQUAL(idx("WEATHER"), screenPageFind("weather"));
  TEST_ASSERT_EQUAL(idx("IMAGE"), screenPageFind("image"));
  for (const char *other : {"", "nope", "CLOCK", "Clock", "clock/", "date", "iss-demo", "nimbus",
                            "horloge", "clock ", "../panel"})
    TEST_ASSERT_EQUAL_MESSAGE(-1, screenPageFind(other), other);
  TEST_ASSERT_EQUAL(-1, screenPageFind(nullptr));
  int pages = 0;
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    char slug[SCREEN_SLUG_MAX];
    screenSlug(i, slug, sizeof(slug));
    TEST_ASSERT_EQUAL_MESSAGE(screenSettings(i) ? i : -1, screenPageFind(slug), slug);
    pages += screenSettings(i) != nullptr;
  }
  TEST_ASSERT_EQUAL(7, pages);
}

// What the save did, at the top of the side column, under the form: saved,
// saved for the next restart, refused by the memory, refused for the list
static void test_result_banners() {
  const Settings values;
  const struct { SaveResult r; std::string banner; } cases[] = {
    {SAVE_DONE, std::string("<div class=ok>") + EN.webSaved + "</div>"},
    {SAVE_LATER, std::string("<div class=ok>") + EN.webSavedLater + "</div>"},
    {SAVE_FAILED, std::string("<div class=er role=alert>") + EN.webNotSaved + "</div>"},
  };
  for (const auto &c : cases) {
    OrderView v = view(values);
    v.result = c.r;
    const std::string s = aside(render(EN, v));
    TEST_ASSERT_TRUE_MESSAGE(has(s, "</form>" + c.banner + "<label for=rt>"), c.banner.c_str());
    TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=ok>") + occurrences(s, "<div class=er"));
    TEST_ASSERT_FALSE(has(s, "autofocus"));
  }
  OrderView v = view(values);
  v.result = SAVE_FAILED;
  TEST_ASSERT_TRUE(has(render(FR, v), std::string("<div class=er role=alert>") + FR.webNotSaved));
  const std::string none = render(EN, view(values));
  TEST_ASSERT_FALSE(has(none, "<div class=ok>"));
  TEST_ASSERT_FALSE(has(none, "<div class=er"));
}

// --- The list --------------------------------------------------------------------

// The marks come from each screen's declared orientation, nothing else
static void test_marks_from_declared_orientation() {
  const std::string s = render(EN, view(Settings()));
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    const char *n = screenNameEn(i);
    const std::string r = row(s, n);
    const Orientation o = SCREEN_INFO[i].orientation;
    const std::string mark =
        o == LANDSCAPE ? " <small>Landscape only</small>" : o == PORTRAIT ? " <small>Portrait only</small>" : "";
    TEST_ASSERT_TRUE_MESSAGE(has(r, mark + "</span></label>"), n);
    TEST_ASSERT_TRUE_MESSAGE(has(r, std::string("<span class=nm>") + n), n);
  }
  TEST_ASSERT_EQUAL(4, occurrences(s, "<small>Landscape only</small>"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<small>Portrait only</small>"));
  TEST_ASSERT_TRUE(has(row(s, "CAT"), "<span class=nm>CAT <small>Portrait only</small></span>"));
  TEST_ASSERT_TRUE(has(row(s, "TEXT"), "Landscape only"));
}

// The form names screens in English; what a person reads (the row's name,
// the arrows' labels, the announcement's name) is in the page's language.
// The place number is hidden from screen readers: a box is named by its
// screen and its mark.
static void test_rows_identify_screens_by_name() {
  const std::string s = render(EN, view(Settings()));
  const int iss = idx("ISS DEMO");
  char want[700];
  snprintf(want, sizeof(want),
           "<li data-n=\"ISS DEMO\"><div class=sr>"
           "<input type=hidden name=o value=\"ISS DEMO\" form=sv>"
           "<label class=ck for=s%d><input id=s%d type=checkbox name=on value=\"ISS DEMO\" form=sv"
           " checked><span class=n aria-hidden=true>%d</span>"
           "<i class=th style=\"--p:%d\" aria-hidden=true></i>"
           "<span class=nm>ISS DEMO</span></label>"
           "<button class=mv form=sv name=mv value=\"ISS DEMO:u\" aria-label=\"Move ISS DEMO up\">"
           "</button>"
           "<button class=\"mv d\" form=sv name=mv value=\"ISS DEMO:d\" "
           "aria-label=\"Move ISS DEMO down\"></button>"
           "<span class=gp></span>"
           "<button type=button class=dg form=sv aria-label=\"Move ISS DEMO, position %d of 24\">"
           "</button></div></li>",
           iss, iss, iss + 1, iss, iss + 1);
  TEST_ASSERT_EQUAL_STRING(want, row(s, "ISS DEMO").c_str());

  const std::string f = render(FR, view(Settings()));
  const std::string cat = row(f, "CHAT");
  TEST_ASSERT_TRUE(has(cat, "<input type=hidden name=o value=\"CAT\" form=sv>"));
  TEST_ASSERT_TRUE(has(cat, "name=on value=\"CAT\" form=sv checked>"));
  TEST_ASSERT_TRUE(has(cat, "name=mv value=\"CAT:u\" aria-label=\"Monter CHAT\">"));
  TEST_ASSERT_TRUE(has(cat, "name=mv value=\"CAT:d\" aria-label=\"Descendre CHAT\">"));
  TEST_ASSERT_TRUE(has(cat, "<span class=nm>CHAT <small>Portrait seulement</small></span>"));
  TEST_ASSERT_FALSE(has(f, "data-n=\"CAT\""));
  TEST_ASSERT_EQUAL(SCREEN_COUNT, occurrences(f, "<input type=hidden name=o value=\""));
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    TEST_ASSERT_TRUE(has(f, std::string("name=o value=\"") + screenNameEn(i) + "\" form=sv>"));
    TEST_ASSERT_TRUE(has(f, std::string("data-n=\"") + FR.*SCREEN_INFO[i].name + "\""));
  }
}

// Every row carries its drag handle after the arrows, then the way to its
// page when it has one. With scripts the handle takes the arrows' place
// (the CSS): a button named for its screen and its place, in the page's
// language, which a drag or ArrowUp and ArrowDown move. The list carries
// the name's pattern, "*" for the screen, the place and the count, for the
// script to name a handle again once its row moved.
static void test_rows_carry_a_drag_handle() {
  const std::string s = render(EN, view(filled()));
  TEST_ASSERT_EQUAL(SCREEN_COUNT, occurrences(s, "<button type=button class=dg form=sv aria-label=\""));
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) {
    const uint8_t i = filled().screens.order[p];
    const std::string r = row(s, EN.*SCREEN_INFO[i].name);
    char name[96];
    snprintf(name, sizeof(name),
             "<button type=button class=dg form=sv aria-label=\"Move %s, position %u of 24\"></button>",
             EN.*SCREEN_INFO[i].name, (unsigned)p + 1);
    const size_t handle = at(r, name);
    TEST_ASSERT_TRUE(at(r, "class=\"mv d\"") < handle);
    const size_t next = handle + strlen(name);
    TEST_ASSERT_EQUAL(0, r.compare(next, 12, "</div></li>"));
    TEST_ASSERT_TRUE(std::min(r.find("<a class=gt "), r.find("<span class=gp>")) < handle);
  }
  const std::string f = render(FR, view(Settings()));
  TEST_ASSERT_TRUE(has(f, "<ol class=list data-h=\"D\xC3\xA9placer *, position * sur *\" data-l=\"en sc\xC3\xA8ne\">"));
  TEST_ASSERT_TRUE(has(row(f, "HORLOGE"), "<button type=button class=dg form=sv "
                                          "aria-label=\"D\xC3\xA9placer HORLOGE, position "));
}

static void test_arrows_disabled_at_the_ends() {
  Settings values;
  values.screens.order[0] = idx("IMAGE");
  values.screens.order[idx("IMAGE")] = 0;
  const std::string s = render(EN, view(values));
  TEST_ASSERT_EQUAL(2, occurrences(s, " disabled>"));
  const std::string first = row(s, "IMAGE"), last = row(s, "NIMBUS");
  TEST_ASSERT_TRUE(has(first, "aria-label=\"Move IMAGE up\" disabled>"));
  TEST_ASSERT_TRUE(has(first, "aria-label=\"Move IMAGE down\">"));
  TEST_ASSERT_TRUE(has(last, "aria-label=\"Move NIMBUS up\">"));
  TEST_ASSERT_TRUE(has(last, "aria-label=\"Move NIMBUS down\" disabled>"));
}

// An off screen: an empty box (its name and poster greyed by the CSS), and
// the count
static void test_off_rows_unchecked() {
  Settings values;
  values.screens.on &= ~((1u << idx("CLOCK")) | (1u << idx("CODE")));
  const std::string s = render(EN, view(values));
  TEST_ASSERT_TRUE(has(s, "<h2 id=h-i>In order, 18 of 24 on</h2>"));
  TEST_ASSERT_EQUAL(18, occurrences(s, "\" form=sv checked>"));   // the screens' boxes
  TEST_ASSERT_TRUE(has(row(s, "CLOCK"), "name=on value=\"CLOCK\" form=sv><span class=n"));
  TEST_ASSERT_TRUE(has(row(s, "CODE"), "name=on value=\"CODE\" form=sv><span class=n"));
  TEST_ASSERT_TRUE(has(row(s, "DATE"), "name=on value=\"DATE\" form=sv checked>"));
}

// One form for the list and the screens' settings, in the side column:
// every control of the page belongs to it through form=sv, the hidden Save
// is its first submit button (Enter anywhere saves; the first row's up
// arrow is disabled), a Save in the side column and under the list. No
// image form is left on the page.
static void test_default_save_first_and_save_after_list() {
  const std::string s = render(EN, view(filled()));
  TEST_ASSERT_EQUAL((int)at(s, "<button"), (int)at(s, "<button hidden>Save</button>"));
  TEST_ASSERT_EQUAL(3, occurrences(s, ">Save</button>"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<form"));
  const size_t end = at(s, "</form>");
  for (const char *tag : {"<input ", "<select ", "<button "}) {
    for (size_t p = s.find(tag, end); p != std::string::npos; p = s.find(tag, p + 1)) {
      const std::string el = s.substr(p, s.find('>', p) - p);
      TEST_ASSERT_TRUE_MESSAGE(has(el, " form=sv"), el.c_str());
    }
  }
  TEST_ASSERT_FALSE(has(s, "/images/"));
  TEST_ASSERT_FALSE(has(s, "multipart"));
}

// A refused list: "Not saved." and the mark to look for, at the top of the
// side column, and the interval focused so the browser scrolls there
static void test_refusal_banner_variants() {
  const struct { ListCheck c; const char *text; } cases[] = {
    {LIST_EMPTY, EN.webRefusedEmpty}, {LIST_NO_PORTRAIT, EN.webRefusedPortrait},
    {LIST_NO_LANDSCAPE, EN.webRefusedLandscape}};
  const Settings values;
  for (const auto &c : cases) {
    OrderView v = view(values);
    v.screens.refusal = c.c;
    v.result = SAVE_REFUSED;
    const std::string s = render(EN, v);
    TEST_ASSERT_TRUE_MESSAGE(has(aside(s), std::string("<button hidden>Save</button></form>"
                                                       "<div class=er role=alert>") +
                                               c.text + "</div><label for=rt>"), c.text);
    TEST_ASSERT_TRUE(has(s, "<select id=rt name=rt form=sv autofocus>"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "role=alert"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "autofocus"));
  }
  OrderView v = view(values);
  v.screens.refusal = LIST_NO_PORTRAIT;
  v.result = SAVE_REFUSED;
  TEST_ASSERT_TRUE(has(render(FR, v), std::string("<div class=er role=alert>") +
                                          FR.webRefusedPortrait + "</div>"));
  const std::string ok = render(EN, view(Settings()));
  TEST_ASSERT_FALSE(has(ok, "role=alert"));
  TEST_ASSERT_FALSE(has(ok, "autofocus"));
}

// --- Each screen's row: its summary and the way to its page -----------------------

// A screen with settings: a one-line summary of its current choice under
// its name, and a 44 px link to its page after the arrows; no settings
// left in the list
static void test_row_summaries() {
  const Settings full = filled();
  OrderView v = view(full);
  std::string s = render(EN, v);
  TEST_ASSERT_EQUAL_STRING("Auto", summary(s, "CODE").c_str());
  TEST_ASSERT_EQUAL_STRING("Stone · date, seconds", summary(s, "CLOCK").c_str());
  TEST_ASSERT_EQUAL_STRING("Lyon · Glance", summary(s, "WEATHER").c_str());
  TEST_ASSERT_EQUAL_STRING("BACK AT 2 PM * ", summary(s, "TEXT").c_str());
  TEST_ASSERT_EQUAL_STRING("2,850 EUR · 9–18 h", summary(s, "MONEY").c_str());
  TEST_ASSERT_EQUAL_STRING("YouTube, GitHub, Instagram, X", summary(s, "SOCIAL").c_str());
  TEST_ASSERT_EQUAL_STRING("6 images, all in turn", summary(s, "IMAGE").c_str());
  for (const char *none : {"NIMBUS", "DATE", "ISS DEMO", "CAT"})
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", summary(s, none).c_str(), none);
  TEST_ASSERT_FALSE(has(s, "<details"));
  TEST_ASSERT_FALSE(has(s, "class=sd"));

  // MONEY writes the chosen currency's code, in both languages
  Settings swiss = filled();
  swiss.currency = CURRENCY_CHF;
  TEST_ASSERT_EQUAL_STRING("2,850 CHF · 9–18 h", summary(render(EN, view(swiss)), "MONEY").c_str());
  TEST_ASSERT_EQUAL_STRING("2 850 CHF · 9 h–18 h", summary(render(FR, view(swiss)), "ARGENT").c_str());
  swiss.currency = CURRENCY_USD;
  TEST_ASSERT_EQUAL_STRING("2,850 USD · 9–18 h", summary(render(EN, view(swiss)), "MONEY").c_str());
  swiss.currency = CURRENCY_GBP;
  TEST_ASSERT_EQUAL_STRING("2,850 GBP · 9–18 h", summary(render(EN, view(swiss)), "MONEY").c_str());

  // French
  const std::string f = render(FR, v);
  TEST_ASSERT_EQUAL_STRING("Pierre · date, secondes", summary(f, "HORLOGE").c_str());
  TEST_ASSERT_EQUAL_STRING("Lyon · Coup d'œil", summary(f, "METEO").c_str());
  TEST_ASSERT_EQUAL_STRING("2 850 EUR · 9 h–18 h", summary(f, "ARGENT").c_str());
  TEST_ASSERT_EQUAL_STRING("6 images, à tour de rôle", summary(f, "IMAGE").c_str());

  // Other choices: every item, none, another style, the image shown, no
  // storage, empty values (no line, or the style alone)
  Settings o = filled();
  o.clockStyle = CLOCK_DIAL;
  o.clockShow = CLOCK_SHOW_ALL;
  o.weatherStyle = WEATHER_DAY;
  o.codeStyle = CODE_LIST;
  o.city[0] = '\0';
  o.message[0] = '\0';
  o.ytChannel[0] = o.ghUser[0] = '\0';
  o.instaFollowers = 0;
  OrderView w = view(o);
  w.screens.images = {true, 6, "<b>.gif"};
  s = render(EN, w);
  TEST_ASSERT_EQUAL_STRING("Dial · date, seconds, week, outside", summary(s, "CLOCK").c_str());
  TEST_ASSERT_EQUAL_STRING("Day", summary(s, "WEATHER").c_str());
  TEST_ASSERT_EQUAL_STRING("List", summary(s, "CODE").c_str());
  TEST_ASSERT_EQUAL_STRING("", summary(s, "TEXT").c_str());
  TEST_ASSERT_EQUAL_STRING("X", summary(s, "SOCIAL").c_str());
  TEST_ASSERT_EQUAL_STRING("&lt;b&gt;.gif", summary(s, "IMAGE").c_str());
  o.clockShow = 0;
  w = view(o);
  w.screens.images = {false, 0, ""};
  s = render(EN, w);
  TEST_ASSERT_EQUAL_STRING("Dial", summary(s, "CLOCK").c_str());
  TEST_ASSERT_EQUAL_STRING("Not available", summary(s, "IMAGE").c_str());
  TEST_ASSERT_EQUAL_STRING("Indisponible", summary(render(FR, w), "IMAGE").c_str());
}

// A long message: its start, cut on a character, with an ellipsis; typed
// text escaped
static void test_row_summary_of_a_long_message() {
  Settings v;
  snprintf(v.message, sizeof(v.message), "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG * ");
  TEST_ASSERT_EQUAL_STRING("THE QUICK BROWN FOX JUMPS OVER T…", summary(render(EN, view(v)), "TEXT").c_str());
  snprintf(v.message, sizeof(v.message), "<b>\"&'");
  TEST_ASSERT_EQUAL_STRING("&lt;b&gt;&quot;&amp;&#39;", summary(render(EN, view(v)), "TEXT").c_str());
  // A UTF-8 character across the cut is not split (a message set by MQTT)
  memset(v.message, 'A', 31);
  snprintf(v.message + 31, sizeof(v.message) - 31, "ééé");
  TEST_ASSERT_EQUAL_STRING("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA…", summary(render(EN, view(v)), "TEXT").c_str());
  snprintf(v.city, sizeof(v.city), "<i>");
  TEST_ASSERT_EQUAL_STRING("&lt;i&gt; · Glance", summary(render(EN, view(v)), "WEATHER").c_str());
}

// A row's thumbnail is its screen in the chosen style (posterFor); the others
// keep their own cell
static int cellOf(const std::string &r) {
  const size_t a = r.find("--p:");
  return a == std::string::npos ? -1 : atoi(r.c_str() + a + 4);
}
static void test_row_thumbnail_follows_style() {
  for (int i = 0; i < CODE_STYLE_COUNT; i++) {
    Settings v;
    v.codeStyle = (uint8_t)i;
    TEST_ASSERT_EQUAL(posterFor((uint8_t)idx("CODE"), (uint8_t)i),
                      cellOf(row(render(EN, view(v)), "CODE")));
  }
  for (int i = 0; i < CLOCK_STYLE_COUNT; i++) {
    Settings v;
    v.clockStyle = (uint8_t)i;
    TEST_ASSERT_EQUAL(posterFor((uint8_t)idx("CLOCK"), (uint8_t)i),
                      cellOf(row(render(EN, view(v)), "CLOCK")));
  }
  for (int i = 0; i < WEATHER_STYLE_COUNT; i++) {
    Settings v;
    v.weatherStyle = (uint8_t)i;
    TEST_ASSERT_EQUAL(posterFor((uint8_t)idx("WEATHER"), (uint8_t)i),
                      cellOf(row(render(EN, view(v)), "WEATHER")));
  }
  TEST_ASSERT_EQUAL(idx("TEXT"), cellOf(row(render(EN, view(Settings())), "TEXT")));
}

// The link: after the arrows and before the handle, a 44 px gear (.gt), named for
// its screen in the page's language, to the screen's own address; a row
// without settings holds an empty 44 px column (.gp) in its place, so the
// handles line up
static void test_row_links_to_its_page() {
  const std::string s = render(EN, view(Settings()));
  char clock[300];
  snprintf(clock, sizeof(clock),
           "aria-label=\"Move CLOCK down\"></button>"
           "<a class=gt href=\"/screens/clock\" aria-label=\"CLOCK settings\"></a>"
           "<button type=button class=dg form=sv aria-label=\"Move CLOCK, position %d of 24\">"
           "</button></div></li>",
           idx("CLOCK") + 1);
  TEST_ASSERT_TRUE(has(row(s, "CLOCK"), clock));
  TEST_ASSERT_TRUE(has(row(s, "TEXT"), "<a class=gt href=\"/screens/text\""));
  TEST_ASSERT_TRUE(has(row(s, "IMAGE"), "<a class=gt href=\"/screens/image\""));
  TEST_ASSERT_EQUAL(7, occurrences(s, "<a class=gt "));
  for (uint8_t i = 0; i < SCREEN_COUNT; i++)
    TEST_ASSERT_EQUAL_MESSAGE(screenSettings(i) != nullptr, has(row(s, screenNameEn(i)), "class=gt"),
                              screenNameEn(i));
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    const std::string r = row(s, screenNameEn(i));
    TEST_ASSERT_EQUAL_MESSAGE(screenSettings(i) == nullptr, has(r, "<span class=gp></span><button type=button class=dg "),
                              screenNameEn(i));
    TEST_ASSERT_EQUAL_MESSAGE(1, occurrences(r, "class=gt") + occurrences(r, "class=gp"),
                              screenNameEn(i));
  }
  TEST_ASSERT_EQUAL(17, occurrences(s, "<span class=gp></span>"));
  const std::string f = render(FR, view(Settings()));
  TEST_ASSERT_TRUE(has(row(f, "HORLOGE"), "<a class=gt href=\"/screens/clock\" "
                                          "aria-label=\"Réglages de HORLOGE\"></a>"));
  // No setting's field is left on Running order
  for (const char *f2 : {"msg", "msg0", "city", "sal", "hd", "hf", "jm", "ytk", "ytc", "gh", "ig",
                         "xa", "cs", "ks", "kw", "kwh", "ws"})
    TEST_ASSERT_FALSE_MESSAGE(has(s, std::string(" name=") + f2 + " "), f2);
}

// --- A screen's page --------------------------------------------------------------

// The side column: the way back to Running order, the screen's name as the
// page's one h1, the page's one form (its return path pg, the hidden Save),
// then Save; the settings beside it, the live region, Save again for the
// phone; the unsaved bar's script alone
static void test_screen_page_markup() {
  const std::string s = renderScreen(EN, view(filled()), "CLOCK");
  TEST_ASSERT_EQUAL(0, (int)at(s, "<!doctype html><html lang=en>"));
  TEST_ASSERT_TRUE(has(s, "<title>CLOCK"));
  TEST_ASSERT_TRUE(has(s, "<a href=\"/screens\" aria-current=page>Running order</a>"));
  TEST_ASSERT_TRUE(has(s, "<main id=m><div class=w><div class=two><aside>"
                          "<a class=bk href=\"/screens\">Running order</a><h1>CLOCK</h1>"
                          "<form id=sv method=post action=\"/sauver\">"
                          "<input type=hidden name=pg value=screens/clock>"
                          "<button hidden>Save</button></form>"
                          "<div class=\"sv dk\"><p class=u hidden>Unsaved changes</p>"
                          "<button class=p form=sv>Save</button>"
                          "<button type=button class=dc form=sv hidden>Discard</button></div>"
                          "</aside><div class=sp><fieldset><legend>Style</legend>"));
  TEST_ASSERT_TRUE(has(s, "</fieldset><p id=ann class=vh aria-live=polite></p>"
                          "<div class=\"sv mb\"><p class=u hidden>Unsaved changes</p>"
                          "<button class=p form=sv>Save</button>"
                          "<button type=button class=dc form=sv hidden>Discard</button></div>"
                          "</div></div></div></main>"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<h1"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<form"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<main"));
  TEST_ASSERT_EQUAL((int)at(s, "<button"), (int)at(s, "<button hidden>Save</button>"));
  TEST_ASSERT_TRUE(has(s, std::string("<script>") + UNSAVED_SCRIPT + "</script>"));
  TEST_ASSERT_FALSE(has(s, SCREENS_SCRIPT));
  TEST_ASSERT_FALSE(has(s, "name=o "));   // the list is Running order's alone
  TEST_ASSERT_FALSE(has(s, "name=rt "));
  const size_t end = at(s, "</form>");
  for (const char *tag : {"<input ", "<select ", "<button "}) {
    for (size_t p = s.find(tag, end); p != std::string::npos; p = s.find(tag, p + 1)) {
      const std::string el = s.substr(p, s.find('>', p) - p);
      TEST_ASSERT_TRUE_MESSAGE(has(el, " form=sv"), el.c_str());
    }
  }
  // Each screen's own return path, its name as h1, in the page's language
  const std::string w = renderScreen(FR, view(filled()), "WEATHER");
  TEST_ASSERT_TRUE(has(w, "<html lang=fr>"));
  TEST_ASSERT_TRUE(has(w, "<a class=bk href=\"/screens\">Ordre de passage</a><h1>METEO</h1>"));
  TEST_ASSERT_TRUE(has(w, "<input type=hidden name=pg value=screens/weather>"
                          "<button hidden>Enregistrer</button></form>"));
  TEST_ASSERT_TRUE(has(w, ">Ignorer</button>"));
  for (const char *n : {"CODE", "TEXT", "MONEY", "SOCIAL"}) {
    char slug[SCREEN_SLUG_MAX];
    screenSlug((uint8_t)idx(n), slug, sizeof(slug));
    const std::string p = renderScreen(EN, view(filled()), n);
    TEST_ASSERT_TRUE_MESSAGE(has(p, std::string("<h1>") + n + "</h1>"), n);
    TEST_ASSERT_TRUE_MESSAGE(has(p, std::string("name=pg value=screens/") + slug + ">"), n);
    TEST_ASSERT_EQUAL_MESSAGE(1, occurrences(p, "<h1"), n);
  }
}

// IMAGE's page holds nothing to save: its summary and the way to Images,
// with no form, no save bar and no script
static void test_image_page_has_no_form() {
  const std::string s = renderScreen(EN, view(Settings()), "IMAGE");
  TEST_ASSERT_TRUE(has(s, "<aside><a class=bk href=\"/screens\">Running order</a><h1>IMAGE</h1>"
                          "</aside><div class=sp>"));
  TEST_ASSERT_FALSE(has(s, "<form"));
  TEST_ASSERT_FALSE(has(s, "class=\"sv"));
  TEST_ASSERT_FALSE(has(s, "<script"));
  TEST_ASSERT_FALSE(has(s, "<button"));
}

// What the save did, under the form; after a refused or failed one, the
// posted values with the bars up and Discard opening this page again
static void test_screen_page_after_a_save() {
  const Settings values = filled();
  const struct { SaveResult r; ListCheck c; std::string banner; } cases[] = {
    {SAVE_DONE, LIST_OK, std::string("<div class=ok>") + EN.webSaved + "</div>"},
    {SAVE_LATER, LIST_OK, std::string("<div class=ok>") + EN.webSavedLater + "</div>"},
    {SAVE_FAILED, LIST_OK, std::string("<div class=er role=alert>") + EN.webNotSaved + "</div>"},
    {SAVE_REFUSED, LIST_NO_PORTRAIT,
     std::string("<div class=er role=alert>") + EN.webRefusedPortrait + "</div>"},
  };
  for (const auto &c : cases) {
    OrderView v = view(values);
    v.result = c.r;
    v.screens.refusal = c.c;
    const std::string s = renderScreen(EN, v, "CLOCK");
    TEST_ASSERT_TRUE_MESSAGE(has(s, "<button hidden>Save</button></form>" + c.banner +
                                        "<div class=\"sv dk"), c.banner.c_str());
    const bool posted = c.r == SAVE_FAILED || c.r == SAVE_REFUSED;
    TEST_ASSERT_EQUAL(posted, has(s, "<form id=sv data-d=/screens/clock method=post"));
    TEST_ASSERT_EQUAL(posted ? 1 : 0, occurrences(s, "<div class=\"sv dk on\">"));
    TEST_ASSERT_EQUAL(posted ? 1 : 0, occurrences(s, "<div class=\"sv mb on\">"));
  }
  OrderView v = view(values);
  v.result = SAVE_FAILED;
  TEST_ASSERT_TRUE(has(renderScreen(FR, v, "TEXT"), std::string("<div class=er role=alert>") +
                                                        FR.webNotSaved + "</div>"));
  TEST_ASSERT_TRUE(has(renderScreen(FR, v, "TEXT"), "data-d=/screens/text "));
  const std::string none = renderScreen(EN, view(values), "CLOCK");
  TEST_ASSERT_FALSE(has(none, "<div class=ok>"));
  TEST_ASSERT_FALSE(has(none, "role=alert"));
}

// IMAGE's page: which image the panel shows (the saved choice, escaped, or
// all in turn) and the way to Images, where the forms are
static void test_image_block_summary() {
  Settings values;
  OrderView v = view(values);
  TEST_ASSERT_EQUAL_STRING(
      "<small style=\"margin-top:0\">Shows every image in turn. Add, delete or choose the image "
      "on the Images page.</small><a class=go href=\"/images\">Open Images</a>",
      block(v, "IMAGE").c_str());
  v.screens.images = {true, 24, "<b>\"&'.gif"};
  std::string s = renderScreen(EN, v, "IMAGE");
  TEST_ASSERT_TRUE(has(fields(s), "<small style=\"margin-top:0\">Shows "
                                  "&lt;b&gt;&quot;&amp;&#39;.gif. Add, delete or choose "
                                  "the image on the Images page.</small>"));
  TEST_ASSERT_FALSE(has(s, "<b>\""));
  for (const char *form : {"type=file", "/images/show", "/images/delete", "name=sh"})
    TEST_ASSERT_FALSE_MESSAGE(has(s, form), form);
  // No storage area: no image to name, the way to Images all the same
  v.screens.images = {false, 0, ""};
  s = block(v, "IMAGE");
  TEST_ASSERT_TRUE(has(s, std::string("<small style=\"margin-top:0\">") + EN.webImgNaText +
                          "</small><a class=go href=\"/images\">Open Images</a>"));
  TEST_ASSERT_FALSE(has(s, "Shows "));
  v.screens.images = {true, 6, "x.gif"};
  s = block(v, "IMAGE", FR);
  TEST_ASSERT_TRUE(has(s, "Montre x.gif. Ajoutez, supprimez ou choisissez l'image sur la page Images."));
  TEST_ASSERT_TRUE(has(s, ">Ouvrir Images</a>"));
}

// SOCIAL's page opens with the counts the panel fetched and how long ago,
// grouped as the page's language writes numbers, "not yet" while unknown
static void test_social_block_counts() {
  const Settings values = filled();
  OrderView v = view(values);
  std::string b = block(v, "SOCIAL");
  TEST_ASSERT_EQUAL(0, (int)at(b, "<div class=st><span>YouTube</span><b>1,240</b></div>"
                                  "<div class=st><span>GitHub</span><b>386</b></div>"
                                  "<div class=st><span>Last update</span><b>12 min ago</b></div>"
                                  "<h3 class=lh>YouTube</h3>"));
  v.social = {true, true, 12500, 386, 12};
  b = block(v, "SOCIAL", FR);
  TEST_ASSERT_TRUE(has(b, "<span>YouTube</span><b>12 500</b>"));
  TEST_ASSERT_TRUE(has(b, "<span>GitHub</span><b>386</b>"));
  v.social = {false, true, 0, 7, 0};
  b = block(v, "SOCIAL");
  TEST_ASSERT_TRUE(has(b, "<span>YouTube</span><b>not yet</b>"));
  TEST_ASSERT_TRUE(has(b, "<span>GitHub</span><b>7</b>"));
  TEST_ASSERT_TRUE(has(b, "<span>Last update</span><b>0 min ago</b>"));
  v.social = {false, false, 0, 0, 0};
  b = block(v, "SOCIAL", FR);
  TEST_ASSERT_TRUE(has(b, "<span>YouTube</span><b>pas encore</b>"));
  TEST_ASSERT_TRUE(has(b, "<span>GitHub</span><b>pas encore</b>"));
  TEST_ASSERT_TRUE(has(b, "<span>Dernière mise à jour</span><b>pas encore</b>"));
  TEST_ASSERT_FALSE(has(b, "<b>--</b>"));
  v.social = {true, true, 2147483647L, 0, 4000000000u};
  b = block(v, "SOCIAL", FR);
  TEST_ASSERT_TRUE(has(b, "<b>2 147 483 647</b>"));
  TEST_ASSERT_TRUE(has(b, "<span>GitHub</span><b>0</b>"));
  TEST_ASSERT_TRUE(has(b, "<b>il y a 4000000000 min</b>"));
  v.social = {true, true, 999, 1000, 0};
  b = block(v, "SOCIAL");
  TEST_ASSERT_TRUE(has(b, "<span>YouTube</span><b>999</b>"));
  TEST_ASSERT_TRUE(has(b, "<span>GitHub</span><b>1,000</b>"));
  // Only on SOCIAL's page
  TEST_ASSERT_FALSE(has(render(EN, view(values)), "<span>YouTube</span>"));
}

// WEATHER's city field, under its input: the chosen suggestion's id (off
// until one is chosen, so a stale one is never posted), the list the script
// fills and the live line that counts it, its two sentences as data
static std::string citySuggestions(const Strings &t) {
  std::string matches = t.webCityMatches;
  matches.replace(matches.find("%s"), 2, "*");   // the script puts the count there
  return std::string("<input type=hidden id=cid name=cid form=sv disabled>"
                     "<div id=pl role=listbox aria-label=\"") + t.webCity +
         "\" hidden></div><p id=pn class=vh aria-live=polite data-m=\"" +
         matches + "\" data-z=\"" + t.webCityNone + "\"></p>";
}

// Each page holds its screen's fields as Running order held them, with the
// names /sauver reads
static void test_blocks_hold_their_fields() {
  Settings values = filled();
  const OrderView v = view(values);
  TEST_ASSERT_EQUAL_STRING(
      "<label for=msg>Scrolling text</label>"
      "<input id=msg name=msg form=sv value=\"BACK AT 2 PM * \" maxlength=95>"
      "<input type=hidden name=msg0 form=sv value=\"BACK AT 2 PM * \">",
      block(v, "TEXT").c_str());
  TEST_ASSERT_EQUAL_STRING(
      (styleRows(EN, "ws", EN.weatherStyles, EN.weatherStyleHints, WEATHER_STYLE_COUNT,
                 WEATHER_GLANCE) +
       "<label for=city>City</label><input id=city name=city form=sv value=\"Lyon\" "
       "maxlength=47 placeholder=\"Lyon\">" + citySuggestions(EN) +
       "<div class=st><span>Found</span><b>Lyon, Auvergne-Rhône-Alpes, France</b>"
       "</div><small>" + EN.webCityHint + "</small>").c_str(),
      block(v, "WEATHER").c_str());
  TEST_ASSERT_EQUAL_STRING(
      "<label for=sal>Net monthly salary (EUR)</label>"
      "<input id=sal name=sal form=sv value=\"2850.00\" inputmode=decimal>"
      "<div class=r><div><label for=hd>Day starts</label>"
      "<input id=hd name=hd form=sv value=\"9\" inputmode=numeric></div>"
      "<div><label for=hf>Day ends</label>"
      "<input id=hf name=hf form=sv value=\"18\" inputmode=numeric></div>"
      "<div><label for=jm>Days / month</label>"
      "<input id=jm name=jm form=sv value=\"21\" inputmode=numeric></div></div>"
      "<small>The counter goes up live during working hours.</small>",
      block(v, "MONEY").c_str());
  TEST_ASSERT_EQUAL_STRING(
      (std::string("<div class=st><span>YouTube</span><b>1,240</b></div>"
                   "<div class=st><span>GitHub</span><b>386</b></div>"
                   "<div class=st><span>Last update</span><b>12 min ago</b></div>"
                   "<h3 class=lh>YouTube</h3>"
                   "<label for=ytk>API key (Google Cloud, YouTube Data v3)</label>"
                   "<input id=ytk name=ytk form=sv value=\"\" placeholder=\"AIza...\" "
                   "autocomplete=off>"
                   "<label for=ytc>Channel ID</label><input id=ytc name=ytc form=sv "
                   "value=\"UC4R8DWoMoI7CAwX8_LjQHig\" placeholder=\"UCxxxxxxxxxxxxxxxxxxxxxx\">"
                   "<small>") + EN.webYtChannelHint + "</small>"
                   "<h3 class=lh>GitHub</h3><label for=gh>Username</label>"
                   "<input id=gh name=gh form=sv value=\"serein-design\">"
                   "<small>No key needed.</small>"
                   "<h3 class=lh>Manual entry</h3><div class=r><div><label for=ig>Instagram "
                   "followers</label><input id=ig name=ig form=sv value=\"1240\" "
                   "inputmode=numeric></div><div><label for=xa>X followers</label><input id=xa "
                   "name=xa form=sv value=\"312\" inputmode=numeric></div></div><small>" +
                   EN.webManualHint + "</small>").c_str(),
      block(v, "SOCIAL").c_str());
  for (const char *n : {"CODE", "TEXT", "CLOCK", "WEATHER", "MONEY", "SOCIAL", "IMAGE"})
    TEST_ASSERT_FALSE_MESSAGE(has(renderScreen(EN, v, n), "required"), n);

  // The place: not found, being looked up, or no row without a city
  values.place[0] = '\0';
  OrderView w = view(values);
  w.screens.locateFailed = true;
  TEST_ASSERT_TRUE(has(block(w, "WEATHER"), "<div class=st><span>Found</span><b>not found, check the "
                                            "spelling</b></div>"));
  TEST_ASSERT_FALSE(has(block(w, "WEATHER"), "id=fd"));   // nothing to wait for
  w.screens.locateFailed = false;
  // Being looked up: the row has an id and the sentence the script shows if
  // the city is not found; found or failed, neither
  TEST_ASSERT_TRUE(has(block(w, "WEATHER"), "<div class=st><span>Found</span><b id=fd "
                                            "data-f=\"not found, check the spelling\">searching...</b></div>"));
  TEST_ASSERT_TRUE(has(block(w, "WEATHER", STRINGS[LANG_FR]), "data-f=\"introuvable, v\xC3\xA9rifiez l&#39;orthographe\""));
  TEST_ASSERT_FALSE(has(block(view(filled()), "WEATHER"), "id=fd"));
  values.city[0] = '\0';
  TEST_ASSERT_FALSE(has(block(view(values), "WEATHER"), "class=st"));
}

// --- Styles --------------------------------------------------------------------------

// MONEY's salary label names the currency the owner chose, by its code
static void test_money_label_follows_the_currency() {
  struct { uint8_t currency; const char *en, *fr; } cases[] = {
    {CURRENCY_EUR, "Net monthly salary (EUR)", "Salaire net mensuel (EUR)"},
    {CURRENCY_USD, "Net monthly salary (USD)", "Salaire net mensuel (USD)"},
    {CURRENCY_GBP, "Net monthly salary (GBP)", "Salaire net mensuel (GBP)"},
    {CURRENCY_CHF, "Net monthly salary (CHF)", "Salaire net mensuel (CHF)"},
  };
  for (const auto &c : cases) {
    Settings values = filled();
    values.currency = c.currency;
    const OrderView v = view(values);
    TEST_ASSERT_TRUE_MESSAGE(has(block(v, "MONEY"), std::string("<label for=sal>") + c.en + "</label>"),
                             c.en);
    TEST_ASSERT_TRUE_MESSAGE(
        has(block(v, "MONEY", STRINGS[LANG_FR]), std::string("<label for=sal>") + c.fr + "</label>"),
        c.fr);
  }
}

static void test_code_block_offers_its_styles() {
  Settings values;
  std::string b = block(view(values), "CODE");
  TEST_ASSERT_EQUAL_STRING(
      styleRows(EN, "cs", EN.codeStyles, EN.codeStyleHints, CODE_STYLE_COUNT, CODE_AUTO).c_str(),
      b.c_str());
  TEST_ASSERT_EQUAL_STRING("Auto", EN.codeStyles[CODE_AUTO]);
  TEST_ASSERT_EQUAL_STRING("Cloud", EN.codeStyles[CODE_CLOUD]);
  TEST_ASSERT_EQUAL_STRING("List", EN.codeStyles[CODE_LIST]);
  values.codeStyle = CODE_LIST;
  b = block(view(values), "CODE", FR);
  TEST_ASSERT_EQUAL_STRING(
      styleRows(FR, "cs", FR.codeStyles, FR.codeStyleHints, CODE_STYLE_COUNT, CODE_LIST).c_str(),
      b.c_str());
  TEST_ASSERT_EQUAL_STRING("Nuage", FR.codeStyles[CODE_CLOUD]);
  // Each card's poster is its style's cell (posterFor), as the poster route
  // serves the sheet
  for (uint8_t i = 0; i < CODE_STYLE_COUNT; i++) {
    char cell[64];
    snprintf(cell, sizeof(cell), "<i class=\"th b\" style=\"--p:%d\" aria-hidden=true>",
             posterFor((uint8_t)idx("CODE"), i));
    TEST_ASSERT_TRUE(has(b, cell));
  }
}

// CLOCK: its style, then what it shows besides the time, one box per item;
// the hidden kwh tells a save with every box unticked from a form without
// them (Panel's, a script)
static void test_clock_block_offers_its_styles_and_items() {
  Settings values;
  std::string b = block(view(values), "CLOCK");
  TEST_ASSERT_EQUAL_STRING(
      (styleRows(EN, "ks", EN.clockStyles, EN.clockStyleHints, CLOCK_STYLE_COUNT, CLOCK_STONE) +
       "<fieldset><legend>Shows</legend><input type=hidden name=kwh value=1 form=sv>"
       "<label class=op><input type=checkbox name=kw value=0 form=sv checked><span>Date</span>"
       "</label><label class=op><input type=checkbox name=kw value=1 form=sv checked><span>"
       "Seconds line</span></label><label class=op><input type=checkbox name=kw value=2 "
       "form=sv><span>Week number</span></label><label class=op><input type=checkbox name=kw "
       "value=3 form=sv><span>Outside temperature</span></label></fieldset>")
          .c_str(),
      b.c_str());
  values.clockStyle = CLOCK_CYCLE;
  values.clockShow = CLOCK_SHOW_WEEK | CLOCK_SHOW_OUTSIDE;
  b = block(view(values), "CLOCK", FR);
  TEST_ASSERT_TRUE(has(b, styleRows(FR, "ks", FR.clockStyles, FR.clockStyleHints, CLOCK_STYLE_COUNT,
                                    CLOCK_CYCLE)));
  TEST_ASSERT_TRUE(has(b, "<legend>Affiche</legend>"));
  TEST_ASSERT_TRUE(has(b, "value=0 form=sv><span>Date</span>"));
  TEST_ASSERT_TRUE(has(b, "value=1 form=sv><span>Ligne des secondes</span>"));
  TEST_ASSERT_TRUE(has(b, "value=2 form=sv checked><span>Numéro de semaine</span>"));
  TEST_ASSERT_TRUE(has(b, "value=3 form=sv checked><span>Température extérieure</span>"));
  TEST_ASSERT_EQUAL_STRING("Pierre", FR.clockStyles[CLOCK_STONE]);
  TEST_ASSERT_EQUAL_STRING("Cadran", FR.clockStyles[CLOCK_DIAL]);
}

// WEATHER: its style first, then the city as before
static void test_weather_block_offers_its_styles() {
  Settings values = filled();
  std::string b = block(view(values), "WEATHER");
  TEST_ASSERT_EQUAL(0, (int)at(b, styleRows(EN, "ws", EN.weatherStyles, EN.weatherStyleHints,
                                            WEATHER_STYLE_COUNT, WEATHER_GLANCE) +
                                      "<label for=city>"));
  values.weatherStyle = WEATHER_DAY;
  b = block(view(values), "WEATHER", FR);
  TEST_ASSERT_TRUE(has(b, styleRows(FR, "ws", FR.weatherStyles, FR.weatherStyleHints,
                                    WEATHER_STYLE_COUNT, WEATHER_DAY)));
  TEST_ASSERT_EQUAL_STRING("Coup d'œil", FR.weatherStyles[WEATHER_GLANCE]);
  TEST_ASSERT_EQUAL_STRING("Journée", FR.weatherStyles[WEATHER_DAY]);
}

// Each style's controls sit once, on their screen's page, and on no other
static void test_style_controls_only_in_their_block() {
  const Settings full = filled();
  const OrderView v = view(full);
  const struct { const char *screen, *control; int count; } owned[] = {
    {"CODE", "name=cs value=", CODE_STYLE_COUNT}, {"CLOCK", "name=ks value=", CLOCK_STYLE_COUNT},
    {"CLOCK", "name=kw value=", 4}, {"CLOCK", "name=kwh value=", 1},
    {"WEATHER", "name=ws value=", WEATHER_STYLE_COUNT}};
  for (const auto &o : owned) {
    TEST_ASSERT_EQUAL_MESSAGE(o.count, occurrences(renderScreen(EN, v, o.screen), o.control), o.control);
    TEST_ASSERT_EQUAL_MESSAGE(0, occurrences(render(EN, v), o.control), o.control);
    for (const char *other : {"TEXT", "MONEY", "SOCIAL"})
      TEST_ASSERT_EQUAL_MESSAGE(0, occurrences(renderScreen(EN, v, other), o.control), other);
  }
}

// After a refused save, the styles as posted, with the rest of the form
static void test_styles_as_posted() {
  Settings posted, stored;
  posted.codeStyle = CODE_CLOUD;
  posted.clockShow = 0;
  posted.weatherStyle = WEATHER_SCENE;
  OrderView v = view(posted);
  v.screens.stored = &stored;
  v.screens.refusal = LIST_EMPTY;
  TEST_ASSERT_TRUE(has(renderScreen(EN, v, "CODE"), "name=cs value=1 form=sv checked>"));
  TEST_ASSERT_TRUE(has(renderScreen(EN, v, "WEATHER"), "name=ws value=1 form=sv checked>"));
  const std::string c = renderScreen(EN, v, "CLOCK");
  TEST_ASSERT_EQUAL(0, occurrences(c, "type=checkbox name=kw value=0 form=sv checked"));
  TEST_ASSERT_EQUAL(0, occurrences(c, "type=checkbox name=kw value=1 form=sv checked"));
}

// What the save reads, by name as the browser posts it
static Settings postStyles(const Settings &stored,
                           std::initializer_list<std::pair<const char *, const char *>> args) {
  StylesForm f;
  for (const auto &a : args) f.arg(a.first, a.second);
  Settings s = stored;
  f.apply(s);
  return s;
}

static void test_styles_saved_from_the_form() {
  const Settings stored;
  Settings s = postStyles(stored, {{"cs", "2"}, {"ks", "1"}, {"kwh", "1"}, {"kw", "2"},
                                   {"kw", "3"}, {"ws", "2"}, {"o", "CAT"}});
  TEST_ASSERT_EQUAL(CODE_LIST, s.codeStyle);
  TEST_ASSERT_EQUAL(CLOCK_DIAL, s.clockStyle);
  TEST_ASSERT_EQUAL(CLOCK_SHOW_WEEK | CLOCK_SHOW_OUTSIDE, s.clockShow);
  TEST_ASSERT_EQUAL(WEATHER_DAY, s.weatherStyle);
  // Every box unticked: the time alone
  s = postStyles(stored, {{"kwh", "1"}});
  TEST_ASSERT_EQUAL(0, s.clockShow);
  // Each field alone leaves the others as stored
  s = postStyles(stored, {{"ks", "2"}});
  TEST_ASSERT_EQUAL(CLOCK_CYCLE, s.clockStyle);
  TEST_ASSERT_EQUAL(CODE_AUTO, s.codeStyle);
  TEST_ASSERT_EQUAL(CLOCK_SHOW_DEFAULT, s.clockShow);
  TEST_ASSERT_EQUAL(WEATHER_GLANCE, s.weatherStyle);
}

// A form without them (Panel's, a script) keeps what is stored; so does a
// value that is no option; the first of two wins, as a radio posts one
static void test_styles_keep_what_is_stored_without_an_option() {
  Settings stored;
  stored.codeStyle = CODE_CLOUD;
  stored.clockStyle = CLOCK_DIAL;
  stored.clockShow = CLOCK_SHOW_WEEK;
  stored.weatherStyle = WEATHER_SCENE;
  Settings s = postStyles(stored, {{"lang", "1"}, {"msg", "HI"}, {"o", "CLOCK"}});
  TEST_ASSERT_EQUAL(CODE_CLOUD, s.codeStyle);
  TEST_ASSERT_EQUAL(CLOCK_DIAL, s.clockStyle);
  TEST_ASSERT_EQUAL(CLOCK_SHOW_WEEK, s.clockShow);
  TEST_ASSERT_EQUAL(WEATHER_SCENE, s.weatherStyle);
  // A box without the mark (a script) is the boxes all the same
  s = postStyles(stored, {{"kw", "1"}});
  TEST_ASSERT_EQUAL(CLOCK_SHOW_SECONDS, s.clockShow);
  for (const char *bad : {"3", "9", "-1", "", "1x", "01", " 1", "x", "255", "4294967297"}) {
    s = postStyles(stored, {{"cs", bad}, {"ks", bad}, {"ws", bad}});
    TEST_ASSERT_EQUAL_MESSAGE(CODE_CLOUD, s.codeStyle, bad);
    TEST_ASSERT_EQUAL_MESSAGE(CLOCK_DIAL, s.clockStyle, bad);
    TEST_ASSERT_EQUAL_MESSAGE(WEATHER_SCENE, s.weatherStyle, bad);
  }
  s = postStyles(stored, {{"kwh", "1"}, {"kw", "4"}, {"kw", "7"}, {"kw", "-1"}, {"kw", "0"}});
  TEST_ASSERT_EQUAL(CLOCK_SHOW_DATE, s.clockShow);
  s = postStyles(stored, {{"cs", "2"}, {"cs", "0"}});
  TEST_ASSERT_EQUAL(CODE_LIST, s.codeStyle);
}

// Pages come from screen_table.h's column alone: a screen without a
// declaration has none, and every setting sits once, on its screen's page
static void test_blocks_only_from_declarations() {
  const char *const withBlock[] = {"CODE", "TEXT", "CLOCK", "WEATHER", "MONEY", "SOCIAL", "IMAGE"};
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    bool declared = false;
    for (const char *n : withBlock) declared |= !strcmp(n, screenNameEn(i));
    TEST_ASSERT_EQUAL_MESSAGE(declared, screenSettings(i) != nullptr, screenNameEn(i));
  }
  const Settings full = filled();
  const OrderView v = view(full);
  const struct { const char *screen; const char *fields[5]; } owned[] = {
    {"TEXT", {"msg"}}, {"WEATHER", {"city", "cid"}}, {"MONEY", {"sal", "hd", "hf", "jm"}},
    {"SOCIAL", {"ytk", "ytc", "gh", "ig", "xa"}}};
  for (const auto &o : owned) {
    for (const char *f : o.fields) {
      if (!f) break;
      const std::string name = std::string(" name=") + f + " ";
      for (const char *n : withBlock)
        TEST_ASSERT_EQUAL_MESSAGE(!strcmp(n, o.screen), occurrences(renderScreen(EN, v, n), name), f);
    }
  }
  TEST_ASSERT_EQUAL(screenSettings(idx("DATE")), nullptr);
}

// The suggestions are the script's: the page holds the places for them
// (the chosen id, off; the empty list, hidden; the live line with its two
// sentences) and the city input stays a plain field that posts city, so
// without scripts the owner types and saves as before
static void test_city_field_has_its_suggestions_markup() {
  Settings values = filled();
  for (const Strings *t : {&EN, &FR}) {
    const std::string b = block(view(values), "WEATHER", *t);
    TEST_ASSERT_TRUE(has(b, std::string("<input id=city name=city form=sv value=\"Lyon\" "
                                        "maxlength=47 placeholder=\"Lyon\">") +
                                citySuggestions(*t)));
    TEST_ASSERT_EQUAL(1, occurrences(b, "name=cid "));
    TEST_ASSERT_FALSE(has(b, "role=combobox"));   // the script says so, when it runs
  }
  // The city's own field is the only one with suggestions
  for (const char *n : {"CODE", "CLOCK", "TEXT", "MONEY", "SOCIAL"})
    TEST_ASSERT_FALSE_MESSAGE(has(renderScreen(EN, view(values), n), "id=pl "), n);
  TEST_ASSERT_FALSE(has(render(EN, view(values)), "id=pl "));
}

// The script comes after the unsaved bar's, on WEATHER's page alone, with
// no data in it and no "<"; it asks /places once the
// owner has stopped typing, with at least 2 characters
static void test_city_script_is_weathers_alone() {
  const std::string w = renderScreen(EN, view(filled()), "WEATHER");
  TEST_ASSERT_EQUAL(1, occurrences(w, "<script"));
  TEST_ASSERT_TRUE(has(w, std::string("<script>") + UNSAVED_SCRIPT + CITY_SCRIPT + "</script>"));
  TEST_ASSERT_TRUE(at(w, "</footer>") < at(w, "<script>"));
  for (const char *n : {"CODE", "CLOCK", "TEXT", "MONEY", "SOCIAL"})
    TEST_ASSERT_FALSE_MESSAGE(has(renderScreen(EN, view(filled()), n), CITY_SCRIPT), n);
  TEST_ASSERT_FALSE(has(render(EN, view(filled())), CITY_SCRIPT));
  printf("City script: %u bytes\n", (unsigned)strlen(CITY_SCRIPT));
  TEST_ASSERT_TRUE(strlen(CITY_SCRIPT) <= 2400);
  TEST_ASSERT_NULL(strchr(CITY_SCRIPT, '<'));
  for (const char *part : {"/place'", "fd.textContent", "fd.dataset.f", "/places?q=", "encodeURIComponent", "250", "ArrowDown", "ArrowUp", "Escape",
                           "Enter", "role", "option", "aria-activedescendant", "aria-expanded",
                           "cid"})
    TEST_ASSERT_TRUE_MESSAGE(strstr(CITY_SCRIPT, part), part);
}

// The city, the place and the message are typed by someone: escaped in the
// summaries and in the values
static void test_titles_escape_user_text() {
  Settings values;
  snprintf(values.message, sizeof(values.message), "<b>\"&'");
  snprintf(values.city, sizeof(values.city), "<b>\"&'");
  snprintf(values.place, sizeof(values.place), "<i>x</i>");
  snprintf(values.ytChannel, sizeof(values.ytChannel), "\"><script>");
  snprintf(values.ghUser, sizeof(values.ghUser), "'<");
  const OrderView v = view(values);
  const std::string s = render(EN, v);
  const std::string esc = "&lt;b&gt;&quot;&amp;&#39;";
  TEST_ASSERT_EQUAL_STRING(esc.c_str(), summary(s, "TEXT").c_str());
  TEST_ASSERT_EQUAL_STRING((esc + " · Glance").c_str(), summary(s, "WEATHER").c_str());
  TEST_ASSERT_TRUE(has(renderScreen(EN, v, "TEXT"), "name=msg form=sv value=\"" + esc + "\""));
  const std::string w = renderScreen(EN, v, "WEATHER");
  TEST_ASSERT_TRUE(has(w, "name=city form=sv value=\"" + esc + "\""));
  TEST_ASSERT_TRUE(has(w, "<b>&lt;i&gt;x&lt;/i&gt;</b>"));
  const std::string so = renderScreen(EN, v, "SOCIAL");
  TEST_ASSERT_TRUE(has(so, "value=\"&quot;&gt;&lt;script&gt;\""));
  TEST_ASSERT_TRUE(has(so, "value=\"&#39;&lt;\""));
  for (const std::string &p : {s, w, so, renderScreen(EN, v, "TEXT")}) {
    TEST_ASSERT_FALSE(has(p, "<b>\""));
    TEST_ASSERT_FALSE(has(p, "<i>"));
    TEST_ASSERT_EQUAL(1, occurrences(p, "<script>"));   // the page's own script alone
  }
  TEST_ASSERT_TRUE(has(s, std::string("<script>") + UNSAVED_SCRIPT + SCREENS_SCRIPT));
}

// A stored key is never sent back: the field is empty and says one is saved
static void test_secret_never_sent_back() {
  Settings values = filled();
  snprintf(values.ytKey, sizeof(values.ytKey), "AIzaSyD-very-secret-key");
  const std::string s = renderScreen(EN, view(values), "SOCIAL");
  TEST_ASSERT_FALSE(has(s, "very-secret"));
  TEST_ASSERT_TRUE(has(s, "<input id=ytk name=ytk form=sv value=\"\" placeholder=\"&bull;&bull;"
                          "&bull;&bull;&bull;&bull;\" autocomplete=off>"));
  TEST_ASSERT_FALSE(has(s, "AIza..."));
  TEST_ASSERT_FALSE(has(render(EN, view(values)), "very-secret"));
}

// A key typed in a refused save is not stored: the page neither shows it nor
// says one is saved, or the next save would keep none without a word
static void test_secret_marker_from_what_is_stored() {
  Settings stored = filled();
  stored.ytKey[0] = '\0';
  Settings posted = stored;
  snprintf(posted.ytKey, sizeof(posted.ytKey), "AIzaTYPEDNOTSTORED");
  OrderView v = view(posted);
  v.screens.stored = &stored;
  v.screens.refusal = LIST_NO_PORTRAIT;
  v.result = SAVE_FAILED;
  const std::string s = renderScreen(EN, v, "SOCIAL");
  TEST_ASSERT_FALSE(has(s, "TYPEDNOTSTORED"));
  TEST_ASSERT_FALSE(has(s, "&bull;"));
  TEST_ASSERT_TRUE(has(s, "<input id=ytk name=ytk form=sv value=\"\" placeholder=\"AIza...\""));
}

// After a refusal or a failed write, the pages show what was posted: the
// boxes, the order, the interval and the other fields
static void test_values_as_posted() {
  Settings posted = filled();
  snprintf(posted.city, sizeof(posted.city), "Paris");
  posted.rotateSec = 30;
  posted.screens.on = 0;
  for (const char *n : {"CODE", "SLEEP", "RUN", "TEXT"}) posted.screens.on |= 1u << idx(n);
  posted.screens.order[0] = idx("TEXT");
  posted.screens.order[idx("TEXT")] = 0;
  OrderView v = view(posted);
  v.screens.refusal = LIST_NO_PORTRAIT;
  const std::string s = render(EN, v);
  TEST_ASSERT_TRUE(has(s, "<h2 id=h-i>In order, 4 of 24 on</h2>"));
  TEST_ASSERT_TRUE(has(s, "<small class=num>4 of 24 on</small>"));
  TEST_ASSERT_TRUE(has(s, "<option value=30 selected>"));
  TEST_ASSERT_TRUE(at(s, "data-n=\"TEXT\"") < at(s, "data-n=\"NIMBUS\""));
  TEST_ASSERT_TRUE(has(row(s, "TEXT"), "<span class=n aria-hidden=true>1</span>"));
  TEST_ASSERT_TRUE(has(row(s, "CODE"), " form=sv checked>"));
  TEST_ASSERT_FALSE(has(row(s, "CAT"), " checked>"));
  TEST_ASSERT_EQUAL_STRING("Paris · Glance", summary(s, "WEATHER").c_str());
  TEST_ASSERT_TRUE(has(renderScreen(EN, v, "WEATHER"), "name=city form=sv value=\"Paris\""));
}

static void test_french_page() {
  const Settings values = filled();
  OrderView v = view(values);
  const std::string s = render(FR, v);
  TEST_ASSERT_TRUE(has(s, "<html lang=fr>"));
  TEST_ASSERT_TRUE(has(s, "<h1>Ordre de passage</h1>"));
  TEST_ASSERT_TRUE(has(s, std::string("<p class=lead>") + FR.ordLead + "</p>"));
  TEST_ASSERT_TRUE(has(s, "<button hidden>Enregistrer</button></form>"
                          "<label for=rt>Changer d'écran</label>"));
  TEST_ASSERT_TRUE(has(s, "<option value=10>Toutes les 10 s</option>"));
  TEST_ASSERT_TRUE(has(s, "<h2 id=h-i>Dans l'ordre, 20 sur 24 allumés</h2>"));
  TEST_ASSERT_TRUE(has(s, std::string(">") + FR.webOrderHint + "</small>"));
  TEST_ASSERT_TRUE(has(s, "aria-label=\"Monter CHAT\""));
  TEST_ASSERT_TRUE(has(s, "<span class=nm>CODE<small class=sm>Auto</small> "
                          "<small>Paysage seulement</small></span>"));
  TEST_ASSERT_TRUE(has(s, "<small class=sm>Lyon · Coup d'œil</small>"));
  TEST_ASSERT_TRUE(has(renderScreen(FR, v, "TEXT"), "<label for=msg>Texte défilant</label>"));
  TEST_ASSERT_TRUE(has(renderScreen(FR, v, "MONEY"), "<title>"));
  v.screens.images.storage = false;
  TEST_ASSERT_EQUAL_STRING("Indisponible", summary(render(FR, v), "IMAGE").c_str());
}

// The worst page: every field at its longest, every character escaped, a
// 40-character choice, a refusal, French. Every piece fits the one String
// the page reserves, and the whole page, as sent, its 24 KB.
static void test_page_fits_24k() {
  Settings values;
  memset(values.message, '"', sizeof(values.message) - 1);
  memset(values.city, '"', sizeof(values.city) - 1);
  memset(values.place, '"', sizeof(values.place) - 1);
  memset(values.ytChannel, '"', sizeof(values.ytChannel) - 1);
  memset(values.ghUser, '"', sizeof(values.ghUser) - 1);
  values.salary = 999999999.99f;
  values.instaFollowers = values.xFollowers = 4294967295u;
  const std::string choice(IMAGE_NAME_MAX, '"');
  size_t worst = 0, page = 0, screenWorst = 0;
  for (int l = 0; l < LANG_COUNT; l++) {
    for (SaveResult r : {SAVE_NONE, SAVE_DONE, SAVE_LATER, SAVE_FAILED, SAVE_REFUSED}) {
      OrderView v = view(values);
      v.screens.refusal = r == SAVE_REFUSED ? LIST_NO_LANDSCAPE : LIST_OK;
      v.screens.images = {true, MAX_IMAGES, choice.c_str()};
      v.social = {true, true, 2147483647L, 2147483647L, 4000000000u};
      v.result = r;
      const std::string s = render(STRINGS[l], v);
      TEST_ASSERT_TRUE(flushes > 2);
      TEST_ASSERT_TRUE_MESSAGE(largest <= ORDER_PAGE_RESERVE, STRINGS[l].code);
      TEST_ASSERT_TRUE_MESSAGE(s.size() <= 24576, STRINGS[l].code);
      TEST_ASSERT_EQUAL(SCREEN_COUNT, occurrences(s, "<li data-n="));
      TEST_ASSERT_TRUE(s.compare(s.size() - 7, 7, "</html>") == 0);
      worst = std::max(worst, largest);
      page = std::max(page, s.size());
      // Each screen's page, in the same String
      for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
        if (!screenSettings(i)) continue;
        const std::string p = renderScreen(STRINGS[l], v, screenNameEn(i));
        TEST_ASSERT_TRUE_MESSAGE(largest <= ORDER_PAGE_RESERVE, screenNameEn(i));
        TEST_ASSERT_TRUE_MESSAGE(p.size() <= 24576, screenNameEn(i));
        TEST_ASSERT_TRUE(p.compare(p.size() - 7, 7, "</html>") == 0);
        screenWorst = std::max(screenWorst, largest);
      }
    }
  }
  printf("Running order: largest piece %u of %u bytes, page %u of 24576; a screen's page: "
         "largest piece %u\n", (unsigned)worst, (unsigned)ORDER_PAGE_RESERVE, (unsigned)page,
         (unsigned)screenWorst);
}

// The board's script, adjusted to the list's rows: no data in it, so
// nothing to escape; the page sends it once, after the footer
static void test_script_is_static_and_small() {
  const char *board =
      "let P=document.querySelector('ol'),L=[...P.children],H=P.dataset.h.split('*'),"
      "n=()=>{let l=[...P.children];l.forEach((x,i)=>{x.querySelector('.n').textContent=i+1;"
      "x.querySelector('.dg').ariaLabel=H[0]+x.dataset.n+H[1]+(i+1)+H[2]+l.length+H[3]});return l},"
      "a=(r,l)=>c(r.dataset.n+' '+(l.indexOf(r)+1)+'/'+l.length);"
      "sv.onreset=()=>{P.append(...L);n()};"
      "P.onkeydown=e=>{let g=e.target.closest('.dg'),u=e.key=='ArrowUp';"
      "if(!g||!u&&e.key!='ArrowDown')return;e.preventDefault();"
      "let r=g.closest('li'),o=u?r.previousElementSibling:r.nextElementSibling;"
      "if(o)P.insertBefore(r,u?o:o.nextSibling);g.focus();a(r,n())};"
      "let O=P.querySelector('.vh')||Object.assign(document.createElement('span'),{className:'vh',"
      "textContent:' '+P.dataset.l}),Q,"
      "f=()=>fetch('/preview?t='+Date.now()).then(y=>y.blob().then(()=>{let s=y.headers.get('X-Screen'),"
      "r=s&&[...P.children].find(x=>x.querySelector('[name=o]').value==s);"
      "if(s){P.querySelectorAll('.at').forEach(x=>x.classList.remove('at'));O.remove();"
      "r&&(r.classList.add('at'),r.querySelector('.nm').firstChild.after(O))}})).catch(()=>0),"
      "g=()=>{clearInterval(Q);document.hidden||(Q=setInterval(f,2e3))};"
      "document.onvisibilitychange=g;g();"
      "P.onpointerdown=e=>{let g=e.target.closest('.dg');if(!g||e.button)return;e.preventDefault();"
      "let r=g.closest('li'),l=[...P.children],i=l.indexOf(r),h=r.offsetHeight,Y=e.clientY,y=Y+scrollY,t=i,"
      "M=l.map(x=>x.offsetTop+x.offsetHeight/2),f,"
      "m=()=>{let d=Y+scrollY-y;t=l.filter((x,j)=>j!=i&&M[i]+d>M[j]).length;"
      "l.forEach((x,j)=>x.style.translate=`0 ${x==r?d:j>i&&t>=j?-h:i>j&&j>=t?h:0}px`)},"
      "s=()=>{let v=60>Y?-8:Y>innerHeight-60?8:0;if(v){scrollBy(0,v);m()}f=requestAnimationFrame(s)};"
      "g.setPointerCapture(e.pointerId);r.classList.add('dr');"
      "g.onpointermove=e=>{Y=e.clientY;m();f||s()};"
      "g.onpointerup=g.onpointercancel=e=>{cancelAnimationFrame(f);"
      "g.onpointermove=g.onpointerup=null;r.classList.remove('dr');"
      "l.forEach(x=>x.style.translate='');if(e.type!='pointerup'||t==i)return;"
      "P.insertBefore(r,l.filter(x=>x!=r)[t]||null);a(r,n())}}";
  TEST_ASSERT_EQUAL_STRING(board, SCREENS_SCRIPT);
  printf("Running order script: %u bytes\n", (unsigned)strlen(SCREENS_SCRIPT));
  // The handle's keys (about 600 bytes), its drag (about 900), the dot (about 500)
  TEST_ASSERT_TRUE(strlen(SCREENS_SCRIPT) <= 2200);
  TEST_ASSERT_NULL(strchr(SCREENS_SCRIPT, '%'));
  TEST_ASSERT_NULL(strchr(SCREENS_SCRIPT, '<'));
  const std::string s = render(EN, view(Settings()));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<script"));
  TEST_ASSERT_TRUE(has(s, std::string("<script>") + UNSAVED_SCRIPT + SCREENS_SCRIPT + "</script>"));
  TEST_ASSERT_TRUE(at(s, "</footer>") < at(s, "<script>"));
}

// MQTT or the API may set the message after the page loaded: the page posts
// back the stored message it was built with (msg0), so a save that did not
// touch the field leaves the newer one. After a refusal the field shows what
// was posted, msg0 still what is stored, so the next save writes it.
static void test_message_posts_back_what_is_stored() {
  Settings posted = filled();
  Settings stored = filled();
  snprintf(posted.message, sizeof(posted.message), "TYPED * ");
  snprintf(stored.message, sizeof(stored.message), "<b>\"&' * ");
  OrderView v = view(posted);
  v.screens.stored = &stored;
  v.screens.refusal = LIST_EMPTY;
  const std::string b = block(v, "TEXT");
  TEST_ASSERT_TRUE(has(b, "name=msg form=sv value=\"TYPED * \""));
  TEST_ASSERT_TRUE(has(b, "<input type=hidden name=msg0 form=sv "
                          "value=\"&lt;b&gt;&quot;&amp;&#39; * \">"));
  TEST_ASSERT_EQUAL(1, occurrences(renderScreen(EN, v, "TEXT"), " name=msg0 "));
}

// A field in a folded block cannot show the browser's message: a bound it
// breaks, or a number half typed ("1e", "--", "9.": badInput, which no
// attribute lifts), would stop every submit of the form without a word
// (Chromium, WebKit, Firefox). So every field is plain text, the numbers
// ask for a digit keyboard, and the save reads them (formNumber,
// formDecimal).
static void test_folded_fields_never_block_the_save() {
  const char *const blocking[] = {"type=", "required", "min=", "max=", "minlength", "pattern"};
  int numbers = 0;
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    const ScreenSettings *b = screenSettings(i);
    if (!b) continue;
    for (uint8_t f = 0; f < b->count; f++) {
      const ScreenField &field = b->fields[f];
      if (!field.attrs) continue;
      for (const char *a : blocking)
        TEST_ASSERT_NULL_MESSAGE(strstr(field.attrs, a), field.name);
      if (field.kind == FIELD_NUMBER) {
        numbers++;
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(field.attrs, "inputmode="), field.name);
      }
    }
  }
  TEST_ASSERT_EQUAL(6, numbers);   // sal, hd, hf, jm, ig, xa
}

// The suggestion's id the city field posts: digits only, within the table,
// else none (-1) and the city typed is used
static void test_form_id_digits_only() {
  TEST_ASSERT_EQUAL(0, formId("0", 0xFFFF));
  TEST_ASSERT_EQUAL(1234, formId("1234", 0xFFFF));
  TEST_ASSERT_EQUAL(65535, formId("65535", 0xFFFF));
  TEST_ASSERT_EQUAL(-1, formId("65536", 0xFFFF));
  TEST_ASSERT_EQUAL(-1, formId("99999999999999999999", 0xFFFF));
  TEST_ASSERT_EQUAL(-1, formId("abc", 0xFFFF));
  TEST_ASSERT_EQUAL(-1, formId("-1", 0xFFFF));
  TEST_ASSERT_EQUAL(-1, formId("undefined", 0xFFFF));
  TEST_ASSERT_EQUAL(-1, formId("12a", 0xFFFF));
  TEST_ASSERT_EQUAL(-1, formId("1 2", 0xFFFF));
  TEST_ASSERT_EQUAL(-1, formId("+3", 0xFFFF));
  TEST_ASSERT_EQUAL(-1, formId("", 0xFFFF));
}

// What the save reads from a number field: a number as people type it,
// held to the bounds the field had in the browser
static void test_form_number_held_to_its_bounds() {
  const long top = 2147483647L;
  TEST_ASSERT_EQUAL(9, formNumber("9", 0, 23, 7));
  TEST_ASSERT_EQUAL(23, formNumber("300", 0, 23, 7));   // a uint8_t would hold 44
  TEST_ASSERT_EQUAL(0, formNumber("-5", 0, 23, 7));
  TEST_ASSERT_EQUAL(1, formNumber("0", 1, 24, 7));
  TEST_ASSERT_EQUAL(12, formNumber("12.7", 1, 31, 7));   // the decimals dropped
  TEST_ASSERT_EQUAL(8, formNumber("8,30", 0, 23, 7));
  TEST_ASSERT_EQUAL(1, formNumber("", 1, 31, 7));       // emptied: the lowest
  TEST_ASSERT_EQUAL(0, formNumber(" ", 0, 23, 7));
  TEST_ASSERT_EQUAL(0, formNumber("-1", 0, top, 7));
  TEST_ASSERT_EQUAL(top, formNumber("99999999999999999999", 0, top, 7));
  TEST_ASSERT_EQUAL(9, formNumber("9.", 0, 23, 7));     // left half typed, then saved
  TEST_ASSERT_EQUAL(23, formNumber("abc", 0, 23, 44));  // what is kept is held too
}

// Followers as people write them: digits grouped by a space (ASCII, the
// no-break and the narrow no-break space of French number formatting), a
// comma or a point; three digits after a comma or a point are a group,
// never decimals ("1,240" is 1240, not 1)
static void test_form_number_reads_grouped_digits() {
  const long top = 2147483647L;
  TEST_ASSERT_EQUAL(1240, formNumber("1,240", 0, top, 7));
  TEST_ASSERT_EQUAL(12500, formNumber("12,500", 0, top, 7));
  TEST_ASSERT_EQUAL(1240, formNumber("1.240", 0, top, 7));
  TEST_ASSERT_EQUAL(1234567, formNumber("1,234,567", 0, top, 7));
  TEST_ASSERT_EQUAL(1234567, formNumber("1.234.567", 0, top, 7));
  TEST_ASSERT_EQUAL(12500, formNumber("12 500", 0, top, 7));
  TEST_ASSERT_EQUAL(1240, formNumber("1\u00A0240", 0, top, 7));
  TEST_ASSERT_EQUAL(1240, formNumber("1\u202F240", 0, top, 7));
  TEST_ASSERT_EQUAL(1240, formNumber(" 1 240 ", 0, top, 7));
  TEST_ASSERT_EQUAL(1240, formNumber("1,240.00", 0, top, 7));
}

// Text that is not a number keeps the stored value: a part of it ("1" of
// "1e", "1" of "1.2k") would be saved as a value nobody typed
static void test_form_number_keeps_what_is_stored_for_any_other_text() {
  const char *const others[] = {"abc", "1e", "1e6", "--", "-", "+", "1.2k", "12abc", "9h",
                                "1,2,3", "12,50,000", "1,,240", ",5", ".5", "1-", "0x1A",
                                "inf", "nan", "1'240", "999999999999999999999999999999999"};
  for (const char *t : others) TEST_ASSERT_EQUAL_MESSAGE(1240, formNumber(t, 0, 2147483647L, 1240), t);
}

// The salary as typed: a French keypad's decimal key is a comma; the last
// comma or point is the decimal one unless three digits follow it, the others
// group digits ("2.850,50", "2,850.50"), and so do spaces
static void test_form_decimal_reads_what_is_typed() {
  TEST_ASSERT_EQUAL_FLOAT(2850.5f, formDecimal("2850.5", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.5f, formDecimal("2850,50", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.5f, formDecimal("2 850,5", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.5f, formDecimal("2,850.50", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.5f, formDecimal("2.850,50", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.0f, formDecimal("2,850", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.0f, formDecimal("2.850", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.5f, formDecimal("2\u00A0850,50", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.0f, formDecimal("2\u00A0850", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.5f, formDecimal("2\u202F850,50", 7));
  TEST_ASSERT_EQUAL_FLOAT(2850.0f, formDecimal("2\u202F850", 7));
  TEST_ASSERT_EQUAL_FLOAT(1234567.5f, formDecimal("1.234.567,5", 7));
  TEST_ASSERT_EQUAL_FLOAT(1.5f, formDecimal("1,5", 7));
  TEST_ASSERT_EQUAL_FLOAT(12.3456f, formDecimal("12.3456", 7));   // four digits: decimals
  TEST_ASSERT_EQUAL_FLOAT(-12.25f, formDecimal("-12.25", 7));
  TEST_ASSERT_EQUAL_FLOAT(9.0f, formDecimal("9.", 7));
  TEST_ASSERT_EQUAL_FLOAT(9.0f, formDecimal("9,", 7));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, formDecimal("", 7));   // emptied: saved as 0, as before
}

// Any other text keeps the stored salary, so it is never 1 for "1e" nor
// 2.85 for a grouping nobody uses
static void test_form_decimal_keeps_what_is_stored_for_any_other_text() {
  const char *const others[] = {"abc", "1e", "1e999", "--", "-", "inf", "nan", "0x1A",
                                "2850 EUR", "2,85,0", "2.8.5", "1'240",
                                "9999999999999999999999999999999999999999999999"};
  for (const char *t : others) TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2000.0f, formDecimal(t, 2000.0f), t);
}

// The MQTT password field (web.cpp's form) alike: its marker reads what is
// stored, whatever was posted, and it never carries a password
static void test_mqtt_password_marker_from_what_is_stored() {
  Settings stored = filled();
  stored.mqttPass[0] = '\0';
  TEST_ASSERT_NULL(strstr(mqttPassAttrs(stored), "&bull;"));
  snprintf(stored.mqttPass, sizeof(stored.mqttPass), "hunter2");
  TEST_ASSERT_NOT_NULL(strstr(mqttPassAttrs(stored), "&bull;"));
  TEST_ASSERT_NULL(strstr(mqttPassAttrs(stored), "hunter2"));
  TEST_ASSERT_NOT_NULL(strstr(mqttPassAttrs(stored), "type=password"));
}

// The save writes a field only when what was posted differs from what the
// page showed; a post without the original (an older page, a script) is
// written as before
static void test_form_edited() {
  TEST_ASSERT_FALSE(formEdited("HELLO * ", "HELLO * "));
  TEST_ASSERT_FALSE(formEdited("", ""));
  TEST_ASSERT_TRUE(formEdited("HELLO", "HELLO * "));
  TEST_ASSERT_TRUE(formEdited("", "HELLO * "));
  TEST_ASSERT_TRUE(formEdited("HELLO * ", nullptr));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_section_order_and_controls);
  RUN_TEST(test_lead_thumbnails_and_live_dot);
  RUN_TEST(test_save_bar_count);
  RUN_TEST(test_unsaved_after_a_failed_save);
  RUN_TEST(test_one_h1_one_main);
  RUN_TEST(test_order_form_carries_no_panel_field);
  RUN_TEST(test_answer_page_by_origin);
  RUN_TEST(test_screen_page_route);
  RUN_TEST(test_result_banners);
  RUN_TEST(test_marks_from_declared_orientation);
  RUN_TEST(test_rows_identify_screens_by_name);
  RUN_TEST(test_rows_carry_a_drag_handle);
  RUN_TEST(test_arrows_disabled_at_the_ends);
  RUN_TEST(test_off_rows_unchecked);
  RUN_TEST(test_default_save_first_and_save_after_list);
  RUN_TEST(test_refusal_banner_variants);
  RUN_TEST(test_row_summaries);
  RUN_TEST(test_money_label_follows_the_currency);
  RUN_TEST(test_row_summary_of_a_long_message);
  RUN_TEST(test_row_links_to_its_page);
  RUN_TEST(test_row_thumbnail_follows_style);
  RUN_TEST(test_screen_page_markup);
  RUN_TEST(test_image_page_has_no_form);
  RUN_TEST(test_screen_page_after_a_save);
  RUN_TEST(test_image_block_summary);
  RUN_TEST(test_social_block_counts);
  RUN_TEST(test_blocks_hold_their_fields);
  RUN_TEST(test_blocks_only_from_declarations);
  RUN_TEST(test_code_block_offers_its_styles);
  RUN_TEST(test_clock_block_offers_its_styles_and_items);
  RUN_TEST(test_weather_block_offers_its_styles);
  RUN_TEST(test_style_controls_only_in_their_block);
  RUN_TEST(test_styles_as_posted);
  RUN_TEST(test_styles_saved_from_the_form);
  RUN_TEST(test_styles_keep_what_is_stored_without_an_option);
  RUN_TEST(test_folded_fields_never_block_the_save);
  RUN_TEST(test_form_id_digits_only);
  RUN_TEST(test_form_number_held_to_its_bounds);
  RUN_TEST(test_form_number_reads_grouped_digits);
  RUN_TEST(test_form_number_keeps_what_is_stored_for_any_other_text);
  RUN_TEST(test_form_decimal_reads_what_is_typed);
  RUN_TEST(test_form_decimal_keeps_what_is_stored_for_any_other_text);
  RUN_TEST(test_city_field_has_its_suggestions_markup);
  RUN_TEST(test_city_script_is_weathers_alone);
  RUN_TEST(test_titles_escape_user_text);
  RUN_TEST(test_secret_never_sent_back);
  RUN_TEST(test_secret_marker_from_what_is_stored);
  RUN_TEST(test_mqtt_password_marker_from_what_is_stored);
  RUN_TEST(test_values_as_posted);
  RUN_TEST(test_french_page);
  RUN_TEST(test_page_fits_24k);
  RUN_TEST(test_script_is_static_and_small);
  RUN_TEST(test_message_posts_back_what_is_stored);
  RUN_TEST(test_form_edited);
  return UNITY_END();
}
