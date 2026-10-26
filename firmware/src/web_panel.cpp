// SPDX-License-Identifier: AGPL-3.0-only
#include "web_panel.h"

#include <stdio.h>
#include <string.h>

#include "web_stage.h"

SignalWord signalWord(int dbm) {
  return dbm >= -67 ? SIGNAL_GOOD : dbm >= -75 ? SIGNAL_FAIR : SIGNAL_WEAK;
}

// Firmware strings are escaped too: the token's hint quotes "<token>"
static void pText(String &s, const char *t) { s += escapeHtml(t); }

static void pRow(String &s, const char *label, const char *cls) {
  s += F("<div class=st><span>");
  pText(s, label);
  s += cls;
}
static void pRowEnd(String &s) { s += F("</b></div>"); }

static void pHint(String &s, const char *t, const char *style = "") {
  s += F("<small");
  s += style;
  s += '>';
  pText(s, t);
  s += F("</small>");
}

static void pSection(String &s, const char *id, int n, const char *title) {
  char head[96];
  snprintf(head, sizeof(head), "<section id=%s class=sec aria-labelledby=h-%d><h2 id=h-%d>", id, n,
           n);
  s += head;
  pText(s, title);
  s += F("</h2>");
}

// Label and input, joined to the save; value escaped, attrs this file's own
static void pInput(String &s, const char *label, const char *name, const String &value,
                  const char *attrs) {
  s += F("<label for=");
  s += name;
  s += '>';
  pText(s, label);
  s += F("</label><input id=");
  s += name;
  s += F(" name=");
  s += name;
  s += F(" form=sv value=\"");
  s += escapeHtml(value);
  s += F("\" ");
  s += attrs;
  s += '>';
}

static void pSaveBar(String &s, const Strings &t, const char *place, bool on) {
  saveBarStart(s, t, place, on);
  s += F("<small>");
  pText(s, t.pnlSaveScope);
  s += F("</small></div>");
}

static void pResult(String &s, const Strings &t, const PanelView &v) {
  if (v.result == SAVE_DONE) notice(s, t.webSaved);
  else if (v.result == SAVE_LATER) notice(s, t.webSavedLater);
  else if (v.result == SAVE_FAILED) errorNotice(s, t.webNotSaved);
  else if (v.result == SAVE_REFUSED)
    errorNotice(s, v.refusal == LIST_EMPTY ? t.webRefusedEmpty
                 : v.refusal == LIST_NO_PORTRAIT ? t.webRefusedPortrait : t.webRefusedLandscape);
}

// A refused or failed save: the page holds the posted values, not the
// stored ones, so the bars start up and Discard opens /panel
static bool pPosted(const PanelView &v) {
  return v.result == SAVE_FAILED || v.result == SAVE_REFUSED;
}

// The page's four forms, empty: each control joins one by form=. The first
// submit button of sv is the hidden one Enter presses.
static void pForms(String &s, const Strings &t, bool posted) {
  s += posted ? F("<form id=sv data-d=/panel method=post action=\"/sauver\">")
              : F("<form id=sv method=post action=\"/sauver\">");
  s += F("<input type=hidden name=pg value=panel><button hidden>");
  pText(s, t.webSave);
  s += F("</button></form><form id=uf method=post action=\"/update\"></form>"
         "<form id=pf method=post action=\"/purge\"></form>"
         "<form id=so method=post action=\"/signout\"></form>"
         "<form id=hk method=post action=\"/hooks\"></form>");
}

static const char *const PANEL_SECTIONS[] = {"lang", "region", "night", "fb", "upd", "net", "adv"};
static const int PANEL_SECTION_COUNT = sizeof(PANEL_SECTIONS) / sizeof(PANEL_SECTIONS[0]);

static const char *pTitle(const Strings &t, int i) {
  const char *const titles[] = {t.webLanguage, t.pnlRegion, t.webNightMode, t.pnlFeedback,
                                t.pnlUpdates, t.pnlWifi, t.pnlAdvanced};
  return titles[i];
}

