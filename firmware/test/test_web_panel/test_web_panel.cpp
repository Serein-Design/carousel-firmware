// SPDX-License-Identifier: AGPL-3.0-only
// Panel without the web server: its sections in order, the language select
// labelled, the night fields, each update state, the signal's word, the
// MQTT password never sent, Advanced folded with its four titles, the
// token and the sign-out, memory and the last restart, the form carrying
// none of Running order's fields, the save's scope and result, and the page
// within its pieces and its 24 KB.
//
//   pio test -e native -f test_web_panel
#include <unity.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include <Preferences.h>

#include "../../src/i18n.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/content_logic.cpp"
#include "../../src/web_shell.cpp"
#include "../../src/web_stage.cpp"
#include "../../src/web_screens.cpp"
#include "../../src/posters.cpp"
#include "../../src/posters_logic.cpp"
#include "../../src/web_panel.cpp"

// settings.cpp's lock, read by content_logic's choice
bool panelLocked() { return false; }

void setUp() {}
void tearDown() {}

// --- Helpers -------------------------------------------------------------------

static const Strings &EN = STRINGS[LANG_EN];
static const Strings &FR = STRINGS[LANG_FR];

static std::string sent;
static size_t largest = 0;
static int flushes = 0;
static void collect(String &s) {
  sent += s.c_str();
  largest = std::max(largest, (size_t)s.length());
  flushes++;
  s = String();
}

static const Settings DEFAULTS;

// Stored settings shown, up to date, on the home network at -58 dBm, MQTT
// up, two sessions, three hours since a power-on
static PanelView view(const Settings &values = DEFAULTS, const Settings &stored = DEFAULTS) {
  PanelView v = {};
  v.values = &values;
  v.stored = &stored;
  v.result = SAVE_NONE;
  v.refusal = LIST_OK;
  v.update = UPD_UP_TO_DATE;
  v.latest = "";
  v.updateError = "";
  v.ssid = "Maison-2.4G";
  v.rssi = -58;
  v.connected = true;
  v.mqtt = MQTT_UP;
  v.sessionCount = 2;
  v.ip = "192.168.1.50";
  v.token = "c7f2a91e5b0d4836af1e27c9d04b6e13";
  v.hookToken = "5e0b7d19a4c3482f9d61b8a2e7f40c35";
  v.hookOpen = false;
  v.hookRefusals = 0;
  v.freeKb = 142;
  v.minFreeKb = 98;
  v.resetReason = "power on";
  v.upSec = 3 * 3600 + 120;
  return v;
}

