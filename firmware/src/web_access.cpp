// SPDX-License-Identifier: AGPL-3.0-only
#include "web_access.h"

#include <stdio.h>
#include <string.h>

#include "posters.h"
#include "screen_list.h"  // screenFindSlug
#include "web_images.h"   // reinstallLink, resetFailedHeading

int wifiRows(WifiNet *nets, int n) {
  int k = 0;
  for (int i = 0; i < n; i++) {
    if (!nets[i].ssid[0]) continue;                       // hidden network
    int j = 0;
    while (j < k && strcmp(nets[j].ssid, nets[i].ssid) != 0) j++;
    if (j < k) {                                          // another access point of it
      if (nets[i].rssi > nets[j].rssi) nets[j] = nets[i];
      continue;
    }
    nets[k++] = nets[i];
  }
  // Insertion sort, strongest first; equal signals keep the scan's order
  for (int a = 1; a < k; a++)
    for (int b = a; b > 0 && nets[b].rssi > nets[b - 1].rssi; b--) {
      const WifiNet tmp = nets[b];
      nets[b] = nets[b - 1];
      nets[b - 1] = tmp;
    }
  return k < WIFI_ROWS_MAX ? k : WIFI_ROWS_MAX;
}

const char *wifiChosenName(const char *typed, const char *row) {
  if (typed && typed[0]) return typed;
  return row ? row : "";
}

int wifiAwaitScan(int (*poll)(), void (*pause)(), int pauses) {
  int n = poll();
  for (int i = 0; n == WIFI_SCAN_BUSY && i < pauses; i++) {
    pause();
    n = poll();
  }
  return n;
}

WifiScanPlan wifiScanPlan(int complete, bool asked, bool restarting) {
  if (restarting) return {false, 0};
  return {complete == WIFI_SCAN_NONE || asked, WIFI_SCAN_WAIT_PAUSES};
}

static void stateRow(String &s, const char *label, const String &value) {
  s += F("<div class=st><span>");
  s += label;
  s += F("</span><b>");
  s += value;
  s += F("</b></div>");
}

static void small(String &s, const char *text) {
  s += F("<small>");
  s += text;
  s += F("</small>");
}

int wifiBars(int16_t rssi) {
  return rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -75 ? 2 : 1;
}

// One radio row: the name wraps; a lock when it is secured, the bars and
// their word after it, hidden from a screen reader, which reads the hidden
// phrase instead ("Maison, excellent signal, secured"). pw: choosing it asks
// for a password (the script moves the field under it).
static void netRow(String &s, const Strings &t, const WifiNet &n, bool pw, bool checked) {
  const String name = escapeHtml(n.ssid);
  const int bars = wifiBars(n.rssi);
  const char *word = t.wifiSignal[bars - 1];
  s += F("<label class=op><input type=radio name=ssid value=\"");
  s += name;
  s += pw ? F("\" data-pw") : F("\"");
  s += checked ? F(" checked><span>") : F("><span>");
  s += name;
  s += F("</span><span class=vh>, ");
  formatText(s, n.open ? t.wifiSignalOpen : t.wifiSignalLocked, word);
  s += n.open ? F("</span><span class=num aria-hidden=true><i class=\"sb s")
              : F("</span><span class=\"num k\" aria-hidden=true><i class=\"sb s");
  s += (char)('0' + bars);
  s += F("\"></i>");
  s += word;
  s += F("</span></label>");
}

// The phone's time zone ("Europe/Paris") gives the panel a first city, so
// weather and clock are right without any other setting. Its region and clock
// give the units, under Panel's names (tu, cu, ck; region.h numbers them):
// rg() maps the region code to °F (the US and its territories, LR, MM, BS,
// BZ, KY, PW; else °C) and the currency (USD where the money is the US
// dollar, GBP for GB and the Crown Dependencies, CHF for CH and LI, else EUR);
// 12 h when the locale's hour cycle is h11 or h12. The save keeps an owner's
// choice (presetRegion). Without scripts the fields stay empty, the save
// ignores them, and works all the same. Then the password
// field: one, after the lists, which without scripts serves whichever row is
// chosen; with them it follows the chosen row when that one asks for a
// password, else it hides and is disabled, so a password typed for another
// row is not posted with the known network. Typing a name chooses the name
// row (the typed name wins anyway: wifiChosenName). The name field shows only
// with that row chosen.
static const char WIFI_SCRIPT[] =
    "try{document.getElementById('tz').value=Intl.DateTimeFormat().resolvedOptions().timeZone}"
    "catch(e){}"
    "function rg(c){return[/^(US|AS|GU|MP|PR|UM|VI|LR|MM|BS|BZ|KY|PW)$/.test(c)?1:0,"
    "/^(US|AS|GU|MP|PR|UM|VI|EC|SV|PA|TL|FM|MH|PW|BQ|TC|VG|LR)$/.test(c)?1:"
    "/^(GB|GG|JE|IM)$/.test(c)?2:/^(CH|LI)$/.test(c)?3:0]}"
    "try{var u=rg(new Intl.Locale(navigator.language).maximize().region);"
    "document.getElementById('tu').value=u[0];document.getElementById('cu').value=u[1]}catch(e){}"
    "try{var h=Intl.DateTimeFormat([],{hour:'numeric'}).resolvedOptions().hourCycle;"
    "if(h)document.getElementById('ck').value=h=='h11'||h=='h12'?1:0}catch(e){}"
    "var b=document.getElementById('pb'),p=document.getElementById('pw'),"
    "o=document.getElementById('on'),N=document.getElementById('nb'),R=document.getElementsByName('ssid');"
    "function w(){N.hidden=!R[R.length-1].checked;for(var i=0;i<R.length;i++)if(R[i].checked){var r=R[i],n=r.hasAttribute('data-pw');"
    "b.hidden=p.disabled=!n;if(n)(r.value?r.parentNode:o).after(b);return}}"
    "o.form.addEventListener('change',function(e){if(e.target.name=='ssid')w()});"
    "o.addEventListener('input',function(){if(o.value&&!R[R.length-1].checked){R[R.length-1].checked=true;w()}});"
    "w()";