static void pSide(String &s, const Strings &t, const PanelView &v) {
  s += F("<div class=two><aside><h1>");
  pText(s, t.webTabs[TAB_PANEL]);
  s += F("</h1><p class=lead>");
  pText(s, t.pnlLead);
  s += F("</p>");
  pResult(s, t, v);
  s += F("<ul class=idx>");
  for (int i = 0; i < PANEL_SECTION_COUNT; i++) {
    s += F("<li><a href=\"#");
    s += PANEL_SECTIONS[i];
    s += F("\">");
    pText(s, pTitle(t, i));
    s += F("</a>");
  }
  s += F("</ul>");
  pSaveBar(s, t, "dk", pPosted(v));
  s += F("</aside><div>");
}

static void languageSection(String &s, const Strings &t, const Settings &st) {
  pSection(s, "lang", 1, pTitle(t, 0));
  s += F("<label for=lg>");
  pText(s, t.pnlLanguageLabel);
  s += F("</label><select id=lg name=lang form=sv>");
  for (uint8_t l = 0; l < LANG_COUNT; l++) {
    s += F("<option value=");
    s += String((unsigned)l);
    s += F(" lang=");
    s += STRINGS[l].code;
    if (l == st.lang) s += F(" selected");
    s += '>';
    pText(s, STRINGS[l].name);
    s += F("</option>");
  }
  s += F("</select></section>");
}

// Three words or fewer: a segmented choice, each segment a radio under its
// word (the CSS draws the chosen one filled)
static void pSegments(String &s, const char *name, const char *legend, uint8_t chosen,
                      const char *const *labels, uint8_t count) {
  s += F("<fieldset class=sg><legend>");
  pText(s, legend);
  s += F("</legend>");
  for (uint8_t i = 0; i < count; i++) {
    s += F("<label><input type=radio name=");
    s += name;
    s += F(" value=");
    s += (char)('0' + i);
    s += i == chosen ? F(" form=sv checked><span>") : F(" form=sv><span>");
    pText(s, labels[i]);
    s += F("</span></label>");
  }
  s += F("</fieldset>");
}

static_assert(TEMP_UNIT_COUNT == 2 && CURRENCY_COUNT == 4,
              "the Region choices are written for these counts");

// Region: the unit, the currency and the clock as segmented choices, each
// segment a radio under its sign (the same in both languages, so not in
// i18n). The numbers are the enums' and the clock's 0 and 1.
static void regionSection(String &s, const Strings &t, const Settings &st) {
  static const char *const TEMPERATURES[] = {"\xC2\xB0" "C", "\xC2\xB0" "F"};
  static const char *const CURRENCIES[] = {"\xE2\x82\xAC", "$", "\xC2\xA3", "CHF"};
  static const char *const CLOCKS[] = {"24\xC2\xA0h", "12\xC2\xA0h"};   // a number never parts from its unit
  pSection(s, "region", 2, t.pnlRegion);
  pSegments(s, "tu", t.pnlTemperature, st.tempUnit, TEMPERATURES, TEMP_UNIT_COUNT);
  pSegments(s, "cu", t.pnlCurrency, st.currency, CURRENCIES, CURRENCY_COUNT);
  pSegments(s, "ck", t.pnlClock, st.clock12 ? 1 : 0, CLOCKS, 2);
  pHint(s, t.pnlRegionHint);
  s += F("</section>");
}

static void nightSection(String &s, const Strings &t, const Settings &st) {
  pSection(s, "night", 3, t.webNightMode);
  s += F("<div class=\"r r3\"><div>");
  pInput(s, t.webNightFrom, "nde", String((unsigned)st.nightStart), "inputmode=numeric");
  s += F("</div><div>");
  pInput(s, t.webNightTo, "nto", String((unsigned)st.nightEnd), "inputmode=numeric");
  s += F("</div><div class=f><label for=nlv>");
  pText(s, t.webNightLevel);
  s += F("</label><select id=nlv name=nlv form=sv>");
  // Out of 255; shown as a percentage
  static const uint8_t NIGHT_LEVELS[] = {0, 3, 10, 25, 50};
  for (uint8_t lv : NIGHT_LEVELS) {
    s += F("<option value=");
    s += String((unsigned)lv);
    if (lv == st.nightBrightness) s += F(" selected");
    s += '>';
    if (lv) {
      s += String((unsigned)((lv * 100 + 254) / 255));
      s += F(" %");   // a number never parts from its unit
    } else {
      pText(s, t.webNightOff);
    }
    s += F("</option>");
  }
  s += F("</select></div></div>");
  pHint(s, t.webNightHint);
  s += F("</section>");
}

