// SPDX-License-Identifier: AGPL-3.0-only
// What every page shares: the head with its versioned stylesheet, the brand
// row with the host, the tabs, the skip link, the footer, and the stylesheet
// itself, compressed in flash. Built without the server.
//
//   pio test -e native -f test_web_shell
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/i18n.cpp"
#include "../../src/web_shell.cpp"
#include "../../src/web_assets.cpp"

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

static Shell shell(const Strings &t, Tab tab = TAB_STAGE, bool tabs = true,
                   const char *host = "carousel.local") {
  return Shell{&t, "Stage", host, tab, tabs, "1.2.3"};
}

static std::string page(const Shell &sh, const char *body = "<h1>Stage</h1>",
                        const char *script = nullptr) {
  String s;
  shellStart(s, sh);
  s += body;
  shellEnd(s, sh, script);
  return s.c_str();
}

// One main, the skip link first and pointing at it, header and footer around
static void test_landmarks_and_skip_link() {
  const std::string p = page(shell(EN));
  TEST_ASSERT_EQUAL(0, (int)p.find("<!doctype html>"));
  const size_t body = at(p, "<body>");
  const size_t skip = at(p, "<a class=skip href=\"#m\">Skip to content</a>");
  TEST_ASSERT_EQUAL(body + 6, skip);
  const size_t header = at(p, "<header>");
  const size_t main = at(p, "<main id=m><div class=w>");
  const size_t h1 = at(p, "<h1>Stage</h1>");
  const size_t end = at(p, "</div></main><footer>");
  TEST_ASSERT_TRUE(skip < header && header < main && main < h1 && h1 < end);
  TEST_ASSERT_EQUAL(1, occurrences(p, "<main"));
  TEST_ASSERT_EQUAL(1, occurrences(p, "<h1"));
  TEST_ASSERT_TRUE(p.size() > 20 && p.compare(p.size() - 14, 14, "</body></html>") == 0);
  // The monogram and the name, linked home
  at(p, "<a class=brand href=\"/\">Carousel</a>");
}

static void test_tabs_mark_the_current_page() {
  static const char *const HREF[] = {"/", "/screens", "/images", "/panel"};
  for (int tab = TAB_STAGE; tab <= TAB_PANEL; tab++) {
    const std::string p = page(shell(EN, (Tab)tab));
    const size_t nav = at(p, "<nav aria-label=\"Pages\"><div class=w>");
    TEST_ASSERT_TRUE(nav > at(p, "</header>") && nav < at(p, "<main"));
    TEST_ASSERT_EQUAL(1, occurrences(p, "aria-current=page"));
    for (int i = 0; i < 4; i++) {
      std::string link = std::string("<a href=\"") + HREF[i] + "\"";
      if (i == tab) link += " aria-current=page";
      link += std::string(">") + EN.webTabs[i] + "</a>";
      at(p, link);
    }
  }
  // Reached from Panel, the Wi-Fi page: the tabs, none of them current
  const std::string p = page(shell(EN, TAB_NONE));
  at(p, "<nav aria-label=\"Pages\">");
  TEST_ASSERT_EQUAL(0, occurrences(p, "aria-current"));
  TEST_ASSERT_EQUAL(4, occurrences(p.substr(at(p, "<nav"), at(p, "</nav>") - at(p, "<nav")),
                    "<a href="));
  // French names
  const std::string f = page(shell(FR, TAB_ORDER));
  at(f, "<a href=\"/screens\" aria-current=page>Ordre de passage</a>");
  at(f, "<a href=\"/\">Sc\xC3\xA8ne</a>");
}

// Sign-in, setup and locked: no nav, the brand points at the content
static void test_no_tabs_mode() {
  const std::string p = page(shell(EN, TAB_STAGE, false));
  TEST_ASSERT_FALSE(has(p, "<nav"));
  TEST_ASSERT_FALSE(has(p, "aria-current"));
  TEST_ASSERT_FALSE(has(p, "href=\"/screens\""));
  at(p, "<a class=brand href=\"#m\">Carousel</a>");
  at(p, "<main id=m>");
}