static std::string render(const Strings &t, const PanelView &v) {
  sent.clear();
  largest = 0;
  flushes = 0;
  const Shell sh = {&t, nullptr, "carousel.local", TAB_NONE, false, "0.0.0-test"};
  String s;
  s.reserve(PANEL_PAGE_RESERVE);
  panelPage(s, sh, v, collect);
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

static std::string aside(const std::string &s) {
  const size_t a = at(s, "<aside>");
  return s.substr(a, at(s, "</aside>", a) - a);
}

// The section from its id to its end
static std::string section(const std::string &s, const char *id) {
  const size_t a = at(s, std::string("<section id=") + id + " ");
  return s.substr(a, at(s, "</section>", a) - a);
}

// One folded row of Advanced, from its title on
static std::string folded(const std::string &s, const std::string &title) {
  const size_t a = at(s, "<summary>" + title);
  return s.substr(a, at(s, "</details>", a) - a);
}

static std::string mqttTitle(const Strings &t, const char *state) {
  String s;
  formatText(s, t.pnlMqttTitle, state);
  return s.c_str();
}

// --- Layout --------------------------------------------------------------------

// Lead, index and save in the side column; then Language, Region, Night mode,
// Feedback, Updates, Wi-Fi and Advanced; the save again at the end
static void test_panel_sections_in_order() {
  const std::string s = render(EN, view());
  const std::string side = aside(s);
  size_t p = at(side, std::string("<h1>") + EN.webTabs[TAB_PANEL] + "</h1>");
  p = at(side, std::string("<p class=lead>") + EN.pnlLead + "</p>", p);
  p = at(side, "<ul class=idx>", p);
  const char *const ids[] = {"lang", "region", "night", "fb", "upd", "net", "adv"};
  for (const char *id : ids) p = at(side, std::string("<a href=\"#") + id + "\">", p);
  at(side, "<div class=\"sv dk\">", p);
  p = at(s, "</aside>");
  for (const char *id : ids) p = at(s, std::string("<section id=") + id + " class=sec", p);
  const std::string adv = section(s, "adv");
  size_t q = at(adv, "<summary>" + mqttTitle(EN, EN.webMqttOn) + "</summary>");
  q = at(adv, "<details id=hooks", q);
  q = at(adv, std::string("<summary>") + EN.pnlApiTitle + "</summary>", q);
  at(adv, std::string("<summary>") + EN.pnlMemoryTitle + "</summary>", q);
  at(s, "<div class=\"sv mb\">", at(s, "<section id=adv "));
  // Each heading names its section; the index says the same words
  const struct { const char *id; const char *title; } heads[] = {
    {"lang", EN.webLanguage}, {"region", EN.pnlRegion}, {"night", EN.webNightMode},
    {"fb", EN.pnlFeedback}, {"upd", EN.pnlUpdates}, {"net", EN.pnlWifi}, {"adv", EN.pnlAdvanced},
  };
  for (int i = 0; i < 7; i++) {
    char h[96];
    snprintf(h, sizeof(h), "<section id=%s class=sec aria-labelledby=h-%d><h2 id=h-%d>%s</h2>",
             heads[i].id, i + 1, i + 1, heads[i].title);
    at(s, h);
    at(side, std::string("<a href=\"#") + heads[i].id + "\">" + heads[i].title + "</a>");
  }
  // The four forms first, empty, before the columns
  p = at(s, "<form id=sv method=post action=\"/sauver\"><input type=hidden name=pg value=panel>"
            "<button hidden>");
  p = at(s, "<form id=uf method=post action=\"/update\"></form>", p);
  p = at(s, "<form id=pf method=post action=\"/purge\"></form>", p);
  p = at(s, "<form id=so method=post action=\"/signout\"></form>", p);
  p = at(s, "<form id=hk method=post action=\"/hooks\"></form>", p);
  TEST_ASSERT_TRUE(p < at(s, "<div class=two>"));
  TEST_ASSERT_EQUAL(5, occurrences(s, "<form"));
  TEST_ASSERT_TRUE(has(s, "<a href=\"/panel\" aria-current=page>"));
}

static void test_one_h1_one_main() {
  const std::string s = render(FR, view());
  TEST_ASSERT_EQUAL(1, occurrences(s, "<h1"));
  TEST_ASSERT_EQUAL(1, occurrences(s, "<main"));
  TEST_ASSERT_TRUE(at(s, "<main") < at(s, std::string("<h1>") + FR.webTabs[TAB_PANEL] + "</h1>"));
  TEST_ASSERT_EQUAL(0, (int)at(s, "<!doctype html>"));
  TEST_ASSERT_TRUE(s.compare(s.size() - 7, 7, "</html>") == 0);
}

// --- Sections ------------------------------------------------------------------

// A label names the select; the stored language is chosen, each option
// says its own language
static void test_language_select_labelled() {
  const std::string en = section(render(EN, view()), "lang");
  at(en, std::string("<label for=lg>") + EN.pnlLanguageLabel + "</label><select id=lg name=lang "
         "form=sv>");
  at(en, "<option value=0 lang=en selected>English</option>");
  at(en, std::string("<option value=1 lang=fr>") + STRINGS[LANG_FR].name + "</option>");
  Settings fr;
  fr.lang = LANG_FR;
  const std::string f = section(render(FR, view(fr, fr)), "lang");
  at(f, std::string("<label for=lg>") + FR.pnlLanguageLabel + "</label>");
  at(f, "<option value=0 lang=en>");
  at(f, "<option value=1 lang=fr selected>");
}

static void test_night_fields() {
  Settings st;
  st.nightStart = 23;
  st.nightEnd = 7;
  st.nightBrightness = 25;
  const std::string s = section(render(EN, view(st, st)), "night");
  at(s, std::string("<label for=nde>") + EN.webNightFrom + "</label><input id=nde name=nde "
        "form=sv value=\"23\" inputmode=numeric>");
  at(s, std::string("<label for=nto>") + EN.webNightTo + "</label><input id=nto name=nto "
        "form=sv value=\"7\" inputmode=numeric>");
  size_t p = at(s, std::string("<label for=nlv>") + EN.webNightLevel + "</label><select id=nlv "
                   "name=nlv form=sv>");
  p = at(s, std::string("<option value=0>") + EN.webNightOff + "</option>", p);
  p = at(s, "<option value=3>2\xC2\xA0%</option>", p);
  p = at(s, "<option value=10>4\xC2\xA0%</option>", p);
  p = at(s, "<option value=25 selected>10\xC2\xA0%</option>", p);
  at(s, "<option value=50>20\xC2\xA0%</option>", p);
  at(s, std::string("<small>") + EN.webNightHint + "</small>");
  TEST_ASSERT_EQUAL(1, occurrences(s, " selected"));
}

// The firmware row with the last check's word; Install only while an
// update is available; the version and the error escaped
static void test_update_states() {
  const char *const fw = "<div class=st><span>Firmware</span><b class=num>0.0.0-test";
  const struct { UpdateState st; std::string tail; } cases[] = {
    {UPD_IDLE, "</b>"},
    {UPD_CHECKING, std::string(" \xC2\xB7 ") + EN.updChecking + "</b>"},
    {UPD_UP_TO_DATE, std::string(" \xC2\xB7 ") + EN.updUpToDate + "</b>"},
    {UPD_AVAILABLE, std::string(" \xC2\xB7 &lt;0.2&quot;&gt; ") + EN.updAvailable + "</b>"},
    {UPD_INSTALLING, std::string(" \xC2\xB7 ") + EN.updInstalling + "</b>"},
    {UPD_FAILED, " \xC2\xB7 no &lt;network&gt;</b>"},
  };
  for (const auto &c : cases) {
    PanelView v = view();
    v.update = c.st;
    v.latest = "<0.2\">";
    v.updateError = "no <network>";
    const std::string s = section(render(EN, v), "upd");
    at(s, fw + c.tail);
    at(s, std::string("<input type=hidden name=auh value=1 form=sv><label class=ck><input "
                      "type=checkbox name=au value=1 form=sv checked>") + EN.updAuto + "</label>");
    at(s, std::string("<button form=uf name=do value=check style=\"margin-top:12px\">") +
              EN.updCheck + "</button>");
    TEST_ASSERT_EQUAL(c.st == UPD_AVAILABLE, has(s, "value=install"));
    if (c.st == UPD_AVAILABLE)
      at(s, std::string("<button form=uf name=do value=install class=p>") + EN.updInstall +
                " &lt;0.2&quot;&gt;</button>");
    TEST_ASSERT_FALSE(has(s, "<0.2"));
  }
  Settings off;
  off.autoUpdate = false;
  TEST_ASSERT_FALSE(has(section(render(EN, view(off, off)), "upd"), "checked"));
}

// good from -67 dBm, fair from -75, weak below; not connected: "--"
static void test_signal_words() {
  TEST_ASSERT_EQUAL(SIGNAL_GOOD, signalWord(-30));
  TEST_ASSERT_EQUAL(SIGNAL_GOOD, signalWord(-67));
  TEST_ASSERT_EQUAL(SIGNAL_FAIR, signalWord(-68));
  TEST_ASSERT_EQUAL(SIGNAL_FAIR, signalWord(-75));
  TEST_ASSERT_EQUAL(SIGNAL_WEAK, signalWord(-76));
  TEST_ASSERT_EQUAL(SIGNAL_WEAK, signalWord(-95));
  const struct { int dbm; const char *word; } rows[] = {
    {-67, "good"}, {-68, "fair"}, {-75, "fair"}, {-76, "weak"},
  };
  for (const auto &r : rows) {
    PanelView v = view();
    v.rssi = r.dbm;
    char row[96];
    snprintf(row, sizeof(row), "<b class=num>%d\xC2\xA0" "dBm \xC2\xB7 %s</b>", r.dbm, r.word);
    at(section(render(EN, v), "net"), row);
  }
  PanelView v = view();
  v.rssi = -76;
  at(section(render(FR, v), "net"), std::string("-76\xC2\xA0" "dBm \xC2\xB7 ") + FR.pnlSignal[2]);
  v.connected = false;
  const std::string off = section(render(EN, v), "net");
  at(off, std::string("<span>") + EN.webSignal + "</span><b class=num>--</b>");
  TEST_ASSERT_FALSE(has(off, "dBm \xC2\xB7"));
  // The saved network, escaped, the hint and the way to change it
  v = view();
  v.ssid = "<Caf\xC3\xA9 \"5G\">";
  const std::string net = section(render(EN, v), "net");
  at(net, std::string("<span>") + EN.wifiSaved + "</span><b>&lt;Caf\xC3\xA9 &quot;5G&quot;&gt;</b>");
  at(net, std::string("<small>") + EN.webSignalHint + "</small>");
  at(net, std::string("<a class=go href=\"/wifi\">") + EN.webChangeWifi + "</a>");
  v.ssid = "";
  at(section(render(EN, v), "net"), std::string("<b>") + EN.wifiNoneYet + "</b>");
}

// Never sent back: the field is empty whatever is stored or posted; the
// "one is stored" mark comes from what is stored only
static void test_mqtt_password_never_sent() {
  Settings stored;
  snprintf(stored.mqttHost, sizeof(stored.mqttHost), "homeassistant.local");
  snprintf(stored.mqttUser, sizeof(stored.mqttUser), "carousel");
  snprintf(stored.mqttPass, sizeof(stored.mqttPass), "hunter2-stored");
  Settings posted = stored;
  snprintf(posted.mqttPass, sizeof(posted.mqttPass), "typed-in-a-failed-save");
  PanelView v = view(posted, stored);
  v.result = SAVE_FAILED;
  for (const PanelView &w : {view(stored, stored), v}) {
    const std::string s = render(EN, w);
    TEST_ASSERT_FALSE(has(s, "hunter2"));
    TEST_ASSERT_FALSE(has(s, "typed-in"));
    at(s, std::string("<input id=mqw name=mqw form=sv value=\"\" ") + mqttPassAttrs(stored) + ">");
    TEST_ASSERT_TRUE(has(s, "&bull;"));
    at(s, "<input id=mqh name=mqh form=sv value=\"homeassistant.local\" maxlength=63");
    at(s, "<input id=mqu name=mqu form=sv value=\"carousel\" maxlength=31 autocomplete=off>");
    at(s, "<input id=mqp name=mqp form=sv value=\"1883\" inputmode=numeric>");
  }
  // Nothing stored, a password typed in a failed save: no mark either
  Settings none;
  Settings typed = none;
  snprintf(typed.mqttPass, sizeof(typed.mqttPass), "typed-in-a-failed-save");
  PanelView w = view(typed, none);
  w.result = SAVE_FAILED;
  const std::string s = render(EN, w);
  TEST_ASSERT_FALSE(has(s, "typed-in"));
  TEST_ASSERT_FALSE(has(s, "&bull;"));
  // The broker's state in the title
  const struct { MqttLink l; const char *word; } links[] = {
    {MQTT_OFF, EN.webMqttOff}, {MQTT_UP, EN.webMqttOn}, {MQTT_DOWN, EN.webMqttDown},
  };
  for (const auto &l : links) {
    PanelView x = view();
    x.mqtt = l.l;
    at(render(EN, x), "<summary>" + mqttTitle(EN, l.word) + "</summary>");
  }
}

static void test_hooks_title_counts() {
  const struct { int n; std::string title; } cases[] = {
    {0, EN.pnlHooksNone}, {1, EN.pnlHooksOne}, {6, "Coding agent hooks: 6 sessions"},
  };
  for (const auto &c : cases) {
    PanelView v = view();
    v.sessionCount = c.n;
    const std::string s = render(EN, v);
    const std::string h = folded(s, c.title + "</summary>");
    at(h, std::string("<span>") + EN.webHookAddress + "</span><b><code>http://192.168.1.50/agent/"
              "&lt;state&gt;</code></b>");
    at(h, std::string("<small>") + EN.webStates + " idle, thinking, tool, attention, error, done, "
              "failed, end. " + EN.pnlHookLegacy + "</small>");
    at(h, std::string("<small>") + EN.pnlHooksHint + "</small>");
    TEST_ASSERT_FALSE(has(h, "/claude/&lt;"));
    TEST_ASSERT_FALSE(has(h, "veille"));
    // The list is cleared only when there is one, as before
    TEST_ASSERT_EQUAL(c.n > 0, has(h, std::string("<button form=pf style=\"margin-top:12px\">") +
                                          EN.webClearSessions + "</button>"));
  }
  PanelView v = view();
  v.sessionCount = 6;
  at(render(FR, v), "<summary>Hooks des agents de code\xE2\x80\xAF: 6 sessions</summary>");
}

// --- The hooks fold: the install command, the token settings --------------------

static const char *const HOOK_TOKEN = "5e0b7d19a4c3482f9d61b8a2e7f40c35";

static std::string hooksFoldOf(const Strings &t, const PanelView &v) {
  const std::string s = render(t, v);
  const size_t a = at(s, "<details id=hooks");
  return s.substr(a, at(s, "</details>", a) - a);
}

// The command in a labelled read-only field, double-quoted all through, with
// the panel's address and the hook token; the title names no one product
static void test_hooks_install_command() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string h = hooksFoldOf(*t, view());
    at(h, std::string("<label for=hkc>") + t->pnlHookInstall + "</label>");
    at(h, std::string("<input id=hkc readonly spellcheck=false autocomplete=off value=\"curl -sS "
                      "--compressed --noproxy &#39;*&#39; &quot;http://192.168.1.50/install?k=") + HOOK_TOKEN +
              "&quot; | sh\">");
    // The API token and the hook token are two secrets
    TEST_ASSERT_FALSE(has(h, view().token));
    TEST_ASSERT_EQUAL(1, occurrences(h, HOOK_TOKEN));
    at(h, std::string("<small>") + t->pnlHookInstallHint + "</small>");
    at(h, std::string("<small>") + t->pnlHookUninstall + " <code>sh ~/.config/carousel/setup.sh "
              "uninstall</code></small>");
  }
  TEST_ASSERT_FALSE(has(render(EN, view()), "Claude Code hooks"));
  TEST_ASSERT_FALSE(has(render(FR, view()), "Hooks Claude Code"));
}

