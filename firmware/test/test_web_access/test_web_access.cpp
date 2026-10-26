// SPDX-License-Identifier: AGPL-3.0-only
// The pages a visitor reaches before the others: the Wi-Fi page (the setup
// access point's captive page, and Change Wi-Fi from Panel), the sign-in
// page and the locked page. Built without the server; web.cpp gathers the
// scan and the state and sends the page.
//
//   pio test -e native -f test_web_access
#include <unity.h>

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
#include "../../src/web_access.cpp"

// settings.cpp's lock, read by content_logic's choice
bool panelLocked() { return false; }

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

static WifiNet net(const char *ssid, int16_t rssi, bool open = false) {
  WifiNet n = {};
  snprintf(n.ssid, sizeof(n.ssid), "%s", ssid);
  n.rssi = rssi;
  n.open = open;
  return n;
}

static const WifiNet ROWS[] = {
  net("Maison-2.4G", -52), net("Livebox-7F2A", -67), net("Freebox_Invite", -71, true),
};

static Shell shell(const Strings &t) {
  return Shell{&t, "ignored", "carousel.local", TAB_STAGE, true, "1.2.3"};
}

static WifiPageView view(WifiLink link, const char *saved = "Maison-2.4G") {
  return WifiPageView{saved, link, "192.168.1.20", TRY_NONE, ROWS, 3, nullptr, nullptr};
}

static std::string page(const Strings &t, const WifiPageView &v) {
  String s;
  wifiPage(s, shell(t), v);
  return s.c_str();
}

// The radio row of a network: its value, then " data-pw" when choosing it
// asks for a password, then " checked"
static std::string row(const char *ssid, bool pw, bool checked) {
  return std::string("<input type=radio name=ssid value=\"") + ssid + "\"" +
         (pw ? " data-pw" : "") + (checked ? " checked" : "") + ">";
}

// A network often broadcasts from several access points: one row per name,
// its strongest signal, strongest first; hidden networks have no name to show
static void test_rows_sorted_deduped_capped() {
  WifiNet nets[40];
  for (int i = 0; i < 40; i++) {
    char name[33];
    snprintf(name, sizeof(name), "net-%02d", i % 35);   // 5 names seen twice
    nets[i] = net(name, (int16_t)(-90 + i));
  }
  nets[3] = net("", -20);                                 // hidden, the strongest
  nets[38] = net("net-03", -95);                          // weaker copy of a name
  const int n = wifiRows(nets, 40);
  TEST_ASSERT_EQUAL(WIFI_ROWS_MAX, n);
  TEST_ASSERT_EQUAL(30, WIFI_ROWS_MAX);
  for (int i = 0; i < n; i++) {
    TEST_ASSERT_TRUE(nets[i].ssid[0] != '\0');
    if (i) TEST_ASSERT_TRUE(nets[i - 1].rssi >= nets[i].rssi);
    for (int j = 0; j < i; j++) TEST_ASSERT_NOT_EQUAL(0, strcmp(nets[i].ssid, nets[j].ssid));
  }
  // The names seen twice keep their second, stronger copy (-55 to -51);
  // net-03's strong copy is the hidden one, so its -95 is the weakest of 35
  // names and the cap drops it
  static const char *const FIRST[] = {"net-04", "net-02", "net-01", "net-00", "net-34"};
  static const int16_t SIGNAL[] = {-51, -53, -54, -55, -56};
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL_STRING(FIRST[i], nets[i].ssid);
    TEST_ASSERT_EQUAL(SIGNAL[i], nets[i].rssi);
  }
  TEST_ASSERT_EQUAL_STRING("net-09", nets[29].ssid);
  for (int i = 0; i < n; i++) TEST_ASSERT_NOT_EQUAL(0, strcmp("net-03", nets[i].ssid));

  // Fewer than the cap: all kept, the open mark follows the strongest copy
  WifiNet few[] = {net("b", -70), net("a", -60, true), net("b", -40, true), net("", -10)};
  TEST_ASSERT_EQUAL(2, wifiRows(few, 4));
  TEST_ASSERT_EQUAL_STRING("b", few[0].ssid);
  TEST_ASSERT_EQUAL(-40, few[0].rssi);
  TEST_ASSERT_TRUE(few[0].open);
  TEST_ASSERT_EQUAL_STRING("a", few[1].ssid);
  TEST_ASSERT_EQUAL(0, wifiRows(few, 0));
}