void wifiPage(String &s, const Shell &shell, const WifiPageView &v) {
  const Strings &t = *shell.t;
  const bool setup = v.link == LINK_SETUP;
  Shell sh = shell;
  sh.title = t.wifiTitle;
  sh.tab = TAB_NONE;
  if (setup) {
    sh.tabs = false;
    sh.host = nullptr;
  }
  shellStart(s, sh);
  s += F("<div class=narrow style=\"padding-top:16px\"><h1>");
  s += t.wifiTitle;
  s += F("</h1><p class=lead>");
  s += t.wifiSubtitle;
  s += F("</p><div class=sec style=\"margin-top:24px\">");
  if (v.message) {
    // The longest message with a 32-byte name escaped to 192 bytes
    char line[400];
    snprintf(line, sizeof(line), v.message,
             escapeHtml(v.messageName ? v.messageName : "").c_str());
    if (v.message == t.wifiMsgBadSsid || v.message == t.wifiMsgBadPassword) errorNotice(s, line);
    else                                                                    notice(s, line);
  }
  const char *saved = v.saved ? v.saved : "";
  switch (v.link) {
    case LINK_SETUP:     stateRow(s, t.wifiState, t.wifiStateSetup); break;
    case LINK_CONNECTED: stateRow(s, t.wifiState, String(t.wifiStateConnected) + " " +
                                                  escapeHtml(v.ip ? v.ip : "")); break;
    default:             stateRow(s, t.wifiState, t.wifiStateOffline); break;
  }
  if (v.lastTry != TRY_NONE)
    stateRow(s, t.wifiLastTry, escapeHtml(saved) + " · " +
                               (v.lastTry == TRY_REFUSED ? t.wifiRefused : t.wifiNotFound));
  s += F("</div>");

  // The saved network first, on its own and chosen: Continue then retries
  // it with the stored password (web.cpp, wifiRetrySaved), unless that one
  // was refused. The others fold under it, open when it is not in range.
  // Radio rows, so the page works without scripts; the typed name wins
  // (wifiChosenName), so the name field is never filled in advance.
  int known = -1;
  for (int i = 0; i < v.count && known < 0; i++)
    if (saved[0] && strcmp(v.rows[i].ssid, saved) == 0) known = i;
  s += F("<form method=post action=\"/wifi\" class=sec><input type=hidden name=tz id=tz>"
         "<input type=hidden name=tu id=tu><input type=hidden name=cu id=cu>"
         "<input type=hidden name=ck id=ck>");
  if (saved[0]) {
    s += F("<h2>");
    s += t.wifiKnown;
    if (known >= 0) {
      s += F("</h2><div class=nl>");
      const WifiNet &k = v.rows[known];
      netRow(s, t, k, !k.open && v.lastTry == TRY_REFUSED, true);
      s += F("</div>");
    } else {
      s += F("</h2><p class=hint>");
      formatText(s, t.wifiAway, escapeHtml(saved).c_str());
      s += F("</p>");
    }
  }
  s += known >= 0 ? F("<details class=nw><summary>") : F("<details class=nw open><summary>");
  s += saved[0] ? t.wifiOthers : t.wifiNetworks;
  s += F("</summary>");
  if (v.count == 0) small(s, t.wifiNoneYet);   // the Refresh link is below
  s += F("<div class=nl>");
  for (int i = 0; i < v.count; i++)
    if (i != known) netRow(s, t, v.rows[i], !v.rows[i].open, false);
  s += known >= 0 ? F("<label class=op><input type=radio name=ssid value=\"\" data-pw><span>")
                  : F("<label class=op><input type=radio name=ssid value=\"\" data-pw checked><span>");
  s += t.wifiAnother;
  s += F("</span></label><div id=nb><label for=on>");
  s += t.wifiName;
  s += F("</label><input id=on name=other maxlength=32 autocomplete=off></div></div></details>"
         "<div id=pb><label for=pw>");
  s += t.wifiPassword;
  s += F("</label><input id=pw name=mdp type=password maxlength=63 autocomplete=current-password>");
  small(s, t.wifiPasswordHint);
  s += F("</div>");
  small(s, t.wifiBand);
  s += F("<button class=p style=\"width:100%;margin-top:24px\">");
  s += t.wifiContinue;
  s += F("</button>");
  small(s, t.wifiFooter);
  s += F("</form><div class=sec><a class=go href=\"/wifi?scan=1\">");
  s += t.wifiRefresh;
  s += F("</a></div></div>");
  shellEnd(s, sh, WIFI_SCRIPT);
}