// Copy is hidden: only the script shows it. It carries both answers
static void test_hooks_copy_button_hidden() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string s = render(*t, view());
    const size_t f = at(s, "<input id=hkc ");
    const size_t b = at(s, std::string("<button type=button id=hkb form=sv hidden data-ok=\"") + t->pnlCopied +
                              "\" data-no=\"" + t->pnlCopyByHand + "\" style=\"margin-top:12px\">" + t->pnlCopy + "</button>", f);
    // The script after it finds both by their ids and unhides the button
    const size_t sc = at(s, "<script>", b);
    at(s, "hkc", sc);
    at(s, "hkb", sc);
    at(s, "execCommand('copy')", sc);
    at(s, ".hidden=false", sc);
  }
}

static void test_hooks_open_setting() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string closed = hooksFoldOf(*t, view());
    at(closed, std::string(t->pnlHookOpenOff));
    at(closed, std::string("<button form=hk name=do value=accept style=\"margin-top:12px\">") +
                   t->pnlHookAccept + "</button>");
    TEST_ASSERT_FALSE(has(closed, t->pnlHookOpenOn));
    TEST_ASSERT_FALSE(has(closed, t->pnlHookOpenWarn));
    TEST_ASSERT_FALSE(has(closed, "value=refuse"));
    PanelView v = view();
    v.hookOpen = true;
    const std::string open = hooksFoldOf(*t, v);
    at(open, std::string(t->pnlHookOpenOn));
    at(open, std::string("<div class=er style=\"margin-top:12px\">") + t->pnlHookOpenWarn + "</div>");
    at(open, std::string("<button form=hk name=do value=refuse style=\"margin-top:12px\">") +
                 t->pnlHookRefuse + "</button>");
    TEST_ASSERT_FALSE(has(open, t->pnlHookOpenOff));
    TEST_ASSERT_FALSE(has(open, "value=accept"));
  }
}

static void test_hooks_renew() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string h = hooksFoldOf(*t, view());
    at(h, std::string("<button form=hk name=do value=renew style=\"margin-top:12px\">") +
              t->pnlHookRenew + "</button>");
    at(h, std::string("<small>") + t->pnlHookRenewHint + "</small>");
  }
}