// The saved network first, on its own, checked; the others folded under it,
// strongest first, each once
static void test_known_network_first() {
  std::string p = page(EN, view(LINK_CONNECTED));
  const size_t known = at(p, std::string("<h2>") + EN.wifiKnown + "</h2><div class=nl>");
  const size_t mine = at(p, std::string("<label class=op>") + row("Maison-2.4G", false, true), known);
  const size_t fold = at(p, std::string("<details class=nw><summary>") + EN.wifiOthers + "</summary>");
  TEST_ASSERT_TRUE(mine < fold);
  const size_t b = at(p, row("Livebox-7F2A", true, false), fold);
  const size_t c = at(p, row("Freebox_Invite", false, false), b);
  const size_t other = at(p, row("", true, false), c);
  at(p, std::string("<span>") + EN.wifiAnother + "</span></label>", other);
  TEST_ASSERT_TRUE(other < at(p, "</details>"));
  TEST_ASSERT_EQUAL(1, occurrences(p, "value=\"Maison-2.4G\""));
  TEST_ASSERT_EQUAL(1, occurrences(p, " checked"));
  TEST_ASSERT_EQUAL(4, occurrences(p, "<label class=op>"));
  TEST_ASSERT_FALSE(has(p, "<details class=nw open>"));
  // Its last try refused: the stored password is wrong, so choosing it asks
  // for one (the field shows under it)
  WifiPageView v = view(LINK_SETUP);
  v.lastTry = TRY_REFUSED;
  at(page(EN, v), row("Maison-2.4G", true, true));
  // Refused but open: nothing to type
  const WifiNet open[] = {net("Cafe", -60, true)};
  v.saved = "Cafe";
  v.rows = open;
  v.count = 1;
  at(page(EN, v), row("Cafe", false, true));

  // Saved but out of range: said in words, nothing to choose; the list open
  p = page(EN, view(LINK_OFFLINE, "Bureau <5G>"));
  const size_t away = at(p, std::string("<h2>") + EN.wifiKnown + "</h2><p class=hint>");
  String line;
  formatText(line, EN.wifiAway, "Bureau &lt;5G&gt;");
  at(p, line.c_str(), away);
  TEST_ASSERT_EQUAL_STRING("%s is not in range right now.", EN.wifiAway);
  at(p, std::string("<details class=nw open><summary>") + EN.wifiOthers + "</summary>", away);
  at(p, row("", true, true));
  at(p, row("Maison-2.4G", true, false));
  TEST_ASSERT_EQUAL(1, occurrences(p, " checked"));
  // Nothing saved: no known section, "Networks" open, the name row checked
  p = page(FR, view(LINK_SETUP, ""));
  TEST_ASSERT_FALSE(has(p, FR.wifiKnown));
  at(p, std::string("<details class=nw open><summary>") + FR.wifiNetworks + "</summary>");
  at(p, row("", true, true));
  at(p, std::string("<span>") + FR.wifiAnother + "</span>");
  TEST_ASSERT_EQUAL(4, occurrences(p, "<label class=op>"));
}

// Bars and a word a person reads, never dBm; a lock on a secured network;
// the accessible name says it all in one phrase
static void test_signal_in_words_and_bars() {
  TEST_ASSERT_EQUAL(4, wifiBars(-30));
  TEST_ASSERT_EQUAL(4, wifiBars(-55));
  TEST_ASSERT_EQUAL(3, wifiBars(-56));
  TEST_ASSERT_EQUAL(3, wifiBars(-67));
  TEST_ASSERT_EQUAL(2, wifiBars(-68));
  TEST_ASSERT_EQUAL(2, wifiBars(-75));
  TEST_ASSERT_EQUAL(1, wifiBars(-76));
  TEST_ASSERT_EQUAL(1, wifiBars(-100));
  TEST_ASSERT_EQUAL(4, wifiBars(0));
  TEST_ASSERT_EQUAL_STRING("excellent", EN.wifiSignal[3]);
  TEST_ASSERT_EQUAL_STRING("weak", EN.wifiSignal[0]);
  const std::string p = page(EN, view(LINK_CONNECTED));
  TEST_ASSERT_FALSE(has(p, "dBm"));
  at(p, ">Maison-2.4G</span><span class=vh>, excellent signal, secured</span>"
        "<span class=\"num k\" aria-hidden=true><i class=\"sb s4\"></i>excellent</span></label>");
  at(p, ">Livebox-7F2A</span><span class=vh>, good signal, secured</span>"
        "<span class=\"num k\" aria-hidden=true><i class=\"sb s3\"></i>good</span></label>");
  at(p, ">Freebox_Invite</span><span class=vh>, fair signal, open</span>"
        "<span class=num aria-hidden=true><i class=\"sb s2\"></i>fair</span></label>");
  const std::string fr = page(FR, view(LINK_CONNECTED));
  at(fr, std::string(">Maison-2.4G</span><span class=vh>, signal ") + FR.wifiSignal[3] +
         ", s\xC3\xA9" "curis\xC3\xA9</span>");
}

// One password field, after the lists: without scripts it serves whichever
// row is chosen; with them it moves under the chosen row when that one asks
// for a password, and is hidden and disabled (not posted) otherwise
static void test_one_password_field() {
  for (const WifiPageView &v : {view(LINK_CONNECTED), view(LINK_SETUP, "")}) {
    const std::string p = page(EN, v);
    TEST_ASSERT_EQUAL(1, occurrences(p, "type=password"));
    const size_t fold = at(p, "</details>");
    const size_t pw = at(p, std::string("<div id=pb><label for=pw>") + EN.wifiPassword +
                            "</label><input id=pw name=mdp type=password maxlength=63 "
                            "autocomplete=current-password><small>" + EN.wifiPasswordHint +
                            "</small></div>", fold);
    const size_t go = at(p, std::string("<button class=p style=\"width:100%;margin-top:24px\">") +
                            EN.wifiContinue + "</button><small>" + EN.wifiFooter + "</small></form>",
                         pw);
    TEST_ASSERT_TRUE(at(p, std::string("<small>") + EN.wifiBand + "</small>", pw) < go);
    // The name field under the name row, inside the list
    const size_t name = at(p, std::string("<span>") + EN.wifiAnother + "</span></label><div id=nb><label for=on>" +
                              EN.wifiName + "</label><input id=on name=other maxlength=32 "
                              "autocomplete=off></div></div></details>");
    TEST_ASSERT_TRUE(name < fold);
    // Shown without JavaScript; the script hides it unless its row is chosen
    TEST_ASSERT_FALSE(has(p, "<div id=nb hidden"));
    TEST_ASSERT_TRUE(has(p, "N.hidden=!R[R.length-1].checked"));
  }
  TEST_ASSERT_EQUAL_STRING("Continue", EN.wifiContinue);
  TEST_ASSERT_EQUAL_STRING("Continuer", FR.wifiContinue);
}