// One option as Running order writes it: the whole row the target, the
// name and its one line
static void pOption(String &s, const char *name, uint8_t value, bool checked, const char *label,
                    const char *hint) {
  s += F("<label class=op><input type=radio name=");
  s += name;
  s += F(" value=");
  s += (char)('0' + value);
  s += checked ? F(" form=sv checked><span>") : F(" form=sv><span>");
  pText(s, label);
  s += F("<small>");
  pText(s, hint);
  s += F("</small></span></label>");
}

static void pChoice(String &s, const char *name, const char *legend, uint8_t chosen, uint8_t count,
                    const char *const *labels, const char *const *hints) {
  s += F("<fieldset><legend>");
  pText(s, legend);
  s += F("</legend>");
  for (uint8_t i = 0; i < count; i++) pOption(s, name, i, i == chosen, labels[i], hints[i]);
  s += F("</fieldset>");
}

static_assert(TRANSITION_COUNT == 4 && BRIGHTNESS_FEEDBACK_COUNT == 4 && KNOB_FEEDBACK_COUNT == 4 &&
              REQUEST_KIND_COUNT == 4 && REQUEST_SHOW_COUNT == 3 && BOOT_DETAIL_COUNT == 3,
              "the Feedback strings are written for these counts");

// Panel's one row for it: the name, the choices in a line and the gear to
// the page (the same link as Running order's rows)
static void feedbackSection(String &s, const Strings &t, const Settings &st) {
  pSection(s, "fb", 4, t.pnlFeedback);
  s += F("<ul class=list><li><div class=sr><span class=nm style=\"flex:1\">");
  pText(s, t.pnlFeedback);
  s += F("<small class=sm>");
  pText(s, t.pnlTransitions[st.transition]);
  s += F(" \xC2\xB7 ");
  pText(s, t.pnlBrightnessFeedbacks[st.brightnessFeedback]);
  s += F(" \xC2\xB7 ");
  pText(s, t.pnlKnobFeedbacks[st.knobFeedback]);
  s += F("</small></span><a class=gt href=\"/panel/feedback\" aria-label=\"");
  String label;
  formatText(label, t.ordSettingsOf, t.pnlFeedback);
  pText(s, label.c_str());
  s += F("\"></a></div></li></ul></section>");
}