// No line at zero; at three, the count
static void test_hooks_refusal_line() {
  for (const Strings *t : {&EN, &FR}) {
    TEST_ASSERT_FALSE(has(hooksFoldOf(*t, view()), "hkr"));
    PanelView v = view();
    v.hookRefusals = 3;
    const std::string h = hooksFoldOf(*t, v);
    String line;
    formatText(line, t->pnlHookRefused, "3");
    TEST_ASSERT_TRUE(has(line.c_str(), "3"));
    at(h, std::string("<small id=hkr>") + line.c_str() + "</small>");
    // After the install lines, before the setting
    TEST_ASSERT_TRUE(at(h, "id=hkr") > at(h, "uninstall"));
    TEST_ASSERT_TRUE(at(h, "id=hkr") < at(h, "value=accept"));
  }
}

// What the firmware did not write is escaped, and quoted
static void test_hooks_values_escaped() {
  PanelView v = view();
  v.ip = "1.2.3.4\" onfocus=\"x";
  v.hookToken = "ab<c>'d";
  const std::string h = hooksFoldOf(EN, v);
  TEST_ASSERT_FALSE(has(h, "onfocus=\"x"));
  at(h, "http://1.2.3.4&quot; onfocus=&quot;x/install?k=ab&lt;c&gt;&#39;d&quot; | sh\"");
  at(h, "<code>http://1.2.3.4&quot; onfocus=&quot;x/agent/&lt;state&gt;</code>");
}

// Every attribute the fold fills from a string or a value is double-quoted
static void test_hooks_attributes_double_quoted() {
  const std::string h = hooksFoldOf(FR, view());
  for (const char *a : {" value=\"", " data-ok=\"", " data-no=\""}) at(h, a);
  TEST_ASSERT_FALSE(has(h, " data-ok=C"));
  TEST_ASSERT_FALSE(has(h, " data-no=S"));
  // The input's value: the only one the fold leaves to a string
  TEST_ASSERT_EQUAL(1, occurrences(h, "readonly spellcheck=false autocomplete=off value=\""));
}

// Four folded rows, none open: Home Assistant, the hooks, the token, memory
static void test_advanced_folded() {
  const std::string adv = section(render(EN, view()), "adv");
  at(adv, std::string("<h2 id=h-7>") + EN.pnlAdvanced + "</h2><ul class=list>");
  TEST_ASSERT_EQUAL(4, occurrences(adv, "<li><details"));
  TEST_ASSERT_EQUAL(4, occurrences(adv, "<summary>"));
  TEST_ASSERT_FALSE(has(adv, " open"));
  const std::string titles[] = {mqttTitle(EN, EN.webMqttOn), "Coding agent hooks: 2 sessions", EN.pnlApiTitle,
                                EN.pnlMemoryTitle};
  size_t p = 0;
  for (const std::string &t : titles) p = at(adv, "<summary>" + t + "</summary><div class=sd>", p);
}

// The token (escaped all the same), its hint, and the sign-out outside the
// save form
static void test_token_and_sign_out() {
  PanelView v = view();
  v.token = "c7f2<a>";
  const std::string s = render(EN, v);
  const std::string api = folded(s, std::string(EN.pnlApiTitle) + "</summary>");
  at(api, std::string("<span>") + EN.pnlToken + "</span><b><code>c7f2&lt;a&gt;</code></b>");
  String hint = escapeHtml(EN.webApiHint);
  at(api, std::string("<small>") + hint.c_str() + "</small>");
  TEST_ASSERT_TRUE(has(api, "Bearer &lt;token&gt;"));
  at(api, std::string("<button form=so class=del style=\"margin-top:12px\">") + EN.signOutAll +
              "</button>");
}

static void test_memory_and_restart() {
  const std::string m = folded(render(EN, view()), std::string(EN.pnlMemoryTitle) + "</summary>");
  at(m, std::string("<span>") + EN.webFreeMemory + "</span><b class=num>142\xC2\xA0KB (min "
            "98\xC2\xA0KB)</b>");
  at(m, std::string("<span>") + EN.webLastRestart + "</span><b>power on, 3\xC2\xA0h ago</b>");
  at(m, std::string("<small>") + EN.pnlRestartHint + "</small>");
  PanelView v = view();
  v.upSec = 42;
  v.resetReason = "<watchdog>";
  const std::string f = folded(render(FR, v), std::string(FR.pnlMemoryTitle) + "</summary>");
  at(f, "142\xC2\xA0Ko (plancher 98\xC2\xA0Ko)");
  at(f, "<b>&lt;watchdog&gt;, il y a 42\xC2\xA0s</b>");
}

// A Panel save stores only Panel's fields: none of Running order's is in
// its page, and its form says where it came from
static void test_panel_form_carries_no_screen_field() {
  PanelView v = view();
  v.result = SAVE_FAILED;
  for (const std::string &s : {render(EN, view()), render(FR, v)}) {
    for (const char *f : {"o", "on", "mv", "rt", "msg", "msg0", "city", "sal", "hd", "hf", "jm",
                          "ytk", "ytc", "gh", "ig", "xa", "cs", "ks", "kw", "kwh", "ws"}) {
      TEST_ASSERT_FALSE_MESSAGE(has(s, std::string("name=") + f + " "), f);
      TEST_ASSERT_FALSE_MESSAGE(has(s, std::string("name=") + f + ">"), f);
      TEST_ASSERT_FALSE_MESSAGE(has(s, std::string("name=\"") + f + "\""), f);
    }
    TEST_ASSERT_EQUAL(1, occurrences(s, "name=pg value=panel>"));
    // Every control of the save joins form sv
    for (const char *f : {"lang", "nde", "nto", "nlv", "auh", "au", "mqh", "mqp", "mqu", "mqw"})
      TEST_ASSERT_TRUE_MESSAGE(has(s, std::string("name=") + f + " form=sv") ||
                               has(s, std::string("name=") + f + " value=1 form=sv"), f);
    // The region's radios too: each joins sv
    for (const char *f : {"tu", "cu", "ck"})
      TEST_ASSERT_TRUE_MESSAGE(has(s, std::string("name=") + f + " value=0 form=sv"), f);
    // Feedback's choices live on its own page
    for (const char *f : {"tr", "bf", "kf", "rq0", "rq1", "rq2", "rq3"})
      TEST_ASSERT_FALSE_MESSAGE(has(s, std::string("name=") + f + " "), f);
  }
}

// --- Feedback ---------------------------------------------------------------------

static std::string radio(const char *name, int value, bool checked, const char *label,
                         const char *hint) {
  return std::string("<label class=op><input type=radio name=") + name + " value=" +
         (char)('0' + value) + " form=sv" + (checked ? " checked" : "") + "><span>" + label +
         "<small>" + hint + "</small></span></label>";
}

// One segment of a request kind: a real radio under its word, the whole
// segment the target
static std::string segment(const char *name, int value, bool checked, const char *label) {
  return std::string("<label><input type=radio name=") + name + " value=" + (char)('0' + value) +
         " form=sv" + (checked ? " checked" : "") + "><span>" + label + "</span></label>";
}

static std::string renderFeedback(const Strings &t, const PanelView &v) {
  sent.clear();
  largest = 0;
  flushes = 0;
  const Shell sh = {&t, nullptr, "carousel.local", TAB_NONE, false, "0.0.0-test"};
  String s;
  s.reserve(PANEL_PAGE_RESERVE);
  feedbackPage(s, sh, v, collect);
  if (s.length()) collect(s);
  return sent;
}