// A neighbour's network name is anything they want it to be
static void test_names_escaped() {
  const char *evil = "<script>\"'&aaaaaaaaaaaaaaaaaaaaa";   // 32 bytes
  TEST_ASSERT_EQUAL(32, (int)strlen(evil));
  const WifiNet rows[] = {net(evil, -40)};
  WifiPageView v = view(LINK_SETUP, evil);
  v.rows = rows;
  v.count = 1;
  v.lastTry = TRY_REFUSED;
  const std::string p = page(EN, v);
  const std::string esc = "&lt;script&gt;&quot;&#39;&amp;aaaaaaaaaaaaaaaaaaaaa";
  at(p, std::string("value=\"") + esc + "\" data-pw checked>");
  at(p, std::string("<span>") + esc + "</span><span class=vh>");
  at(p, std::string("<b>") + esc + " \xC2\xB7 " + EN.wifiRefused + "</b>");
  TEST_ASSERT_FALSE(has(p, "<script>\""));
  TEST_ASSERT_FALSE(has(p, "'&a"));
}

// Continue is the one action: the old "Keep the current network and retry"
// is choosing the known network and Continue (wifiRetrySaved)
static void test_setup_and_home_buttons() {
  const std::string setup = page(EN, view(LINK_SETUP));
  const std::string home = page(EN, view(LINK_CONNECTED));
  for (const std::string *p : {&setup, &home}) {
    TEST_ASSERT_EQUAL(1, occurrences(*p, "<button"));
    TEST_ASSERT_FALSE(has(*p, "annuler"));
    at(*p, std::string("<a class=go href=\"/wifi?scan=1\">") + EN.wifiRefresh + "</a>");
    at(*p, "<form method=post action=\"/wifi\" class=sec>");
  }
  // The state rows: the access point, or the address on the home network;
  // the saved network is the known section's, not a row
  at(setup, std::string("<div class=st><span>") + EN.wifiState + "</span><b>" + EN.wifiStateSetup + "</b></div>");
  at(home, std::string("<b>") + EN.wifiStateConnected + " 192.168.1.20</b>");
  at(page(EN, view(LINK_OFFLINE)), std::string("<b>") + EN.wifiStateOffline + "</b>");
  TEST_ASSERT_FALSE(has(home, EN.wifiSaved));
}

// The captive page: the other pages cannot help without a network
static void test_setup_mode_has_no_tabs_and_the_setup_brand() {
  const std::string setup = page(EN, view(LINK_SETUP));
  TEST_ASSERT_FALSE(has(setup, "<nav"));
  at(setup, std::string("<span class=host>") + EN.webSetupBrand + "</span>");
  TEST_ASSERT_FALSE(has(setup, "carousel.local"));
  at(setup, std::string("<title>") + EN.wifiTitle + " \xC2\xB7 Carousel</title>");
  // From Panel on the home network: the tabs, none current, the host
  const std::string home = page(FR, view(LINK_CONNECTED));
  at(home, "<nav aria-label=\"Pages\">");
  TEST_ASSERT_FALSE(has(home, "aria-current"));
  at(home, "<span class=host>carousel.local</span>");
  at(home, std::string("<h1>") + FR.wifiTitle + "</h1><p class=lead>" + FR.wifiSubtitle + "</p>");
}

// A scan takes about 3 s: /wifi waits for it in the web task and answers
// with the list, so no page reloads itself. One not done in time answers an
// empty list that says so, with the Refresh link and the typed name.
static void test_scan_not_done_offers_refresh() {
  WifiPageView v = view(LINK_SETUP);
  TEST_ASSERT_FALSE(has(page(EN, v), EN.wifiNoneYet));
  v.count = 0;
  for (const Strings *t : {&EN, &FR}) {
    const std::string p = page(*t, v);
    TEST_ASSERT_FALSE(has(p, "http-equiv"));
    const size_t fold = at(p, std::string("<details class=nw open><summary>") + t->wifiOthers);
    TEST_ASSERT_TRUE(fold < at(p, t->wifiNoneYet));
    at(p, std::string("<a class=go href=\"/wifi?scan=1\">") + t->wifiRefresh + "</a>");
    at(p, row("", true, true));        // the typed name still works meanwhile
  }
}

// The wait is bounded: the scan's answer as soon as it has one, else
// "still running" after the given number of pauses, never more
static int pollsLeft, pauses;
static int pollResult;
static int fakePoll() { return pollsLeft-- > 0 ? WIFI_SCAN_BUSY : pollResult; }
static void fakePause() { pauses++; }