static void test_host_is_escaped() {
  const std::string p = page(shell(EN, TAB_STAGE, true, "<b>\"'&"));
  at(p, "<span class=host>&lt;b&gt;&quot;&#39;&amp;</span>");
  TEST_ASSERT_FALSE(has(p, "<b>\""));
  // No host: the setup access point's name for the brand row
  const std::string setup = page(shell(FR, TAB_NONE, false, nullptr));
  at(setup, std::string("<span class=host>") + FR.webSetupBrand + "</span>");
  TEST_ASSERT_EQUAL_STRING("Carousel setup", EN.webSetupBrand);
}

// AGPL section 13: the source is offered to whoever uses the panel
static void test_footer_has_source_version_licence_notice() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string p = page(shell(*t));
    const size_t footer = at(p, "<footer><div class=w>");
    const size_t link = at(p, std::string("<a href=\"https://github.com/Serein-Design/carousel-firmware\">") +
                                  t->webSource + "</a>", footer);
    const size_t version = at(p, " \xC2\xB7 1.2.3 \xC2\xB7 AGPL-3.0", link);
    const size_t notice = at(p, std::string("<small>") + t->webDisclaimer + "</small>", version);
    TEST_ASSERT_TRUE(notice < at(p, "</footer>"));
  }
}

// CC BY 4.0: the pages that show the weather (Stage's preview, Running
// order's WEATHER and its city) credit MET Norway and GeoNames, with links
static void test_footer_credits_the_weather_data() {
  static const char CC[] = "(<a href=\"https://creativecommons.org/licenses/by/4.0/\">CC BY 4.0</a>)";
  const std::string met = std::string("<a href=\"https://api.met.no/\">MET Norway</a> ") + CC;
  const std::string geo = std::string("<a href=\"https://www.geonames.org/\">GeoNames</a> ") + CC;
  for (Tab tab : {TAB_STAGE, TAB_ORDER}) {
    std::string p = page(shell(EN, tab));
    const size_t credit = at(p, "<small>Weather: " + met + " \xC2\xB7 Places: " + geo + "</small>");
    TEST_ASSERT_TRUE(at(p, "<footer>") < credit && credit < at(p, "</footer>"));
    p = page(shell(FR, tab));
    at(p, "<small>M\xC3\xA9t\xC3\xA9o\xE2\x80\xAF: " + met + " \xC2\xB7 Lieux\xE2\x80\xAF: " + geo +
              "</small>");
  }
  for (Tab tab : {TAB_IMAGES, TAB_PANEL, TAB_NONE})
    TEST_ASSERT_FALSE(has(page(shell(EN, tab)), "api.met.no"));
  TEST_ASSERT_FALSE(has(page(shell(EN, TAB_STAGE, false)), "api.met.no"));   // sign-in, setup
}

static void test_title_format() {
  TEST_ASSERT_EQUAL_STRING("%s \xC2\xB7 Carousel", EN.webTitle);
  TEST_ASSERT_EQUAL_STRING("%s \xC2\xB7 Carousel", FR.webTitle);
  at(page(shell(EN)), "<title>Stage \xC2\xB7 Carousel</title>");
  Shell sh = shell(FR);
  sh.title = "Wi-Fi du panneau";
  at(page(sh), "<title>Wi-Fi du panneau \xC2\xB7 Carousel</title>");
}

static void test_lang_attribute() {
  TEST_ASSERT_EQUAL(0, (int)page(shell(EN)).find("<!doctype html><html lang=en><head>"));
  TEST_ASSERT_EQUAL(0, (int)page(shell(FR)).find("<!doctype html><html lang=fr><head>"));
}