// Panel keeps one row: the name, the choices in a line, the gear to the page
static void test_feedback_row_on_panel() {
  for (const Strings *t : {&EN, &FR}) {
    Settings st;
    st.transition = TRANSITION_PUSH;
    st.brightnessFeedback = BRIGHTNESS_DOTS;
    st.knobFeedback = KNOB_BAND;
    const std::string s = section(render(*t, view(st, st)), "fb");
    String label;
    formatText(label, t->ordSettingsOf, t->pnlFeedback);
    at(s, std::string("<h2 id=h-4>") + t->pnlFeedback + "</h2><ul class=list><li><div class=sr>"
          "<span class=nm style=\"flex:1\">" + t->pnlFeedback + "<small class=sm>" +
          t->pnlTransitions[TRANSITION_PUSH] + " \xC2\xB7 " +
          t->pnlBrightnessFeedbacks[BRIGHTNESS_DOTS] + " \xC2\xB7 " +
          t->pnlKnobFeedbacks[KNOB_BAND] + "</small></span><a class=gt href=\"/panel/feedback\" "
          "aria-label=\"" + label.c_str() + "\"></a></div></li></ul>");
    TEST_ASSERT_FALSE(has(s, "<input"));
    TEST_ASSERT_FALSE(has(s, "<fieldset"));
  }
  // The line follows the stored choices
  Settings st;
  st.transition = TRANSITION_CUT;
  st.brightnessFeedback = BRIGHTNESS_NONE;
  st.knobFeedback = KNOB_NEIGHBOURS;
  at(section(render(FR, view(st, st)), "fb"),
     std::string("<small class=sm>") + FR.pnlTransitions[TRANSITION_CUT] + " \xC2\xB7 " +
         FR.pnlBrightnessFeedbacks[BRIGHTNESS_NONE] + " \xC2\xB7 " +
         FR.pnlKnobFeedbacks[KNOB_NEIGHBOURS] + "</small>");
}

// The page: back to Panel, the title, the one form, then the three groups
// of four radios, the default first, one line under each name
static void test_feedback_page_choices() {
  for (const Strings *t : {&EN, &FR}) {
    Settings st;
    st.transition = TRANSITION_WIPE;
    const std::string s = renderFeedback(*t, view(st, st));
    const std::string side = aside(s);
    size_t p = at(side, std::string("<a class=bk href=\"/panel\">") + t->webTabs[TAB_PANEL] + "</a>");
    p = at(side, std::string("<h1>") + t->pnlFeedback + "</h1>", p);
    p = at(side, "<form id=sv method=post action=\"/sauver\"><input type=hidden name=pg "
                 "value=panel/feedback><button hidden>", p);
    at(side, "<div class=\"sv dk\">", p);
    TEST_ASSERT_EQUAL(1, occurrences(s, "<h1"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "<form"));
    p = at(s, "</aside><div class=sp>");
    const struct { const char *name, *legend; const char *const *names, *const *hints; int set; }
        groups[] = {
      {"tr", t->pnlTransition, t->pnlTransitions, t->pnlTransitionHints, TRANSITION_WIPE},
      {"bf", t->pnlBrightnessChange, t->pnlBrightnessFeedbacks, t->pnlBrightnessHints, 0},
      {"kf", t->pnlKnobTurn, t->pnlKnobFeedbacks, t->pnlKnobHints, 0},
    };
    for (const auto &g : groups) {
      p = at(s, std::string("<fieldset><legend>") + g.legend + "</legend>", p);
      for (int i = 0; i < 4; i++) p = at(s, radio(g.name, i, i == g.set, g.names[i], g.hints[i]), p);
    }
    TEST_ASSERT_EQUAL(3 + 4, occurrences(s, " checked"));
  }
}

// Requests: the three meanings once, then a row per kind, a three-way
// segmented choice in a fieldset named for the kind, the stored one checked
static void test_feedback_page_requests() {
  for (const Strings *t : {&EN, &FR}) {
    Settings st;
    st.requestShow[REQUEST_IMAGE] = REQUEST_DISCREET;
    st.requestShow[REQUEST_AGENT] = REQUEST_OFF;
    const std::string s = renderFeedback(*t, view(st, st));
    size_t p = at(s, std::string("<h2 id=rq>") + t->pnlRequests + "</h2>");
    for (int i = 0; i < 3; i++) {
      p = at(s, std::string("<small><b>") + t->pnlRequestShows[i] + "</b> \xC2\xB7 " +
                    t->pnlRequestShowHints[i] + "</small>", p);
      TEST_ASSERT_EQUAL(1, occurrences(s, t->pnlRequestShowHints[i]));
    }
    for (int k = 0; k < 4; k++) {
      char name[8];
      snprintf(name, sizeof(name), "rq%d", k);
      p = at(s, std::string("<fieldset class=sg><legend>") + t->pnlRequestKinds[k] + "</legend>", p);
      for (int i = 0; i < 3; i++)
        p = at(s, segment(name, i, i == st.requestShow[k], t->pnlRequestShows[i]), p);
      p = at(s, "</fieldset>", p);
    }
    TEST_ASSERT_EQUAL(3 + 4, occurrences(s, " checked"));
    TEST_ASSERT_EQUAL(4 * 3 + 3 * 4, occurrences(s, "<input type=radio"));
  }
}

// Posted through Panel's save, its return path named; the unsaved bars
// start up after a refused or failed save and Discard opens the page
static void test_feedback_page_save() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string s = renderFeedback(*t, view());
    const std::string bar = std::string("<p class=u hidden>") + t->webUnsaved +
                            "</p><button class=p form=sv>" + t->webSave +
                            "</button><button type=button class=dc form=sv hidden>" + t->webDiscard +
                            "</button><small>" + t->pnlSaveScope + "</small></div>";
    TEST_ASSERT_EQUAL(2, occurrences(s, bar));
    at(s, "<p id=ann class=vh aria-live=polite></p><div class=\"sv mb\">" + bar, at(s, "</aside>"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "<script>"));
    at(s, std::string("<script>") + UNSAVED_SCRIPT + "</script>", at(s, "</footer>"));
  }
  for (SaveResult r : {SAVE_FAILED, SAVE_REFUSED}) {
    PanelView v = view();
    v.result = r;
    const std::string s = renderFeedback(EN, v);
    TEST_ASSERT_TRUE(has(s, "<form id=sv data-d=/panel/feedback method=post action=\"/sauver\">"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=\"sv dk on\">"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=\"sv mb on\">"));
  }
  PanelView v = view();
  v.result = SAVE_DONE;
  at(aside(renderFeedback(EN, v)), std::string("<div class=ok>") + EN.webSaved + "</div>");
  v.result = SAVE_FAILED;
  at(aside(renderFeedback(FR, v)), std::string("<div class=er role=alert>") + FR.webNotSaved + "</div>");
  // After a failed write the page holds what was posted
  Settings posted;
  posted.requestShow[REQUEST_COMMAND] = REQUEST_OFF;
  PanelView w = view(posted, DEFAULTS);
  w.result = SAVE_FAILED;
  at(renderFeedback(EN, w), segment("rq2", REQUEST_OFF, true, EN.pnlRequestShows[REQUEST_OFF]));
}