static void test_scan_wait_is_bounded() {
  pollsLeft = 0; pauses = 0; pollResult = 5;
  TEST_ASSERT_EQUAL(5, wifiAwaitScan(fakePoll, fakePause, 40));
  TEST_ASSERT_EQUAL(0, pauses);              // done already: no wait
  pollsLeft = 3; pauses = 0; pollResult = 0;
  TEST_ASSERT_EQUAL(0, wifiAwaitScan(fakePoll, fakePause, 40));
  TEST_ASSERT_EQUAL(3, pauses);
  pollsLeft = 3; pauses = 0; pollResult = -2;  // a failed scan is an answer too
  TEST_ASSERT_EQUAL(-2, wifiAwaitScan(fakePoll, fakePause, 40));
  pollsLeft = 1000; pauses = 0;
  TEST_ASSERT_EQUAL(WIFI_SCAN_BUSY, wifiAwaitScan(fakePoll, fakePause, 40));
  TEST_ASSERT_EQUAL(40, pauses);
}

// The answer to a save or a retry schedules a restart 2 s on: it neither
// starts a scan nor waits for one, or the restart cuts it off. Others start
// one when none is there or the Refresh link asks, and wait for it.
static void test_scan_plan_never_delays_a_restart() {
  for (int complete : {WIFI_SCAN_NONE, WIFI_SCAN_BUSY, 0, 7}) {
    for (bool asked : {false, true}) {
      const WifiScanPlan p = wifiScanPlan(complete, asked, true);
      TEST_ASSERT_FALSE(p.start);
      TEST_ASSERT_EQUAL(0, p.pauses);
    }
  }
  WifiScanPlan p = wifiScanPlan(WIFI_SCAN_NONE, false, false);
  TEST_ASSERT_TRUE(p.start);
  TEST_ASSERT_EQUAL(WIFI_SCAN_WAIT_PAUSES, p.pauses);
  p = wifiScanPlan(7, true, false);
  TEST_ASSERT_TRUE(p.start);
  p = wifiScanPlan(7, false, false);         // a list is there: kept
  TEST_ASSERT_FALSE(p.start);
  p = wifiScanPlan(WIFI_SCAN_BUSY, false, false);   // one runs: waited for
  TEST_ASSERT_FALSE(p.start);
  TEST_ASSERT_EQUAL(WIFI_SCAN_WAIT_PAUSES, p.pauses);
  // 0 pauses: the scan's state as it is, at once
  pollsLeft = 1000; pauses = 0;
  TEST_ASSERT_EQUAL(WIFI_SCAN_BUSY, wifiAwaitScan(fakePoll, fakePause, 0));
  TEST_ASSERT_EQUAL(0, pauses);
}

static void test_typed_name_wins() {
  TEST_ASSERT_EQUAL_STRING("Typed", wifiChosenName("Typed", "Row"));
  TEST_ASSERT_EQUAL_STRING("Row", wifiChosenName("", "Row"));
  TEST_ASSERT_EQUAL_STRING("Row", wifiChosenName(nullptr, "Row"));
  TEST_ASSERT_EQUAL_STRING("", wifiChosenName("", ""));
  TEST_ASSERT_EQUAL_STRING("", wifiChosenName(nullptr, nullptr));
  // The name field is never filled in advance: what it holds is typed
  const std::string p = page(EN, view(LINK_CONNECTED));
  TEST_ASSERT_FALSE(has(p.substr(at(p, "<input id=on"), 80), "value="));
}

// The confirmation names the network saved, as typed: escaped
static void test_saved_names_the_network() {
  TEST_ASSERT_NOT_NULL(strstr(EN.wifiMsgSaved, "%s"));
  WifiPageView v = view(LINK_SETUP);
  v.message = EN.wifiMsgSaved;
  v.messageName = "<i>Caf\xC3\xA9 & co";
  const std::string p = page(EN, v);
  at(p, "<div class=ok>Saved. The panel restarts and joins &lt;i&gt;Caf\xC3\xA9 &amp; co. "
        "You can disconnect.</div>");
  TEST_ASSERT_FALSE(has(p, "<i>Caf"));
  // The other messages: a refusal is an alert, the retry a plain notice
  v.message = EN.wifiMsgBadSsid;
  v.messageName = nullptr;
  at(page(EN, v), std::string("<div class=er role=alert>") + EN.wifiMsgBadSsid + "</div>");
  v.message = FR.wifiMsgBadPassword;
  at(page(FR, v), std::string("<div class=er role=alert>") + FR.wifiMsgBadPassword + "</div>");
  v.message = EN.wifiMsgRetry;
  at(page(EN, v), std::string("<div class=ok>") + EN.wifiMsgRetry + "</div>");
  // Above the state rows, under the lead
  const std::string r = page(EN, v);
  TEST_ASSERT_TRUE(at(r, "<p class=lead>") < at(r, "<div class=ok>") &&
                   at(r, "<div class=ok>") < at(r, "<div class=st>"));
}