static const char *const RETURN_PATHS[] = {"/", "/screens", "/images", "/panel", "/panel/feedback", "/wifi"};

// A screen's page: "/screens/" and a slug screenFindSlug knows, nothing
// more, so what is returned holds only [a-z-]
static const char SCREEN_PAGES[] = "/screens/";

const char *returnPath(const char *asked) {
  if (!asked) return RETURN_PATHS[0];
  for (const char *p : RETURN_PATHS)
    if (strcmp(asked, p) == 0) return p;
  const size_t n = sizeof(SCREEN_PAGES) - 1;
  if (!strncmp(asked, SCREEN_PAGES, n) && screenFindSlug(asked + n) >= 0) return asked;
  return RETURN_PATHS[0];
}

// The stage block of a page before the others: the panel's frame (a cell of
// the poster sheet, twice the size), named for a screen reader, then the
// heading at display size with its live line
static void stageBlock(String &s, int slot, const char *frameName, const char *caption,
                       const char *heading, const char *live) {
  s += F("<section class=stage aria-labelledby=act><div><div role=img aria-label=\"");
  s += frameName;
  s += F("\">");
  poster(s, slot, true);
  s += F("</div>");
  if (caption) small(s, caption);
  s += F("</div><div class=bill><div class=act><h1 id=act>");
  s += heading;
  s += F("</h1><p class=live>");
  s += live;
  s += F("</p></div></div></section>");
}

// Six one-digit boxes, built from the one field the page carries (without
// scripts that field works alone, as always). The field is hidden and holds
// the boxes' digits; a digit typed moves on, Backspace on an empty box goes
// back, a whole code (pasted, or a phone's one-time-code suggestion in the
// first box) fills every box from the first, and the sixth digit posts.
// The boxes take the field's error marks and its label (a group named by
// it), each named by its place (data-d, "Digit %d of 6").
static const char CODE_SCRIPT[] =
    "var c=document.getElementById('c'),f=c.form,g=document.createElement('div'),B=[],i;"
    "function v(){return B.map(function(x){return x.value}).join('')}"
    "function put(k,d){for(var j=0;j<d.length&&k<6;j++)B[k++].value=d[j];c.value=v();B[Math.min(k,5)].focus();if(c.value.length==6)f.requestSubmit?f.requestSubmit():f.submit()}"
    "g.className='otp';g.setAttribute('role','group');g.setAttribute('aria-labelledby','cl');"
    "for(i=0;i<6;i++){var x=document.createElement('input');x.inputMode='numeric';x.autocomplete=i?'off':'one-time-code';"
    "x.setAttribute('aria-label',c.dataset.d.replace('%d',i+1));"
    "['aria-invalid','aria-describedby'].forEach(function(a){if(c.hasAttribute(a))x.setAttribute(a,c.getAttribute(a))});"
    "x.addEventListener('focus',function(){this.select()});"
    "x.addEventListener('input',function(){var k=B.indexOf(this),d=this.value.replace(/\\D/g,'');this.value='';if(d.length>5)k=0;if(d)put(k,d);else c.value=v()});"
    "x.addEventListener('keydown',function(e){var k=B.indexOf(this);if(e.key=='Backspace'&&!this.value&&k){B[k-1].value='';B[k-1].focus();c.value=v();e.preventDefault()}else if(e.key=='ArrowLeft'&&k)B[k-1].focus();else if(e.key=='ArrowRight'&&k<5)B[k+1].focus()});"
    "x.addEventListener('paste',function(e){var d=(e.clipboardData.getData('text')||'').replace(/\\D/g,'');if(d){e.preventDefault();put(d.length>5?0:B.indexOf(this),d)}});"
    "B.push(x);g.appendChild(x)}"
    "B[0].id='c';c.id='';c.type='hidden';c.parentNode.insertBefore(g,c);if(c.autofocus)B[0].focus();"
    "f.addEventListener('submit',function(e){c.value=v();if(c.value.length<6){e.preventDefault();for(i=0;B[i].value;i++);B[i].focus()}})";