// GET /panel/feedback: the choices Panel's row sums up. The side column as a
// screen's page: the way back, the title, the one form (it names this page
// as the save's return path), what the save did, then Save.
void feedbackPage(String &s, const Shell &shell, const PanelView &v, void (*flush)(String &)) {
  const Strings &t = *shell.t;
  const Settings &st = *v.values;
  Shell sh = shell;
  sh.title = t.pnlFeedback;
  sh.tab = TAB_PANEL;
  sh.tabs = true;
  shellStart(s, sh);
  s += F("<div class=two><aside><a class=bk href=\"/panel\">");
  pText(s, t.webTabs[TAB_PANEL]);
  s += F("</a><h1>");
  pText(s, t.pnlFeedback);
  s += F("</h1>");
  s += pPosted(v) ? F("<form id=sv data-d=/panel/feedback method=post action=\"/sauver\">")
                  : F("<form id=sv method=post action=\"/sauver\">");
  s += F("<input type=hidden name=pg value=panel/feedback><button hidden>");
  pText(s, t.webSave);
  s += F("</button></form>");
  pResult(s, t, v);
  pSaveBar(s, t, "dk", pPosted(v));
  s += F("</aside><div class=sp>");
  pChoice(s, "tr", t.pnlTransition, st.transition, 4, t.pnlTransitions, t.pnlTransitionHints);
  pChoice(s, "bf", t.pnlBrightnessChange, st.brightnessFeedback, 4, t.pnlBrightnessFeedbacks,
          t.pnlBrightnessHints);
  pChoice(s, "kf", t.pnlKnobTurn, st.knobFeedback, 4, t.pnlKnobFeedbacks, t.pnlKnobHints);
  flush(s);
  s += F("<section class=sec aria-labelledby=rq><h2 id=rq>");
  pText(s, t.pnlRequests);
  s += F("</h2>");
  for (uint8_t i = 0; i < REQUEST_SHOW_COUNT; i++) {
    s += F("<small><b>");
    pText(s, t.pnlRequestShows[i]);
    s += F("</b> \xC2\xB7 ");
    pText(s, t.pnlRequestShowHints[i]);
    s += F("</small>");
  }
  for (uint8_t k = 0; k < REQUEST_KIND_COUNT; k++) {
    char name[4] = {'r', 'q', (char)('0' + k), 0};
    pSegments(s, name, t.pnlRequestKinds[k], st.requestShow[k], t.pnlRequestShows,
              REQUEST_SHOW_COUNT);
  }
  s += F("</section><p id=ann class=vh aria-live=polite></p>");
  pSaveBar(s, t, "mb", pPosted(v));
  s += F("</div></div>");
  flush(s);
  shellEnd(s, sh, UNSAVED_SCRIPT);
}

void FeedbackForm::arg(const char *name, const char *value) {
  // One digit below the count, as the page writes an option; -1: anything else
  auto digit = [&](uint8_t count) {
    return value[0] >= '0' && value[0] < '0' + count && !value[1] ? (int8_t)(value[0] - '0')
                                                                   : (int8_t)-1;
  };
  int8_t *slot = nullptr;
  uint8_t count = 0;
  if (!strcmp(name, "tr")) {
    slot = &transition_;
    count = TRANSITION_COUNT;
  } else if (!strcmp(name, "bf")) {
    slot = &brightness_;
    count = BRIGHTNESS_FEEDBACK_COUNT;
  } else if (!strcmp(name, "kf")) {
    slot = &knob_;
    count = KNOB_FEEDBACK_COUNT;
  } else if (!strcmp(name, "bd")) {
    slot = &boot_;
    count = BOOT_DETAIL_COUNT;
  } else if (name[0] == 'r' && name[1] == 'q' && name[2] >= '0' &&
             name[2] < '0' + REQUEST_KIND_COUNT && !name[3]) {
    slot = &request_[name[2] - '0'];
    count = REQUEST_SHOW_COUNT;
  }
  if (slot && *slot < 0) *slot = digit(count);
}

void FeedbackForm::apply(Settings &s) const {
  if (transition_ >= 0) s.transition = (uint8_t)transition_;
  if (brightness_ >= 0) s.brightnessFeedback = (uint8_t)brightness_;
  if (knob_ >= 0) s.knobFeedback = (uint8_t)knob_;
  if (boot_ >= 0) s.bootDetail = (uint8_t)boot_;
  for (uint8_t k = 0; k < REQUEST_KIND_COUNT; k++)
    if (request_[k] >= 0) s.requestShow[k] = (uint8_t)request_[k];
}

void RegionForm::arg(const char *name, const char *value) {
  // One digit below the count, as the page writes a segment; -1: anything else
  auto digit = [&](uint8_t count) {
    return value[0] >= '0' && value[0] < '0' + count && !value[1] ? (int8_t)(value[0] - '0')
                                                                   : (int8_t)-1;
  };
  int8_t *slot = nullptr;
  uint8_t count = 0;
  if (!strcmp(name, "tu")) {
    slot = &choice_.unit;
    count = TEMP_UNIT_COUNT;
  } else if (!strcmp(name, "cu")) {
    slot = &choice_.currency;
    count = CURRENCY_COUNT;
  } else if (!strcmp(name, "ck")) {
    slot = &choice_.clock12;
    count = 2;
  }
  if (slot && *slot < 0) *slot = digit(count);
}