// Scripts on, the phone's time zone gives the first city; off, the form
// posts all the same with the field empty
static void test_time_zone_script_is_optional() {
  const std::string p = page(EN, view(LINK_SETUP));
  const size_t form = at(p, "<form method=post action=\"/wifi\" class=sec>");
  const size_t tz = at(p, "<input type=hidden name=tz id=tz>", form);
  TEST_ASSERT_TRUE(tz < at(p, "</form>", form));
  const size_t script = at(p, "<script>");
  TEST_ASSERT_TRUE(script > at(p, "</footer>"));
  at(p, "document.getElementById('tz').value=Intl.DateTimeFormat().resolvedOptions().timeZone",
     script);
  TEST_ASSERT_EQUAL(1, occurrences(p, "<script"));
  TEST_ASSERT_FALSE(has(p, "onchange="));
  TEST_ASSERT_FALSE(has(p, " required"));
  // The password field follows the chosen row, hidden and disabled when it
  // asks for none; typing a name chooses the name row
  at(p, "getElementsByName('ssid')", script);
  at(p, "hasAttribute('data-pw')", script);
  at(p, ".hidden=", script);
  at(p, ".disabled=", script);
}

// Scripts on, the phone's region and clock preset the units, under the
// names Panel's Region posts; off, the fields post empty, which the save
// ignores, so the defaults stay
static void test_phone_units_script_is_optional() {
  for (WifiLink link : {LINK_SETUP, LINK_CONNECTED}) {
    const std::string p = page(EN, view(link));
    const size_t form = at(p, "<form method=post action=\"/wifi\" class=sec>");
    const size_t tz = at(p, "<input type=hidden name=tz id=tz>", form);
    const size_t units = at(p, "<input type=hidden name=tu id=tu><input type=hidden name=cu id=cu>"
                               "<input type=hidden name=ck id=ck>", form);
    TEST_ASSERT_TRUE(tz < units && units < at(p, "</form>", form));
    TEST_ASSERT_FALSE(has(p, "id=tu value") || has(p, "id=cu value") || has(p, "id=ck value"));
    const size_t script = at(p, "<script>");
    // The mapping is a function of the region code, which webcheck calls
    at(p, "function rg(c){", script);
    at(p, "new Intl.Locale(navigator.language).maximize().region", script);
    at(p, "hourCycle", script);
    at(p, "h=='h11'||h=='h12'", script);
    TEST_ASSERT_EQUAL(1, occurrences(p, "<script"));
  }
}

// Sign-in sends the visitor back to the page asked for, and only to one of
// the six pages: anything else, another site's address above all, is home
static void test_return_path_keeps_six_pages() {
  for (const char *p : {"/", "/screens", "/images", "/panel", "/panel/feedback", "/wifi"})
    TEST_ASSERT_EQUAL_STRING(p, returnPath(p));
  for (const char *p : {"/sauver", "//evil.example", "/screens?x=1", "/SCREENS", "", "http://x/",
                        "/screens/", "/wifi\r\nSet-Cookie: x", "/images/delete", "/panel/", "/panel/feedback/",
                        "/panel/Feedback", "/panel/feedback?x=1", "/panel/other"})
    TEST_ASSERT_EQUAL_STRING_MESSAGE("/", returnPath(p), p);
  TEST_ASSERT_EQUAL_STRING("/", returnPath(nullptr));
  // A screen's page, signed out: back to it once signed in, by its exact
  // address only
  for (const char *p : {"/screens/clock", "/screens/iss-demo", "/screens/weather"})
    TEST_ASSERT_EQUAL_STRING(p, returnPath(p));
  for (const char *p : {"/screens/nope", "/screens/CLOCK", "/screens/clock/", "/screens/clock?x=1",
                        "/screens//clock", "/screens/../panel", "/screens/clock\r\nX: y",
                        "/screensclock"})
    TEST_ASSERT_EQUAL_STRING_MESSAGE("/", returnPath(p), p);
}

static std::string signIn(const Strings &t, bool wrong, const char *next = "/") {
  String s;
  signInPage(s, shell(t), SignInView{wrong, next});
  return s.c_str();
}

static std::string locked(bool knob) {
  String s;
  lockedPage(s, shell(FR), knob);   // the language setting cannot be read: ignored
  return s.c_str();
}

static void test_signin_carries_the_return_path() {
  for (const char *p : {"/", "/screens", "/images", "/panel", "/wifi"}) {
    const std::string page = signIn(EN, false, p);
    const size_t form = at(page, "<form method=post action=\"/login\"");
    const size_t next = at(page, std::string("<input type=hidden name=next value=\"") + p + "\">", form);
    TEST_ASSERT_TRUE(next < at(page, "</form>", form));
  }
  // Whatever web.cpp hands over, the page only carries one of the six
  at(signIn(EN, true, "//evil.example"), "<input type=hidden name=next value=\"/\">");
  at(signIn(FR, false, nullptr), "<input type=hidden name=next value=\"/\">");
}

// No run of six digits anywhere: the code only ever shows on the panel
static bool hasSixDigits(const std::string &s) {
  int run = 0;
  for (char c : s) {
    run = (c >= '0' && c <= '9') ? run + 1 : 0;
    if (run >= 6) return true;
  }
  return false;
}