void signInPage(String &s, const Shell &shell, const SignInView &v) {
  const Strings &t = *shell.t;
  Shell sh = shell;
  sh.title = t.signInTitle;
  sh.tab = TAB_NONE;
  sh.tabs = false;
  shellStart(s, sh);
  stageBlock(s, POSTER_SIGN_IN, t.signInFrame, t.signInCaption, t.signInTitle, t.signInLive);
  s += F("<div class=narrow style=\"padding-top:24px\"><p class=lead id=hi>");
  s += t.signInHint;
  s += F("</p><form method=post action=\"/login\" style=\"margin-top:24px\">"
         "<input type=hidden name=next value=\"");
  s += returnPath(v.next);   // a listed path or a screen's page, [a-z/-] only: nothing to escape
  s += F("\">");
  if (v.wrong) errorNotice(s, t.signInWrong, "e1");
  s += F("<label for=c id=cl>");
  s += t.signInCode;
  // The wrong code is not typed back: the field starts empty either way
  s += F("</label><input id=c class=code name=code inputmode=numeric "
         "autocomplete=one-time-code maxlength=6 pattern=\"[0-9]{6}\" required data-d=\"");
  s += t.signInDigit;
  s += F("\" ");
  s += v.wrong ? F("aria-describedby=\"e1 hi\" aria-invalid=true autofocus>")
               : F("aria-describedby=\"hi\">");
  s += F("<button class=p style=\"width:100%;margin-top:16px\">");
  s += t.signInButton;
  s += F("</button></form><small style=\"margin-top:24px\">");
  s += t.signInReload;
  s += F("</small></div>");
  shellEnd(s, sh, CODE_SCRIPT);
}

// One language's half of the locked page
static void lockedHalf(String &s, const Strings &t, bool knob, bool french) {
  s += french ? F("<section class=sec lang=fr aria-labelledby=h-f><h2 id=h-f>")
              : F("<section class=sec aria-labelledby=h-e><h2 id=h-e>");
  s += t.resetFailedTitle;
  s += F("</h2><p>");
  s += resetFailedText(t, knob);
  s += F("</p>");
  if (!knob) reinstallLink(s, t);
  s += F("</section>");
}

void lockedPage(String &s, const Shell &shell, bool knob) {
  const Strings &en = STRINGS[LANG_EN];
  String title;
  resetFailedHeading(title);
  Shell sh = shell;
  sh.t = &en;
  sh.title = title.c_str();
  sh.tab = TAB_NONE;
  sh.tabs = false;
  shellStart(s, sh);
  // The panel's own words, upper case as it draws them
  String panel = en.locked[0];
  panel += F(" \u00B7 ");
  panel += en.locked[1];
  panel += F(" \u00B7 ");
  panel += en.locked[2];
  // Both languages on the live line, the French half marked so a screen
  // reader does not read it with the English voice
  String live = en.lockedLive;
  live += F(" \u00B7 <span lang=fr>");
  live += STRINGS[LANG_FR].lockedLive;
  live += F("</span>");
  stageBlock(s, POSTER_LOCKED, panel.c_str(), nullptr, panel.c_str(), live.c_str());
  s += F("<div class=q>");
  lockedHalf(s, en, knob, false);
  lockedHalf(s, STRINGS[LANG_FR], knob, true);
  s += F("</div>");
  shellEnd(s, sh);
}

ErrorKind errorKind(int code) {
  switch (code) {
    case 404: return ERR_NOT_FOUND;
    case 401:
    case 403: return ERR_REFUSED;
    case 413: return ERR_TOO_LARGE;
    case 400:
    case 415: return ERR_BAD_FORM;
    default:  return ERR_BUSY;
  }
}

// No frame: the block holds the words alone, across the whole stage
void errorPage(String &s, const Shell &shell, int code, Tab back) {
  const Strings &t = *shell.t;
  const ErrorKind k = errorKind(code);
  Shell sh = shell;
  sh.title = t.errTitle[k];
  sh.tab = TAB_NONE;
  shellStart(s, sh);
  s += F("<section class=stage aria-labelledby=act><div class=bill><div class=act><h1 id=act>");
  s += t.errTitle[k];
  char live[24];
  snprintf(live, sizeof(live), "</h1><p class=live>%d", code);
  s += live;
  s += F("</p></div><p>");
  s += t.errText[k];
  s += F("</p></div></section><p class=sec><a class=go href=\"");
  s += TAB_HREF[back];
  s += F("\">");
  formatText(s, t.errBack, t.webTabs[back]);
  s += F("</a></p>");
  shellEnd(s, sh);
}