// The stylesheet's address carries the version: a new firmware is a new
// address, so the browser's year-long copy is never stale
static void test_asset_paths_follow_the_version() {
  char out[40];
  TEST_ASSERT_EQUAL(14, assetPath(out, sizeof(out), "1.2.3", "c.css"));
  TEST_ASSERT_EQUAL_STRING("/s/1.2.3/c.css", out);
  assetPath(out, sizeof(out), "0.2.0", "p.png");
  TEST_ASSERT_EQUAL_STRING("/s/0.2.0/p.png", out);
  // Cut short: the length it needed, so the caller can tell
  char tiny[8];
  TEST_ASSERT_EQUAL(14, assetPath(tiny, sizeof(tiny), "1.2.3", "c.css"));
  TEST_ASSERT_EQUAL_STRING("/s/1.2.", tiny);

  const std::string p = page(shell(EN));
  at(p, "<link rel=stylesheet href=\"/s/1.2.3/c.css\">");
  TEST_ASSERT_TRUE(at(p, "<link rel=stylesheet") < at(p, "</head>"));
  TEST_ASSERT_FALSE(has(p, "<style"));
  Shell sh = shell(EN);
  sh.version = "9.9.9";
  at(page(sh), "href=\"/s/9.9.9/c.css\"");
}

static void test_stylesheet_fits_and_is_gzip() {
  TEST_ASSERT_TRUE(STYLESHEET_GZ_LEN > 1000);
  TEST_ASSERT_TRUE_MESSAGE(STYLESHEET_GZ_LEN <= 6144, "the stylesheet's flash budget");
  TEST_ASSERT_EQUAL_HEX8(0x1f, STYLESHEET_GZ[0]);
  TEST_ASSERT_EQUAL_HEX8(0x8b, STYLESHEET_GZ[1]);
  TEST_ASSERT_EQUAL_HEX8(8, STYLESHEET_GZ[2]);   // deflate
}

// The readable source the compressed bytes come from: system fonts, nothing
// fetched but the poster sheet next to it (inline data: images aside)
static void test_no_web_font_no_script_framework() {
  FILE *f = fopen("web/page.css", "rb");   // pio runs the tests from the project folder
  TEST_ASSERT_NOT_NULL_MESSAGE(f, "firmware/web/page.css");
  std::string css;
  char buf[4096];
  for (size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0; ) css.append(buf, n);
  fclose(f);
  TEST_ASSERT_TRUE(css.size() > 4000);
  TEST_ASSERT_FALSE(has(css, "@font-face"));
  TEST_ASSERT_FALSE(has(css, "@import"));
  int sheet = 0;
  for (size_t p = css.find("url("); p != std::string::npos; p = css.find("url(", p + 1)) {
    const std::string arg = css.substr(p + 4, 12);
    const bool png = arg.compare(0, 6, "p.png)") == 0;
    const bool data = arg.compare(0, 6, "\"data:") == 0;
    TEST_ASSERT_TRUE_MESSAGE(png || data, css.substr(p, 40).c_str());
    sheet += png;
  }
  TEST_ASSERT_EQUAL(1, sheet);
  at(css, "-apple-system");
  // The page: one stylesheet link, no script but the page's own
  const std::string p = page(shell(EN), "<h1>Stage</h1>", "var a=1");
  TEST_ASSERT_EQUAL(1, occurrences(p, "<link "));
  TEST_ASSERT_EQUAL(1, occurrences(p, "<script"));
  TEST_ASSERT_FALSE(has(p, "<script src"));
  TEST_ASSERT_TRUE(at(p, "</footer>") < at(p, "<script>var a=1</script></body></html>"));
  TEST_ASSERT_FALSE(has(page(shell(EN)), "<script"));
}