static void test_signin_stage_block_without_digits() {
  for (const Strings *t : {&EN, &FR})
    for (bool wrong : {false, true}) {
      const std::string p = signIn(*t, wrong, "/panel");
      TEST_ASSERT_FALSE(hasSixDigits(p));
      const size_t stage = at(p, "<section class=stage aria-labelledby=act>");
      // The panel's drawing: the sign-in frame of the poster sheet, twice
      // the size, named by what the panel shows
      char frame[96];
      snprintf(frame, sizeof(frame), "<i class=\"th b\" style=\"--p:%d\" aria-hidden=true></i>",
               POSTER_SIGN_IN);
      const size_t img = at(p, std::string("<div role=img aria-label=\"") + t->signInFrame + "\">" +
                               frame + "</div>", stage);
      const size_t caption = at(p, std::string("<small>") + t->signInCaption + "</small>", img);
      const size_t h1 = at(p, std::string("<h1 id=act>") + t->signInTitle + "</h1>", caption);
      at(p, std::string("<p class=live>") + t->signInLive + "</p>", h1);
      TEST_ASSERT_TRUE(h1 < at(p, "</section>", stage));
      TEST_ASSERT_TRUE(at(p, "</section>", stage) < at(p, "<form"));
      // The hint before the form, the reload hint after it
      TEST_ASSERT_TRUE(at(p, std::string("<p class=lead id=hi>") + t->signInHint + "</p>") <
                       at(p, "<form"));
      TEST_ASSERT_TRUE(at(p, "</form>") < at(p, t->signInReload));
      // Before any session: no tabs
      TEST_ASSERT_FALSE(has(p, "<nav"));
      at(p, std::string("<title>") + t->signInTitle + " \xC2\xB7 Carousel</title>");
    }
}

static void test_signin_field_attributes() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string p = signIn(*t, false);
    const size_t label = at(p, std::string("<label for=c id=cl>") + t->signInCode + "</label>");
    const size_t input = at(p, std::string("<input id=c class=code name=code inputmode=numeric "
                               "autocomplete=one-time-code maxlength=6 pattern=\"[0-9]{6}\" "
                               "required data-d=\"") + t->signInDigit + "\" aria-describedby=\"hi\">", label);
    const size_t button = at(p, std::string("<button class=p style=\"width:100%;margin-top:16px\">") +
                                t->signInButton + "</button></form>", input);
    TEST_ASSERT_TRUE(button > input);
    // The markup, the script aside (it copies these marks when they are there)
    const std::string page = p.substr(0, at(p, "<script>"));
    TEST_ASSERT_FALSE(has(page, "aria-invalid"));
    TEST_ASSERT_FALSE(has(page, "role=alert"));
    TEST_ASSERT_FALSE(has(page, "autofocus"));   // the keyboard opens only to fix a code
  }
}

// Without scripts, the one field as always; with them, the script builds six
// one-digit boxes from it (named "Digit 1 of 6"...), the field hidden and
// filled from them: auto-advance, backspace back, a pasted code fills all,
// the sixth digit submits
static void test_signin_boxes_from_the_field() {
  TEST_ASSERT_EQUAL_STRING("Digit %d of 6", EN.signInDigit);
  TEST_ASSERT_EQUAL_STRING("Chiffre %d sur 6", FR.signInDigit);
  for (bool wrong : {false, true}) {
    const std::string p = signIn(EN, wrong);
    TEST_ASSERT_EQUAL(1, occurrences(p, "<script"));
    const size_t script = at(p, "<script>");
    TEST_ASSERT_TRUE(script > at(p, "</footer>"));
    for (const char *part : {"getElementById('c')", "className='otp'", "'one-time-code'",
                             "dataset.d.replace('%d',", "c.type='hidden'", "'Backspace'",
                             "'paste'", "requestSubmit", "'aria-invalid'", "'aria-labelledby','cl'"})
      at(p, part, script);
    TEST_ASSERT_FALSE(has(p, "onclick="));
  }
}

static void test_signin_wrong_marks_the_field() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string p = signIn(*t, true);
    const size_t form = at(p, "<form method=post action=\"/login\"");
    const size_t error = at(p, std::string("<div class=er role=alert id=\"e1\">") + t->signInWrong +
                               "</div>", form);
    const size_t input = at(p, "<input id=c class=code name=code ", error);
    at(p, std::string("required data-d=\"") + t->signInDigit +
              "\" aria-describedby=\"e1 hi\" aria-invalid=true autofocus>", input);
    TEST_ASSERT_EQUAL(1, occurrences(p, "role=alert"));
  }
}