// What the browser posts, by name: one digit below the count, else the
// stored value stays; of two values for one control the first counts
static void test_feedback_form_parsing() {
  Settings st;
  FeedbackForm f;
  f.arg("tr", "3");
  f.arg("bf", "2");
  f.arg("kf", "1");
  f.arg("rq0", "2");
  f.arg("rq1", "1");
  f.arg("rq3", "2");
  f.arg("tr", "1");
  f.apply(st);
  TEST_ASSERT_EQUAL(TRANSITION_CUT, st.transition);
  TEST_ASSERT_EQUAL(BRIGHTNESS_NUMBER, st.brightnessFeedback);
  TEST_ASSERT_EQUAL(KNOB_NEIGHBOURS, st.knobFeedback);
  TEST_ASSERT_EQUAL(REQUEST_OFF, st.requestShow[REQUEST_MESSAGE]);
  TEST_ASSERT_EQUAL(REQUEST_DISCREET, st.requestShow[REQUEST_IMAGE]);
  TEST_ASSERT_EQUAL(REQUEST_SHOW, st.requestShow[REQUEST_COMMAND]);   // not posted
  TEST_ASSERT_EQUAL(REQUEST_OFF, st.requestShow[REQUEST_AGENT]);

  // Not an option, empty, two digits, a letter: the stored value stays
  Settings kept;
  kept.transition = TRANSITION_DISSOLVE;
  kept.requestShow[REQUEST_COMMAND] = REQUEST_DISCREET;
  FeedbackForm g;
  g.arg("tr", "4");
  g.arg("bf", "");
  g.arg("kf", "10");
  g.arg("rq2", "3");
  g.arg("rq0", "x");
  g.arg("other", "1");
  g.apply(kept);
  TEST_ASSERT_EQUAL(TRANSITION_DISSOLVE, kept.transition);
  TEST_ASSERT_EQUAL(BRIGHTNESS_DOTS, kept.brightnessFeedback);
  TEST_ASSERT_EQUAL(KNOB_BAND, kept.knobFeedback);
  TEST_ASSERT_EQUAL(REQUEST_DISCREET, kept.requestShow[REQUEST_COMMAND]);
  TEST_ASSERT_EQUAL(REQUEST_SHOW, kept.requestShow[REQUEST_MESSAGE]);
}

// The Wi-Fi setup posts the same names: what was taken, -1 for the rest
static void test_region_form_choice() {
  RegionForm f;
  f.arg("tu", "1");
  f.arg("cu", "12");
  f.arg("ck", "0");
  const RegionChoice c = f.choice();
  TEST_ASSERT_EQUAL(TEMP_F, c.unit);
  TEST_ASSERT_EQUAL(-1, c.currency);
  TEST_ASSERT_EQUAL(0, c.clock12);
  const RegionChoice none = RegionForm().choice();
  TEST_ASSERT_TRUE(none.unit == -1 && none.currency == -1 && none.clock12 == -1);
}

// Each control takes what its own enum offers: its last option, never its count
static void test_feedback_form_follows_each_enums_count() {
  struct { const char *name; uint8_t count; uint8_t Settings::*field; } controls[] = {
    {"tr", TRANSITION_COUNT, &Settings::transition},
    {"bf", BRIGHTNESS_FEEDBACK_COUNT, &Settings::brightnessFeedback},
    {"kf", KNOB_FEEDBACK_COUNT, &Settings::knobFeedback},
    {"bd", BOOT_DETAIL_COUNT, &Settings::bootDetail},
  };
  for (const auto &c : controls) {
    const char last[2] = {(char)('0' + c.count - 1), 0}, past[2] = {(char)('0' + c.count), 0};
    Settings st;
    st.*c.field = 0;
    FeedbackForm f;
    f.arg(c.name, past);
    f.apply(st);
    TEST_ASSERT_EQUAL_MESSAGE(0, st.*c.field, c.name);
    FeedbackForm g;
    g.arg(c.name, last);
    g.apply(st);
    TEST_ASSERT_EQUAL_MESSAGE(c.count - 1, st.*c.field, c.name);
  }
}

// The boot screen's three words as one segmented choice under Advanced,
// after the folds, the stored one checked, and its line
static void test_boot_choice_under_advanced() {
  for (const Strings *t : {&EN, &FR}) {
    Settings st;
    st.bootDetail = BOOT_DETAILED;
    const std::string adv = section(render(*t, view(st)), "adv");
    size_t p = at(adv, std::string("<summary>") + t->pnlMemoryTitle + "</summary>");
    p = at(adv, std::string("</ul><fieldset class=sg><legend>") + t->pnlBoot + "</legend>", p);
    p = at(adv, segment("bd", BOOT_AUTO, false, t->pnlBoots[BOOT_AUTO]), p);
    p = at(adv, segment("bd", BOOT_SIMPLE, false, t->pnlBoots[BOOT_SIMPLE]), p);
    p = at(adv, segment("bd", BOOT_DETAILED, true, t->pnlBoots[BOOT_DETAILED]), p);
    String hint = escapeHtml(t->pnlBootHint);
    at(adv, std::string("</fieldset><small>") + hint.c_str() + "</small>", p);
  }
}

// The line says what the boot shows now: Detailed adds only the IP under
// the loading line; the steps' checklist and the version are long gone
static void test_boot_hint_says_what_detailed_adds() {
  const std::string en = EN.pnlBootHint, fr = FR.pnlBootHint;
  TEST_ASSERT_TRUE(en.find("IP address under the line") != std::string::npos);
  TEST_ASSERT_TRUE(fr.find("adresse IP sous la ligne") != std::string::npos);
  TEST_ASSERT_TRUE(en.find("steps") == std::string::npos);
  TEST_ASSERT_TRUE(fr.find("étapes") == std::string::npos);
}

static void test_boot_choice_parsing() {
  Settings st;
  FeedbackForm f;
  f.arg("bd", "2");
  f.arg("bd", "1");
  f.apply(st);
  TEST_ASSERT_EQUAL(BOOT_DETAILED, st.bootDetail);
  Settings kept;
  kept.bootDetail = BOOT_SIMPLE;
  FeedbackForm g;
  g.arg("bd", "3");
  g.apply(kept);
  TEST_ASSERT_EQUAL(BOOT_SIMPLE, kept.bootDetail);
  FeedbackForm none;
  none.apply(kept);
  TEST_ASSERT_EQUAL(BOOT_SIMPLE, kept.bootDetail);
}

// --- Region ----------------------------------------------------------------------

static const char *const TEMP_WORDS[] = {"\xC2\xB0" "C", "\xC2\xB0" "F"};
static const char *const CURRENCY_WORDS[] = {"\xE2\x82\xAC", "$", "\xC2\xA3", "CHF"};
static const char *const CLOCK_WORDS[] = {"24\xC2\xA0h", "12\xC2\xA0h"};