// Notices and posters, the pieces pages share
static void test_notices_and_posters() {
  String s;
  notice(s, "Saved.");
  errorNotice(s, "<b>Not saved.</b>");
  errorNotice(s, "Wrong.", "e1");
  poster(s, 7, false);
  poster(s, 26, true);
  const std::string p = s.c_str();
  at(p, "<div class=ok>Saved.</div>");
  at(p, "<div class=er role=alert><b>Not saved.</b></div>");
  at(p, "<div class=er role=alert id=\"e1\">Wrong.</div>");
  at(p, "<i class=th style=\"--p:7\" aria-hidden=true></i>");
  at(p, "<i class=\"th b\" style=\"--p:26\" aria-hidden=true></i>");
}

// No page reloads itself: a timed refresh takes the page away from someone
// still reading it (WCAG 2.2.1); a slow answer waits on the panel instead
static void test_never_reloads_itself() {
  for (const Tab tab : {TAB_NONE, TAB_STAGE, TAB_PANEL})
    for (const bool tabs : {true, false}) {
      const std::string p = page(shell(EN, tab, tabs, tabs ? "carousel.local" : nullptr));
      TEST_ASSERT_FALSE(has(p, "http-equiv"));
      TEST_ASSERT_FALSE(has(p, "refresh"));
    }
}

// The pages' one "%s" filler: a, b then c, in order, however long they are
static void test_format_text() {
  String s;
  formatText(s, "%s: %s", "Space", std::string(300, 'x').c_str());
  TEST_ASSERT_EQUAL_STRING((std::string("Space: ") + std::string(300, 'x')).c_str(), s.c_str());
  s = "";
  formatText(s, FR.webTitleValue, "Ville", "Lyon");
  TEST_ASSERT_EQUAL_STRING("Ville\u202F: Lyon", s.c_str());
  s = "";
  formatText(s, "Delete %s", "a&amp;b.gif");   // one value: b unused
  TEST_ASSERT_EQUAL_STRING("Delete a&amp;b.gif", s.c_str());
  s = "";
  formatText(s, "%s", "x", "y");               // a value with no %s left: dropped
  TEST_ASSERT_EQUAL_STRING("x", s.c_str());
  s = "";
  formatText(s, "%s, %s of %s", "A", "3", "24");   // three values
  TEST_ASSERT_EQUAL_STRING("A, 3 of 24", s.c_str());
  s = "";
  formatText(s, "%s %s %s %s 5%", "a");        // past c: kept as typed
  TEST_ASSERT_EQUAL_STRING("a   %s 5%", s.c_str());
}

// The save bar a form page shares: the words and Discard are hidden in the
// markup, so without a script Save alone shows, as before; the script (one,
// static, after the page's own) shows the bar once a field differs
static void test_unsaved_bar_and_script() {
  for (const Strings *t : {&EN, &FR}) {
    String s;
    saveBarStart(s, *t, "mb", false);
    TEST_ASSERT_EQUAL_STRING((std::string("<div class=\"sv mb\"><p class=u hidden>") + t->webUnsaved +
                              "</p><button class=p form=sv>" + t->webSave +
                              "</button><button type=button class=dc form=sv hidden>" +
                              t->webDiscard + "</button>").c_str(), s.c_str());
  }
  // After a refused or failed save the page holds the posted values: the
  // bar is already up
  String on;
  saveBarStart(on, EN, "dk", true);
  TEST_ASSERT_EQUAL(0, std::string(on.c_str()).rfind("<div class=\"sv dk on\"><p class=u hidden>", 0));
  TEST_ASSERT_EQUAL_STRING("Unsaved changes", EN.webUnsaved);
  TEST_ASSERT_EQUAL_STRING("Discard", EN.webDiscard);
  TEST_ASSERT_EQUAL_STRING("Modifications non enregistr\u00e9es", FR.webUnsaved);
  TEST_ASSERT_EQUAL_STRING("Ignorer", FR.webDiscard);
  const size_t n = strlen(UNSAVED_SCRIPT);
  printf("Unsaved-changes script: %u bytes\n", (unsigned)n);
  TEST_ASSERT_TRUE(n <= 700);
  TEST_ASSERT_NULL(strchr(UNSAVED_SCRIPT, '%'));
  TEST_ASSERT_NULL(strchr(UNSAVED_SCRIPT, '<'));
  TEST_ASSERT_NOT_NULL(strstr(UNSAVED_SCRIPT, "onbeforeunload"));
  TEST_ASSERT_NOT_NULL(strstr(UNSAVED_SCRIPT, ".reset()"));
  // Starts dirty on a page marked data-d, whose Discard opens that address;
  // compares with the markup's defaults, not values a browser restored
  TEST_ASSERT_NOT_NULL(strstr(UNSAVED_SCRIPT, "F.dataset.d"));
  TEST_ASSERT_NOT_NULL(strstr(UNSAVED_SCRIPT, "location="));
  TEST_ASSERT_NOT_NULL(strstr(UNSAVED_SCRIPT, "defaultChecked"));
  TEST_ASSERT_NOT_NULL(strstr(UNSAVED_SCRIPT, "defaultValue"));
  TEST_ASSERT_NOT_NULL(strstr(UNSAVED_SCRIPT, "defaultSelected"));
  String s;
  shellEnd(s, shell(EN), UNSAVED_SCRIPT, "x()");
  TEST_ASSERT_TRUE(has(s.c_str(), std::string("<script>") + UNSAVED_SCRIPT + "x()</script></body>"));
  TEST_ASSERT_EQUAL(1, occurrences(s.c_str(), "<script>"));
}