static void test_locked_variants() {
  const std::string marker = locked(false), knob = locked(true);
  for (const std::string *p : {&marker, &knob}) {
    // In English first, its French half marked so a screen reader reads it
    // in French; the panel's LOCKED words as it shows them
    at(*p, "<html lang=en>");
    char frame[96];
    snprintf(frame, sizeof(frame), "<i class=\"th b\" style=\"--p:%d\" aria-hidden=true></i>",
             POSTER_LOCKED);
    const size_t stage = at(*p, "<section class=stage aria-labelledby=act>");
    const size_t img = at(*p, std::string("<div role=img aria-label=\"LOCKED \xC2\xB7 RESET NOT DONE \xC2\xB7 RESTART IT\">") + frame, stage);
    const size_t h1 = at(*p, "<h1 id=act>LOCKED \xC2\xB7 RESET NOT DONE \xC2\xB7 RESTART IT</h1>", img);
    // Both languages on one line, the French half marked for a screen reader
    at(*p, std::string("<p class=live>") + EN.lockedLive + " \xC2\xB7 <span lang=fr>" +
           FR.lockedLive + "</span></p>", h1);
    const size_t en = at(*p, std::string("<section class=sec aria-labelledby=h-e><h2 id=h-e>") +
                             EN.resetFailedTitle + "</h2><p>" +
                             resetFailedText(EN, p == &knob) + "</p>", h1);
    const size_t fr = at(*p, std::string("<section class=sec lang=fr aria-labelledby=h-f><h2 id=h-f>") +
                             FR.resetFailedTitle + "</h2><p>" +
                             resetFailedText(FR, p == &knob) + "</p>", en);
    TEST_ASSERT_TRUE(fr < at(*p, "<footer>"));
    at(*p, "<title>Factory reset unfinished / Remise \xC3\xA0 z\xC3\xA9ro inachev\xC3\xA9" "e \xC2\xB7 "
           "Carousel</title>");
  }
  // A marker: the next start resumes the reset, and its text ends on the
  // reinstall, so each half links it. A knob reset asks for the knob instead.
  String linkEn, linkFr;
  reinstallLink(linkEn, EN);
  reinstallLink(linkFr, FR);
  const size_t fr = at(marker, "<section class=sec lang=fr");
  TEST_ASSERT_TRUE(at(marker, linkEn.c_str()) < fr);
  TEST_ASSERT_TRUE(at(marker, linkFr.c_str(), fr) > fr);
  TEST_ASSERT_FALSE(has(knob, "build-and-flash"));
  TEST_ASSERT_TRUE(has(knob, EN.webResetFailedKnob));
}

static void test_locked_page_has_no_form_and_no_tabs() {
  for (bool knob : {false, true}) {
    const std::string p = locked(knob);
    TEST_ASSERT_FALSE(has(p, "<form"));
    TEST_ASSERT_FALSE(has(p, "<button"));
    TEST_ASSERT_FALSE(has(p, "<input"));
    TEST_ASSERT_FALSE(has(p, "<nav"));
    TEST_ASSERT_FALSE(has(p, "<script"));
    at(p, "<span class=host>carousel.local</span>");
  }
}

static void test_one_h1_one_main() {
  for (WifiLink link : {LINK_SETUP, LINK_CONNECTED, LINK_OFFLINE}) {
    const std::string p = page(FR, view(link));
    TEST_ASSERT_EQUAL(1, occurrences(p, "<h1"));
    TEST_ASSERT_EQUAL(1, occurrences(p, "<main"));
    TEST_ASSERT_TRUE(at(p, "<main") < at(p, "<h1"));
  }
  for (const std::string &p : {signIn(EN, false), signIn(FR, true), locked(false), locked(true)}) {
    TEST_ASSERT_EQUAL(1, occurrences(p, "<h1"));
    TEST_ASSERT_EQUAL(1, occurrences(p, "<main"));
    TEST_ASSERT_TRUE(at(p, "<main") < at(p, "<h1"));
  }
}

static std::string error(const Strings &t, int code, Tab back = TAB_STAGE, bool tabs = false) {
  String s;
  Shell sh = shell(t);
  sh.tabs = tabs;
  errorPage(s, sh, code, back);
  return s.c_str();
}

// Each status a browser route answers names its words; one word for the
// two that mean "the form did not arrive as sent"
static void test_error_kind_by_status() {
  TEST_ASSERT_EQUAL(ERR_NOT_FOUND, errorKind(404));
  TEST_ASSERT_EQUAL(ERR_REFUSED, errorKind(403));
  TEST_ASSERT_EQUAL(ERR_REFUSED, errorKind(401));
  TEST_ASSERT_EQUAL(ERR_TOO_LARGE, errorKind(413));
  TEST_ASSERT_EQUAL(ERR_BAD_FORM, errorKind(400));
  TEST_ASSERT_EQUAL(ERR_BAD_FORM, errorKind(415));
  TEST_ASSERT_EQUAL(ERR_BUSY, errorKind(503));
  TEST_ASSERT_EQUAL(ERR_BUSY, errorKind(500));
}

// The shell, a stage block with the kind's title as the one h1, the status
// on the live line, its sentence, and a link back to the page named
static void test_error_page_in_the_shell() {
  for (const Strings *t : {&EN, &FR}) {
    for (int code : {400, 403, 404, 413, 415, 503}) {
      const ErrorKind k = errorKind(code);
      const std::string p = error(*t, code);
      const size_t h1 = at(p, std::string("<h1 id=act>") + t->errTitle[k] + "</h1>");
      at(p, "<section class=stage aria-labelledby=act>");
      char live[32];
      snprintf(live, sizeof(live), "<p class=live>%d</p>", code);
      at(p, live, h1);
      at(p, std::string("<p>") + t->errText[k] + "</p>", h1);
      String back;
      formatText(back, t->errBack, t->webTabs[TAB_STAGE]);
      at(p, std::string("<a class=go href=\"/\">") + back.c_str() + "</a>", h1);
      String title;
      formatText(title, t->webTitle, t->errTitle[k]);
      at(p, std::string("<title>") + title.c_str() + "</title>");
      TEST_ASSERT_EQUAL(1, occurrences(p, "<h1"));
      TEST_ASSERT_EQUAL(1, occurrences(p, "<main"));
      TEST_ASSERT_EQUAL(0, occurrences(p, "<form"));
      TEST_ASSERT_TRUE(p.size() <= ERROR_PAGE_RESERVE);
    }
  }
  String back;
  formatText(back, EN.errBack, EN.webTabs[TAB_IMAGES]);
  at(error(EN, 413, TAB_IMAGES), std::string("<a class=go href=\"/images\">") + back.c_str() + "</a>");
}