void RegionForm::apply(Settings &s) const {
  if (choice_.unit >= 0) s.tempUnit = (uint8_t)choice_.unit;
  if (choice_.currency >= 0) s.currency = (uint8_t)choice_.currency;
  if (choice_.clock12 >= 0) s.clock12 = choice_.clock12 == 1;
}

// The running version and what the last check said; Install only while a
// newer one is available
static void updateSection(String &s, const Shell &sh, const PanelView &v) {
  const Strings &t = *sh.t;
  pSection(s, "upd", 5, t.pnlUpdates);
  pRow(s, t.webFirmware, "</span><b class=num>");
  s += escapeHtml(sh.version);
  const char *word = nullptr;
  switch (v.update) {
    case UPD_IDLE:       break;
    case UPD_CHECKING:   word = t.updChecking; break;
    case UPD_UP_TO_DATE: word = t.updUpToDate; break;
    case UPD_AVAILABLE:  word = t.updAvailable; break;
    case UPD_INSTALLING: word = t.updInstalling; break;
    case UPD_FAILED:     break;
  }
  if (v.update == UPD_AVAILABLE) {
    s += F(" · ");
    s += escapeHtml(v.latest);
    s += ' ';
    pText(s, word);
  } else if (v.update == UPD_FAILED) {
    s += F(" · ");
    s += escapeHtml(v.updateError);
  } else if (word) {
    s += F(" · ");
    pText(s, word);
  }
  pRowEnd(s);
  // The hidden field tells an unticked box from a form without the box
  s += F("<input type=hidden name=auh value=1 form=sv><label class=ck><input type=checkbox "
         "name=au value=1 form=sv");
  if (v.values->autoUpdate) s += F(" checked");
  s += '>';
  pText(s, t.updAuto);
  s += F("</label><button form=uf name=do value=check style=\"margin-top:12px\">");
  pText(s, t.updCheck);
  s += F("</button>");
  if (v.update == UPD_AVAILABLE) {
    s += F("<button form=uf name=do value=install class=p>");
    pText(s, t.updInstall);
    s += ' ';
    s += escapeHtml(v.latest);
    s += F("</button>");
  }
  s += F("</section>");
}

static void wifiSection(String &s, const Strings &t, const PanelView &v) {
  pSection(s, "net", 6, t.pnlWifi);
  pRow(s, t.wifiSaved, "</span><b>");
  if (v.ssid && v.ssid[0]) s += escapeHtml(v.ssid);
  else pText(s, t.wifiNoneYet);
  pRowEnd(s);
  pRow(s, t.webSignal, "</span><b class=num>");
  if (v.connected) {
    char line[24];
    snprintf(line, sizeof(line), "%d dBm · ", v.rssi);
    s += line;
    pText(s, t.pnlSignal[signalWord(v.rssi)]);
  } else {
    s += F("--");   // no reading off the network, so no word either
  }
  pRowEnd(s);
  pHint(s, t.webSignalHint);
  s += F("<a class=go href=\"/wifi\">");
  pText(s, t.webChangeWifi);
  s += F("</a></section>");
}

static void pFoldStart(String &s, const char *id) {
  s += F("<li><details");
  if (id) {
    s += F(" id=");
    s += id;
  }
  s += F(" style=\"margin:0\"><summary>");
}
static void pFoldBody(String &s) { s += F("</summary><div class=sd>"); }
static void pFoldEnd(String &s) { s += F("</div></details></li>"); }