// Region follows Language: the three choices as segmented controls, the
// stored one checked, each a real radio under its word
static void test_region_section() {
  for (const Strings *t : {&EN, &FR}) {
    Settings st;
    st.tempUnit = TEMP_F;
    st.currency = CURRENCY_GBP;
    st.clock12 = true;
    const std::string s = render(*t, view(st, st));
    const std::string r = section(s, "region");
    TEST_ASSERT_TRUE(at(s, "<section id=lang ") < at(s, "<section id=region "));
    TEST_ASSERT_TRUE(at(s, "<section id=region ") < at(s, "<section id=night "));
    at(r, std::string("<h2 id=h-2>") + t->pnlRegion + "</h2>");
    size_t p = at(r, std::string("<fieldset class=sg><legend>") + t->pnlTemperature + "</legend>");
    p = at(r, segment("tu", 0, false, TEMP_WORDS[0]), p);
    p = at(r, segment("tu", 1, true, TEMP_WORDS[1]), p);
    p = at(r, std::string("<fieldset class=sg><legend>") + t->pnlCurrency + "</legend>", p);
    for (int i = 0; i < 4; i++) p = at(r, segment("cu", i, i == CURRENCY_GBP, CURRENCY_WORDS[i]), p);
    p = at(r, std::string("<fieldset class=sg><legend>") + t->pnlClock + "</legend>", p);
    p = at(r, segment("ck", 0, false, CLOCK_WORDS[0]), p);
    p = at(r, segment("ck", 1, true, CLOCK_WORDS[1]), p);
    String hint = escapeHtml(t->pnlRegionHint);
    at(r, std::string("</fieldset><small>") + hint.c_str() + "</small>", p);
    TEST_ASSERT_EQUAL(3, occurrences(r, "<fieldset"));
  }
  // The defaults are what v0.1.0 showed
  const std::string d = section(render(EN, view()), "region");
  at(d, segment("tu", 0, true, TEMP_WORDS[0]));
  at(d, segment("cu", 0, true, CURRENCY_WORDS[0]));
  at(d, segment("ck", 0, true, CLOCK_WORDS[0]));
  // After a failed save the page holds what was posted
  Settings posted;
  posted.currency = CURRENCY_CHF;
  PanelView w = view(posted, DEFAULTS);
  w.result = SAVE_FAILED;
  at(section(render(EN, w), "region"), segment("cu", 3, true, "CHF"));
}

static void test_region_strings() {
  for (const Strings *t : {&EN, &FR}) {
    TEST_ASSERT_TRUE(*t->pnlRegion && *t->pnlTemperature && *t->pnlCurrency && *t->pnlClock);
    TEST_ASSERT_TRUE(*t->pnlRegionHint);
  }
  TEST_ASSERT_EQUAL_STRING("Region", EN.pnlRegion);
  TEST_ASSERT_EQUAL_STRING("R\xC3\xA9gion", FR.pnlRegion);
}

// What the browser posts, by name: one digit below the count, else the
// stored value stays; of two values for one control the first counts
static void test_region_form_parsing() {
  Settings st;
  RegionForm f;
  f.arg("tu", "1");
  f.arg("cu", "3");
  f.arg("ck", "1");
  f.arg("cu", "1");
  f.apply(st);
  TEST_ASSERT_EQUAL(TEMP_F, st.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_CHF, st.currency);
  TEST_ASSERT_TRUE(st.clock12);

  // Not an option, empty, two digits, a letter: the stored value stays
  Settings kept;
  kept.tempUnit = TEMP_F;
  kept.currency = CURRENCY_USD;
  kept.clock12 = true;
  RegionForm g;
  g.arg("tu", "2");
  g.arg("cu", "4");
  g.arg("ck", "");
  g.arg("other", "1");
  g.apply(kept);
  TEST_ASSERT_EQUAL(TEMP_F, kept.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_USD, kept.currency);
  TEST_ASSERT_TRUE(kept.clock12);
  RegionForm h;
  h.arg("tu", "x");
  h.arg("cu", "10");
  h.arg("ck", "2");
  h.apply(kept);
  TEST_ASSERT_EQUAL(TEMP_F, kept.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_USD, kept.currency);
  TEST_ASSERT_TRUE(kept.clock12);
  // A post without the section leaves the region alone; a 0 puts it back
  RegionForm none;
  none.apply(kept);
  TEST_ASSERT_EQUAL(TEMP_F, kept.tempUnit);
  RegionForm back;
  back.arg("tu", "0");
  back.arg("ck", "0");
  back.apply(kept);
  TEST_ASSERT_EQUAL(TEMP_C, kept.tempUnit);
  TEST_ASSERT_FALSE(kept.clock12);
  TEST_ASSERT_EQUAL(CURRENCY_USD, kept.currency);
}

// Each control takes what its own enum offers: its last option, never its count
static void test_region_form_follows_each_enums_count() {
  const char lastTemp[2] = {(char)('0' + TEMP_UNIT_COUNT - 1), 0};
  const char pastTemp[2] = {(char)('0' + TEMP_UNIT_COUNT), 0};
  const char lastMoney[2] = {(char)('0' + CURRENCY_COUNT - 1), 0};
  const char pastMoney[2] = {(char)('0' + CURRENCY_COUNT), 0};
  Settings st;
  RegionForm f;
  f.arg("tu", pastTemp);
  f.arg("cu", pastMoney);
  f.arg("ck", "2");
  f.apply(st);
  TEST_ASSERT_EQUAL(TEMP_C, st.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_EUR, st.currency);
  TEST_ASSERT_FALSE(st.clock12);
  RegionForm g;
  g.arg("tu", lastTemp);
  g.arg("cu", lastMoney);
  g.arg("ck", "1");
  g.apply(st);
  TEST_ASSERT_EQUAL(TEMP_UNIT_COUNT - 1, st.tempUnit);
  TEST_ASSERT_EQUAL(CURRENCY_COUNT - 1, st.currency);
  TEST_ASSERT_TRUE(st.clock12);
}

// The save says what it saves, the region included
static void test_save_scope_names_the_region() {
  TEST_ASSERT_NOT_NULL(strstr(EN.pnlSaveScope, "region"));
  TEST_ASSERT_NOT_NULL(strstr(FR.pnlSaveScope, "r\xC3\xA9gion"));
}

// Save, with what it saves, in the side column and at the end
static void test_save_scope_line() {
  for (const Strings *t : {&EN, &FR}) {
    const std::string s = render(*t, view());
    const std::string bar = std::string("<p class=u hidden>") + t->webUnsaved +
                            "</p><button class=p form=sv>" + t->webSave +
                            "</button><button type=button class=dc form=sv hidden>" + t->webDiscard +
                            "</button><small>" + t->pnlSaveScope + "</small></div>";
    TEST_ASSERT_EQUAL(2, occurrences(s, bar));
    at(aside(s), "<div class=\"sv dk\">" + bar);
    at(s, "<p id=ann class=vh aria-live=polite></p><div class=\"sv mb\">" + bar, at(s, "</aside>"));
    // One script after the footer: the unsaved-changes one, then Copy's
    TEST_ASSERT_EQUAL(1, occurrences(s, "<script>"));
    at(s, std::string("<script>") + UNSAVED_SCRIPT + COPY_SCRIPT + "</script>", at(s, "</footer>"));
  }
}