// No tabs before sign-in (the brand points at the content); a signed-in
// visitor keeps them, none current
static void test_error_page_tabs_only_signed_in() {
  const std::string out = error(EN, 404);
  TEST_ASSERT_EQUAL(0, occurrences(out, "<nav"));
  at(out, "<a class=brand href=\"#m\">");
  const std::string in = error(EN, 503, TAB_STAGE, true);
  TEST_ASSERT_EQUAL(1, occurrences(in, "<nav"));
  TEST_ASSERT_EQUAL(0, occurrences(in, "aria-current"));
}

// The worst case: 30 rows of 32-byte names, every row checked against the
// saved one, the last try refused, a message, in French
static void test_page_fits_24k() {
  WifiNet rows[WIFI_ROWS_MAX];
  for (int i = 0; i < WIFI_ROWS_MAX; i++) {
    char name[33];
    snprintf(name, sizeof(name), "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&%03d", i);   // 32 bytes, 5x escaped
    rows[i] = net(name, (int16_t)(-30 - i), i % 2);
  }
  WifiPageView v = view(LINK_SETUP, rows[29].ssid);
  v.rows = rows;
  v.count = WIFI_ROWS_MAX;
  v.lastTry = TRY_NOT_FOUND;
  v.message = FR.wifiMsgSaved;
  v.messageName = rows[29].ssid;
  const std::string p = page(FR, v);
  char msg[48];
  snprintf(msg, sizeof(msg), "%zu bytes", p.size());
  TEST_ASSERT_TRUE_MESSAGE(p.size() <= 24576, msg);
  TEST_ASSERT_EQUAL(WIFI_ROWS_MAX + 1, occurrences(p, "<label class=op>"));

  // What web.cpp reserves: the page without rows in either language, and
  // each row of a plain 32-byte name, open
  for (const Strings *t : {&EN, &FR}) {
    WifiPageView none = v;
    none.count = 0;
    none.message = t->wifiMsgSaved;
    const size_t base = page(*t, none).size();
    snprintf(msg, sizeof(msg), "%s: %zu bytes without rows", t->code, base);
    TEST_ASSERT_TRUE_MESSAGE(base <= WIFI_PAGE_BASE, msg);
    const WifiNet plain[] = {net("abcdefghijklmnopqrstuvwxyz012345", -100, true)};
    none.rows = plain;
    none.count = 1;
    const size_t one = page(*t, none).size() - base;
    snprintf(msg, sizeof(msg), "%s: %zu bytes a row", t->code, one);
    TEST_ASSERT_TRUE_MESSAGE(one <= WIFI_ROW_BYTES, msg);
  }

  // Sign-in and locked: fixed text, within what web.cpp reserves for them
  for (const std::string &p : {signIn(EN, true, "/screens"), signIn(FR, true, "/screens"),
                               locked(false), locked(true)}) {
    snprintf(msg, sizeof(msg), "%zu bytes", p.size());
    TEST_ASSERT_TRUE_MESSAGE(p.size() <= ACCESS_PAGE_RESERVE, msg);
  }
  TEST_ASSERT_TRUE(ACCESS_PAGE_RESERVE <= 24576);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_rows_sorted_deduped_capped);
  RUN_TEST(test_known_network_first);
  RUN_TEST(test_signal_in_words_and_bars);
  RUN_TEST(test_one_password_field);
  RUN_TEST(test_names_escaped);
  RUN_TEST(test_setup_and_home_buttons);
  RUN_TEST(test_setup_mode_has_no_tabs_and_the_setup_brand);
  RUN_TEST(test_scan_not_done_offers_refresh);
  RUN_TEST(test_scan_wait_is_bounded);
  RUN_TEST(test_scan_plan_never_delays_a_restart);
  RUN_TEST(test_typed_name_wins);
  RUN_TEST(test_saved_names_the_network);
  RUN_TEST(test_time_zone_script_is_optional);
  RUN_TEST(test_phone_units_script_is_optional);
  RUN_TEST(test_return_path_keeps_six_pages);
  RUN_TEST(test_signin_carries_the_return_path);
  RUN_TEST(test_signin_stage_block_without_digits);
  RUN_TEST(test_signin_field_attributes);
  RUN_TEST(test_signin_boxes_from_the_field);
  RUN_TEST(test_signin_wrong_marks_the_field);
  RUN_TEST(test_locked_variants);
  RUN_TEST(test_locked_page_has_no_form_and_no_tabs);
  RUN_TEST(test_error_kind_by_status);
  RUN_TEST(test_error_page_in_the_shell);
  RUN_TEST(test_error_page_tabs_only_signed_in);
  RUN_TEST(test_one_h1_one_main);
  RUN_TEST(test_page_fits_24k);
  return UNITY_END();
}