// A missing address under the API or the hooks gets a machine's 404, with
// or without the trailing slash; any other one the page
static void test_machine_paths() {
  for (const char *u : {"/api", "/api/", "/api/nope", "/claude", "/claude/", "/claude/x/y"})
    TEST_ASSERT_TRUE_MESSAGE(machinePath(u), u);
  for (const char *u : {"/", "/apix", "/api.json", "/claudes", "/x/api/", "/screens", ""})
    TEST_ASSERT_FALSE_MESSAGE(machinePath(u), u);
}

// The hooks' neutral route: a wrong address answers a hook's 404, not a page
static void test_agent_path_gets_machine_404() {
  for (const char *u : {"/agent", "/agent/", "/agent/nope", "/agent/x/y"})
    TEST_ASSERT_TRUE_MESSAGE(machinePath(u), u);
  for (const char *u : {"/agents", "/agent.sh", "/x/agent/"})
    TEST_ASSERT_FALSE_MESSAGE(machinePath(u), u);
}

static void test_escape_html() {
  TEST_ASSERT_EQUAL_STRING("a&lt;b&gt;&amp;&quot;&#39;z", escapeHtml("a<b>&\"'z").c_str());
  TEST_ASSERT_EQUAL_STRING("", escapeHtml("").c_str());
  TEST_ASSERT_EQUAL_STRING("Caf\xC3\xA9", escapeHtml("Caf\xC3\xA9").c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_landmarks_and_skip_link);
  RUN_TEST(test_tabs_mark_the_current_page);
  RUN_TEST(test_no_tabs_mode);
  RUN_TEST(test_host_is_escaped);
  RUN_TEST(test_footer_has_source_version_licence_notice);
  RUN_TEST(test_footer_credits_the_weather_data);
  RUN_TEST(test_title_format);
  RUN_TEST(test_lang_attribute);
  RUN_TEST(test_asset_paths_follow_the_version);
  RUN_TEST(test_stylesheet_fits_and_is_gzip);
  RUN_TEST(test_no_web_font_no_script_framework);
  RUN_TEST(test_notices_and_posters);
  RUN_TEST(test_never_reloads_itself);
  RUN_TEST(test_escape_html);
  RUN_TEST(test_unsaved_bar_and_script);
  RUN_TEST(test_machine_paths);
  RUN_TEST(test_agent_path_gets_machine_404);
  RUN_TEST(test_format_text);
  return UNITY_END();
}