// A refused or a failed save shows the posted values: both bars up as
// sent, form sv marked dirty, Discard opening /panel
static void test_unsaved_after_a_failed_save() {
  for (SaveResult r : {SAVE_FAILED, SAVE_REFUSED}) {
    PanelView v = view();
    v.result = r;
    const std::string s = render(EN, v);
    TEST_ASSERT_TRUE(has(s, "<form id=sv data-d=/panel method=post action=\"/sauver\">"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=\"sv dk on\">"));
    TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=\"sv mb on\">"));
  }
  for (SaveResult r : {SAVE_NONE, SAVE_DONE, SAVE_LATER}) {
    PanelView v = view();
    v.result = r;
    const std::string s = render(EN, v);
    TEST_ASSERT_TRUE(has(s, "<form id=sv method=post"));
    TEST_ASSERT_FALSE(has(s, " on\">"));
  }
}

// What the save did, in the side column under the lead
static void test_result_banners() {
  const struct { SaveResult r; ListCheck c; std::string banner; } cases[] = {
    {SAVE_DONE, LIST_OK, std::string("<div class=ok>") + EN.webSaved + "</div>"},
    {SAVE_LATER, LIST_OK, std::string("<div class=ok>") + EN.webSavedLater + "</div>"},
    {SAVE_FAILED, LIST_OK, std::string("<div class=er role=alert>") + EN.webNotSaved + "</div>"},
    {SAVE_REFUSED, LIST_EMPTY, std::string("<div class=er role=alert>") + EN.webRefusedEmpty + "</div>"},
    {SAVE_REFUSED, LIST_NO_LANDSCAPE,
     std::string("<div class=er role=alert>") + EN.webRefusedLandscape + "</div>"},
  };
  for (const auto &c : cases) {
    PanelView v = view();
    v.result = c.r;
    v.refusal = c.c;
    const std::string s = aside(render(EN, v));
    TEST_ASSERT_TRUE_MESSAGE(has(s, std::string(EN.pnlLead) + "</p>" + c.banner + "<ul class=idx>"),
                             c.banner.c_str());
    TEST_ASSERT_EQUAL(1, occurrences(s, "<div class=ok>") + occurrences(s, "<div class=er"));
  }
  const std::string none = render(EN, view());
  TEST_ASSERT_FALSE(has(none, "<div class=ok>"));
  TEST_ASSERT_FALSE(has(none, "<div class=er"));
  // After a failed write the page shows what was posted
  Settings posted;
  posted.nightStart = 21;
  PanelView v = view(posted, DEFAULTS);
  v.result = SAVE_FAILED;
  at(render(FR, v), "name=nde form=sv value=\"21\"");
}

// The worst case: every value at its longest and escaped, French, six
// sessions, each update state
static void test_page_fits_24k() {
  Settings values;
  memset(values.mqttHost, '"', sizeof(values.mqttHost) - 1);
  memset(values.mqttUser, '"', sizeof(values.mqttUser) - 1);
  memset(values.mqttPass, 'x', sizeof(values.mqttPass) - 1);
  const std::string ssid(32, '"'), latest(23, '"'), error(47, '"'), token(64, '"');
  size_t worst = 0, page = 0;
  for (int l = 0; l < LANG_COUNT; l++) {
    for (UpdateState u : {UPD_AVAILABLE, UPD_FAILED}) {
      for (SaveResult r : {SAVE_NONE, SAVE_LATER, SAVE_FAILED, SAVE_REFUSED}) {
        PanelView v = view(values, values);
        v.update = u;
        v.latest = latest.c_str();
        v.updateError = error.c_str();
        v.ssid = ssid.c_str();
        v.rssi = -100;
        v.sessionCount = 6;
        v.ip = "255.255.255.255";
        v.token = token.c_str();
        v.freeKb = v.minFreeKb = 4294967295u;
        v.resetReason = "software restart (watchdog)";
        v.upSec = 4294967295u;
        v.result = r;
        v.refusal = r == SAVE_REFUSED ? LIST_NO_PORTRAIT : LIST_OK;
        const std::string s = render(STRINGS[l], v);
        TEST_ASSERT_TRUE(flushes > 2);
        TEST_ASSERT_TRUE_MESSAGE(largest <= PANEL_PAGE_RESERVE, STRINGS[l].code);
        TEST_ASSERT_TRUE_MESSAGE(s.size() <= 24576, STRINGS[l].code);
        TEST_ASSERT_TRUE(s.compare(s.size() - 7, 7, "</html>") == 0);
        worst = std::max(worst, largest);
        page = std::max(page, s.size());
        const std::string f = renderFeedback(STRINGS[l], v);
        TEST_ASSERT_TRUE_MESSAGE(largest <= PANEL_PAGE_RESERVE, STRINGS[l].code);
        TEST_ASSERT_TRUE_MESSAGE(f.size() <= 24576, STRINGS[l].code);
        TEST_ASSERT_TRUE(f.compare(f.size() - 7, 7, "</html>") == 0);
      }
    }
  }
  printf("Panel: largest piece %u of %u bytes, page %u of 24576\n", (unsigned)worst,
         (unsigned)PANEL_PAGE_RESERVE, (unsigned)page);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_panel_sections_in_order);
  RUN_TEST(test_one_h1_one_main);
  RUN_TEST(test_language_select_labelled);
  RUN_TEST(test_night_fields);
  RUN_TEST(test_update_states);
  RUN_TEST(test_signal_words);
  RUN_TEST(test_mqtt_password_never_sent);
  RUN_TEST(test_hooks_title_counts);
  RUN_TEST(test_hooks_install_command);
  RUN_TEST(test_hooks_copy_button_hidden);
  RUN_TEST(test_hooks_open_setting);
  RUN_TEST(test_hooks_renew);
  RUN_TEST(test_hooks_refusal_line);
  RUN_TEST(test_hooks_values_escaped);
  RUN_TEST(test_hooks_attributes_double_quoted);
  RUN_TEST(test_advanced_folded);
  RUN_TEST(test_token_and_sign_out);
  RUN_TEST(test_memory_and_restart);
  RUN_TEST(test_panel_form_carries_no_screen_field);
  RUN_TEST(test_region_section);
  RUN_TEST(test_region_strings);
  RUN_TEST(test_region_form_parsing);
  RUN_TEST(test_region_form_choice);
  RUN_TEST(test_region_form_follows_each_enums_count);
  RUN_TEST(test_save_scope_names_the_region);
  RUN_TEST(test_feedback_row_on_panel);
  RUN_TEST(test_feedback_page_choices);
  RUN_TEST(test_feedback_page_requests);
  RUN_TEST(test_feedback_page_save);
  RUN_TEST(test_feedback_form_parsing);
  RUN_TEST(test_feedback_form_follows_each_enums_count);
  RUN_TEST(test_boot_choice_under_advanced);
  RUN_TEST(test_boot_hint_says_what_detailed_adds);
  RUN_TEST(test_boot_choice_parsing);
  RUN_TEST(test_save_scope_line);
  RUN_TEST(test_unsaved_after_a_failed_save);
  RUN_TEST(test_result_banners);
  RUN_TEST(test_page_fits_24k);
  return UNITY_END();
}