static void mqttFold(String &s, const Strings &t, const PanelView &v) {
  const Settings &st = *v.values;
  pFoldStart(s, nullptr);
  String title;
  formatText(title, t.pnlMqttTitle,
             v.mqtt == MQTT_OFF ? t.webMqttOff : v.mqtt == MQTT_UP ? t.webMqttOn : t.webMqttDown);
  pText(s, title.c_str());
  pFoldBody(s);
  s += F("<div class=r><div class=f>");
  pInput(s, t.webMqttHost, "mqh", st.mqttHost, "maxlength=63 placeholder=homeassistant.local");
  s += F("</div><div>");
  pInput(s, t.webMqttPort, "mqp", String((unsigned)st.mqttPort), "inputmode=numeric");
  s += F("</div><div>");
  pInput(s, t.webMqttUser, "mqu", st.mqttUser, "maxlength=31 autocomplete=off");
  s += F("</div><div class=f>");
  // Never sent back; the mark says one is stored, from what is stored
  pInput(s, t.webMqttPass, "mqw", String(), mqttPassAttrs(*v.stored));
  s += F("</div></div>");
  pHint(s, t.webMqttHint);
  pFoldEnd(s);
}

// Copy: the page is plain HTTP, so there is no clipboard API; the older copy
// command runs from a tap in every current browser. The button is hidden in
// the markup, so without scripts the field stays selectable and nothing
// useless shows. Both answers ride on the button (data-ok, data-no) in the
// page's language; ann is the page's polite line.
static const char COPY_SCRIPT[] =
    "{let f=hkc,b=hkb;b.hidden=false;b.onclick=()=>{f.focus();f.select();"
    "f.setSelectionRange(0,f.value.length);let k=0;try{k=document.execCommand('copy')}catch(e){}"
    "ann.textContent=k?b.dataset.ok:b.dataset.no}}";

static void hooksTitle(String &s, const Strings &t, int sessions) {
  if (sessions <= 0) {
    pText(s, t.pnlHooksNone);
  } else if (sessions == 1) {
    pText(s, t.pnlHooksOne);
  } else {
    String title;
    formatText(title, t.pnlHooksMany, String(sessions).c_str());
    pText(s, title.c_str());
  }
}

// The command, in a read-only field that scrolls inside itself, and Copy.
// --noproxy: a computer with http_proxy set would hand the token in the
// address to its proxy and the proxy's logs.
static void hooksInstall(String &s, const Strings &t, const PanelView &v) {
  s += F("<label for=hkc>");
  pText(s, t.pnlHookInstall);
  s += F("</label><input id=hkc readonly spellcheck=false autocomplete=off value=\"");
  String cmd("curl -sS --compressed --noproxy '*' \"http://");
  cmd += v.ip;
  cmd += F("/install?k=");
  cmd += v.hookToken;
  cmd += F("\" | sh");
  s += escapeHtml(cmd);
  s += F("\"><button type=button id=hkb form=sv hidden data-ok=\"");
  pText(s, t.pnlCopied);
  s += F("\" data-no=\"");
  pText(s, t.pnlCopyByHand);
  s += F("\" style=\"margin-top:12px\">");
  pText(s, t.pnlCopy);
  s += F("</button>");
  pHint(s, t.pnlHookInstallHint);
  s += F("<small>");
  pText(s, t.pnlHookUninstall);
  s += F(" <code>sh ~/.config/carousel/setup.sh uninstall</code></small>");
}

// Refusals so far, then the setting for hooks without a token, then Renew
static void hooksToken(String &s, const Strings &t, const PanelView &v) {
  if (v.hookRefusals > 0) {
    String line;
    formatText(line, t.pnlHookRefused, String((unsigned long)v.hookRefusals).c_str());
    s += F("<small id=hkr>");
    pText(s, line.c_str());
    s += F("</small>");
  }
  s += F("<p style=\"margin:16px 0 0\">");
  pText(s, v.hookOpen ? t.pnlHookOpenOn : t.pnlHookOpenOff);
  s += F("</p>");
  if (v.hookOpen) {
    s += F("<div class=er style=\"margin-top:12px\">");
    pText(s, t.pnlHookOpenWarn);
    s += F("</div>");
  }
  s += F("<button form=hk name=do value=");
  s += v.hookOpen ? F("refuse") : F("accept");
  s += F(" style=\"margin-top:12px\">");
  pText(s, v.hookOpen ? t.pnlHookRefuse : t.pnlHookAccept);
  s += F("</button><button form=hk name=do value=renew style=\"margin-top:12px\">");
  pText(s, t.pnlHookRenew);
  s += F("</button>");
  pHint(s, t.pnlHookRenewHint);
}

static void hooksFold(String &s, const Strings &t, const PanelView &v) {
  pFoldStart(s, "hooks");
  hooksTitle(s, t, v.sessionCount);
  pFoldBody(s);
  hooksInstall(s, t, v);
  hooksToken(s, t, v);
  pRow(s, t.webHookAddress, "</span><b><code>http://");
  s += escapeHtml(v.ip);
  s += F("/agent/&lt;state&gt;</code>");
  pRowEnd(s);
  s += F("<small>");
  pText(s, t.webStates);
  s += F(" idle, thinking, tool, attention, error, done, failed, end. ");
  pText(s, t.pnlHookLegacy);
  s += F("</small>");
  pHint(s, t.pnlHooksHint);
  // A session killed without its end hook stays 15 min: this saves the wait
  if (v.sessionCount > 0) {
    s += F("<button form=pf style=\"margin-top:12px\">");
    pText(s, t.webClearSessions);
    s += F("</button>");
  }
  pFoldEnd(s);
}

static void apiFold(String &s, const Strings &t, const PanelView &v) {
  pFoldStart(s, nullptr);
  pText(s, t.pnlApiTitle);
  pFoldBody(s);
  pRow(s, t.pnlToken, "</span><b><code>");
  s += escapeHtml(v.token);
  s += F("</code>");
  pRowEnd(s);
  pHint(s, t.webApiHint);
  s += F("<button form=so class=del style=\"margin-top:12px\">");
  pText(s, t.signOutAll);
  s += F("</button>");
  pFoldEnd(s);
}

static void memoryFold(String &s, const Strings &t, const PanelView &v) {
  pFoldStart(s, nullptr);
  pText(s, t.pnlMemoryTitle);
  pFoldBody(s);
  pRow(s, t.webFreeMemory, "</span><b class=num>");
  s += String((unsigned long)v.freeKb);
  s += F(" ");
  pText(s, t.webKb);
  s += F(" (");
  pText(s, t.webMemoryMin);
  s += ' ';
  s += String((unsigned long)v.minFreeKb);
  s += F(" ");
  pText(s, t.webKb);
  s += ')';
  pRowEnd(s);
  pRow(s, t.webLastRestart, "</span><b>");
  s += escapeHtml(v.resetReason);
  s += F(", ");
  ageText(s, t, v.upSec);
  pRowEnd(s);
  pHint(s, t.pnlRestartHint);
  pFoldEnd(s);
}

void panelPage(String &s, const Shell &shell, const PanelView &v, void (*flush)(String &)) {
  const Strings &t = *shell.t;
  Shell sh = shell;
  sh.title = t.webTabs[TAB_PANEL];
  sh.tab = TAB_PANEL;
  sh.tabs = true;
  shellStart(s, sh);
  pForms(s, t, pPosted(v));
  pSide(s, t, v);
  flush(s);
  languageSection(s, t, *v.values);
  regionSection(s, t, *v.values);
  nightSection(s, t, *v.values);
  flush(s);
  feedbackSection(s, t, *v.values);
  flush(s);
  updateSection(s, sh, v);
  wifiSection(s, t, v);
  flush(s);
  pSection(s, "adv", 7, t.pnlAdvanced);
  s += F("<ul class=list>");
  mqttFold(s, t, v);
  hooksFold(s, t, v);
  flush(s);
  apiFold(s, t, v);
  memoryFold(s, t, v);
  s += F("</ul>");
  pSegments(s, "bd", t.pnlBoot, v.values->bootDetail, t.pnlBoots, BOOT_DETAIL_COUNT);
  pHint(s, t.pnlBootHint);
  s += F("</section><p id=ann class=vh aria-live=polite></p>");
  pSaveBar(s, t, "mb", pPosted(v));
  s += F("</div></div>");
  flush(s);
  shellEnd(s, sh, UNSAVED_SCRIPT, COPY_SCRIPT);
}
