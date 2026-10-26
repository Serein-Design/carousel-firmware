// SPDX-License-Identifier: AGPL-3.0-only
#include "web_stage.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <new>

#include "agent_bridge.h"

static_assert(AGENT_STATE_COUNT == sizeof(Strings::stgStates) / sizeof(const char *),
              "one word per session state");

static bool plays(const ScreenList &l, uint8_t i, bool portrait) {
  return screenOn(l, i) && screenFits(i, portrait);
}

StagePlace stagePlace(const ScreenList &l, uint8_t current, bool portrait) {
  StagePlace p = {0, 0};
  for (uint8_t k = 0; k < SCREEN_COUNT; k++) {
    const uint8_t i = l.order[k];
    if (!plays(l, i, portrait)) continue;
    p.count++;
    if (i == current) p.n = p.count;
  }
  return p;
}

// As ScreenCursor::nextFitting walks it: the list's order after current,
// wrapping
int stageNext(const ScreenList &l, uint8_t current, bool portrait, uint32_t skip) {
  const int place = screenPlace(l, current);
  for (int n = 1; n <= SCREEN_COUNT; n++) {
    const uint8_t i = l.order[(place + n + SCREEN_COUNT) % SCREEN_COUNT];
    if (i != current && plays(l, i, portrait) && !((skip >> i) & 1u)) return i;
  }
  return -1;
}

int stageStrip(const ScreenList &l, uint8_t current, bool portrait, uint8_t *out, int max) {
  uint8_t fit[SCREEN_COUNT];
  int count = 0, at = -1;
  for (uint8_t k = 0; k < SCREEN_COUNT; k++) {
    const uint8_t i = l.order[k];
    if (!plays(l, i, portrait)) continue;
    if (i == current) at = count;
    fit[count++] = i;
  }
  const int n = count < max ? count : max;
  // Not among them (a request in flight): from the first
  const int start = at < 0 ? 0 : (at - 1 + count) % count;
  for (int j = 0; j < n; j++) out[j] = fit[(start + j) % count];
  return n;
}

static const char *screenName(const Strings &t, uint8_t i) { return t.*SCREEN_INFO[i].name; }

ShowOutcome showOutcome(const ScreenList &l, const char *name, bool portrait) {
  if (strlen(name) > 23) return SHOW_UNKNOWN;
  switch (screenRequestStatus(l, name, portrait)) {
    case 200: return SHOW_OK;
    case 409: return SHOW_OFF;
    default:  return screenFind(name) < 0 ? SHOW_UNKNOWN : SHOW_OTHER;
  }
}

StageNotice textNotice(bool on, bool fits) {
  return !on ? STAGE_TEXT_OFF : !fits ? STAGE_TEXT_PORTRAIT : STAGE_NO_NOTICE;
}

// Firmware strings go through escapeHtml too: a stray quote in a translation
// must not end an attribute
static void text(String &s, const char *t) { s += escapeHtml(t); }

// A firmware format, filled and escaped. Measured first and written on the
// heap: the page is built on the web task's stack, a few small frames deep.
static void textf(String &s, const char *format, ...) {
  va_list ap, again;
  va_start(ap, format);
  va_copy(again, ap);
  const int n = vsnprintf(nullptr, 0, format, ap);
  va_end(ap);
  std::unique_ptr<char[]> out(n < 0 ? nullptr : new (std::nothrow) char[n + 1]);
  if (out) {
    vsnprintf(out.get(), n + 1, format, again);
    text(s, out.get());
  }
  va_end(again);
}

static void dataAttr(String &s, const char *name, const String &value) {
  s += ' ';
  s += name;
  s += F("=\"");
  s += escapeHtml(value);
  s += '"';
}

// The names of the screens on and fitting, list order, comma-separated (no
// name holds a comma: the stored list is written the same way). english:
// the names /api/info and /preview's headers give; else the page's.
static String names(const Strings &t, const StageView &v, bool english) {
  String r;
  for (uint8_t k = 0; k < SCREEN_COUNT; k++) {
    const uint8_t i = v.list.order[k];
    if (!plays(v.list, i, v.portrait)) continue;
    if (r.length()) r += ',';
    r += english ? screenNameEn(i) : screenName(t, i);
  }
  return r;
}

// webRotate[1..6]'s words for the interval; "" for none offered
static const char *everyWord(const Strings &t, uint16_t rotateSec) {
  for (int c = 1; c < ROTATE_CHOICE_COUNT; c++)
    if (ROTATE_CHOICES[c] == rotateSec) return t.stgEvery[c - 1];
  return "";
}

// "Next: <b>NAME</b><span class=ni> in 14 s · a new screen every 20 s</span>":
// stgNext split after its %s, so the name stands out and the rest can go
// under it on a wide screen
static void nextLine(String &s, const Strings &t, const StageView &v) {
  s += F("<p class=next>");
  const int next = stageNext(v.list, v.current, v.portrait, v.cannotRotate);
  if (v.rotateSec == 0) {
    text(s, t.stgKnobOnly);
  } else if (next < 0 || v.nextIn < 0) {
    text(s, t.stgNoOther);
  } else {
    const char *name = strstr(t.stgNext, "%s");
    if (!name) name = t.stgNext + strlen(t.stgNext);   // test_i18n pins the %s
    s += escapeHtml(String(t.stgNext).substring(0, name - t.stgNext));
    s += F("<b>");
    text(s, screenName(t, (uint8_t)next));
    s += F("</b><span class=ni>");
    textf(s, *name ? name + 2 : "", (unsigned long)v.nextIn);
    s += F(" · ");
    text(s, everyWord(t, v.rotateSec));
    s += F("</span>");
  }
  s += F("</p>");
}

static void stageBlock(String &s, const Strings &t, const StageView &v) {
  const char *name = screenName(t, v.current);
  s += F("<section class=stage aria-labelledby=act");
  dataAttr(s, "data-o", names(t, v, true));
  dataAttr(s, "data-l", names(t, v, false));
  dataAttr(s, "data-p", t.stgPlace);
  dataAttr(s, "data-x", t.stgNext);
  dataAttr(s, "data-e", everyWord(t, v.rotateSec));
  dataAttr(s, "data-k", t.stgKnobOnly);
  dataAttr(s, "data-z", t.stgNoOther);
  dataAttr(s, "data-a", t.stgPreviewAlt);
  dataAttr(s, "data-s", t.stgSent);
  dataAttr(s, "data-f", t.stgFailed);
  // The panel's own frame, upright: 32 x 64 in portrait
  s += v.portrait ? F("><div><img class=led src=\"/preview\" width=32 height=64 alt=\"")
                  : F("><div><img class=led src=\"/preview\" width=64 height=32 alt=\"");
  textf(s, t.stgPreviewAlt, name);
  s += F("\"><noscript><small>");
  text(s, t.stgStill);
  s += F("</small></noscript></div><div class=bill><div class=act><h1 id=act>");
  text(s, name);
  s += F("</h1><p class=\"num live\">");
  text(s, t.stgLive);
  s += F(" · ");
  const StagePlace p = stagePlace(v.list, v.current, v.portrait);
  textf(s, t.stgPlace, (unsigned)p.n, (unsigned)p.count);
  s += F("</p></div>");
  nextLine(s, t, v);
  s += F("</div></section>");
}

static void put(StageHeader &h, const char *name, const char *format, ...) {
  h.name = name;
  va_list ap;
  va_start(ap, format);
  vsnprintf(h.value, sizeof(h.value), format, ap);
  va_end(ap);
}

int stageHeaders(const StageView &v, StageHeader *out) {
  int n = 0;
  put(out[n++], "X-Screen", "%s", screenNameEn(v.current));
  put(out[n++], "X-Brightness", "%d", v.level);
  if (v.rotateSec == 0) return n;
  put(out[n++], "X-Next-In", "%ld", (long)v.nextIn);
  const int next = stageNext(v.list, v.current, v.portrait, v.cannotRotate);
  if (next >= 0) put(out[n++], "X-Next", "%s", screenNameEn((uint8_t)next));
  return n;
}

static void sectionStart(String &s, const char *id, const char *title) {
  s += F("<section class=sec aria-labelledby=");
  s += id;
  s += F("><h2 id=");
  s += id;
  s += '>';
  text(s, title);
  s += F("</h2>");
}

// The notice of an action, in its section, above its form: refusals as an
// alert, a message stored but not shown as a note
static void actionNotice(String &s, const Strings &t, const StageView &v, bool show) {
  const char *format = nullptr;
  bool refused = true;
  switch (v.notice) {
    case STAGE_SHOW_OFF:      if (show) format = t.stgShowOff; break;
    case STAGE_SHOW_OTHER:    if (show) format = t.stgShowOther; break;
    case STAGE_SHOW_UNKNOWN:  if (show) format = t.stgShowUnknown; break;
    case STAGE_TEXT_OFF:      if (!show) { format = t.stgTextOff; refused = false; } break;
    case STAGE_TEXT_PORTRAIT: if (!show) { format = t.stgTextPortrait; refused = false; } break;
    case STAGE_TEXT_FAILED:   if (!show) format = t.stgTextFailed; break;
    default: break;
  }
  if (!format) return;
  String html;
  textf(html, format, v.noticeName ? v.noticeName : "");
  if (refused) errorNotice(s, html.c_str());
  else         notice(s, html.c_str());
}

// The screens on and fitting, list order, posted by their English name;
// the one the rotation brings on next is preselected
static void bringOn(String &s, const Strings &t, const StageView &v) {
  sectionStart(s, "h-b", t.stgBring);
  actionNotice(s, t, v, true);
  s += F("<form method=post action=\"/show\" class=row><div><label for=sc>");
  text(s, t.stgScreen);
  s += F("</label><select id=sc name=s>");
  const int next = stageNext(v.list, v.current, v.portrait, v.cannotRotate);
  for (uint8_t k = 0; k < SCREEN_COUNT; k++) {
    const uint8_t i = v.list.order[k];
    if (!plays(v.list, i, v.portrait)) continue;
    s += F("<option value=\"");
    text(s, screenNameEn(i));
    s += i == next ? F("\" selected>") : F("\">");
    text(s, screenName(t, i));
    s += F("</option>");
  }
  s += F("</select></div><button class=p>");
  text(s, t.webImgShow);
  s += F("</button></form></section>");
}

// One step button: posts its target level; at the end, disabled (and
// posting the level it is at)
static void stepButton(String &s, const char *cls, int target, int levels, const char *name) {
  const bool off = target < 0 || target >= levels;
  char value[28];
  snprintf(value, sizeof(value), " name=b value=\"%d", off ? (target < 0 ? 0 : levels - 1) : target);
  s += F("<button");
  s += cls;
  s += value;
  s += F("\" aria-label=\"");
  text(s, name);
  s += off ? F("\" disabled></button>") : F("\"></button>");
}

// Where the script answers a press in place: empty until then
static const char LIVE_REGION[] = "<div aria-live=polite></div>";

// The level as six steps, told in words too; the night line under them
static void brightness(String &s, const Strings &t, const StageView &v) {
  sectionStart(s, "h-l", t.stgLights);
  const int step = v.level + 1;
  s += LIVE_REGION;
  s += F("<form method=post action=\"/brightness\" class=lv>");
  stepButton(s, "", v.level - 1, v.levels, t.stgDimmer);
  s += F("<span class=steps role=img aria-label=\"");
  textf(s, t.stgSteps, step, v.levels);
  s += F("\">");
  for (int i = 0; i < v.levels; i++) s += i < step ? F("<i class=on></i>") : F("<i></i>");
  s += F("</span>");
  stepButton(s, " class=pl", v.level + 1, v.levels, t.stgBrighter);
  s += F("</form><small>");
  const unsigned from = v.nightStart, to = v.nightEnd;
  if (from == to) {
    textf(s, t.stgNightNone, step, v.levels);
  } else if (!v.nightBrightness) {
    textf(s, t.stgNightDark, step, v.levels, from, to);
  } else {
    // Out of 255, as Panel's choices show it
    char level[12];
    snprintf(level, sizeof(level), "%u %%", (unsigned)((v.nightBrightness * 100 + 254) / 255));
    textf(s, t.stgNightDims, step, v.levels, from, to, level);
  }
  s += F(" <a href=\"/panel#night\">");
  text(s, t.webNightMode);
  s += F("</a></small></section>");
}

static void saySomething(String &s, const Strings &t, const StageView &v) {
  sectionStart(s, "h-m", t.stgSay);
  actionNotice(s, t, v, false);
  s += LIVE_REGION;
  s += F("<form method=post action=\"/text\" class=row><div><label for=tx>");
  text(s, t.webMessage);
  s += F("</label><input id=tx name=msg maxlength=95 value=\"");
  text(s, v.message ? v.message : "");
  s += F("\"></div><button>");
  text(s, t.stgSend);
  s += F("</button></form><small>");
  text(s, t.stgSayHint);
  s += F("</small></section>");
}

static void strip(String &s, const Strings &t, const StageView &v) {
  sectionStart(s, "h-o", t.webTabs[TAB_ORDER]);
  s += F("<ol class=strip>");
  uint8_t shown[STAGE_STRIP_MAX];
  const int n = stageStrip(v.list, v.current, v.portrait, shown, STAGE_STRIP_MAX);
  for (int j = 0; j < n; j++) {
    const uint8_t i = shown[j];
    const bool now = i == v.current;
    // The outline and the colour come from .strip [aria-current]; the words
    // from the hidden text
    s += now ? F("<li aria-current=true>") : F("<li>");
    poster(s, i, false);
    char place[40];
    snprintf(place, sizeof(place), "<span class=num>%u</span><b>",
             (unsigned)stagePlace(v.list, i, v.portrait).n);
    s += place;
    text(s, screenName(t, i));
    if (now) {
      s += F("<span class=vh>");
      text(s, t.stgNowSr);
      s += F("</span>");
    }
    s += F("</b></li>");
  }
  s += F("</ol><a class=go href=\"/screens\">");
  textf(s, t.stgAllScreens, (unsigned)SCREEN_COUNT);
  s += F("</a></section>");
}

void ageText(String &s, const Strings &t, uint32_t sec) {
  if (sec < 60)        textf(s, t.stgAgoSec, (unsigned long)sec);
  else if (sec < 3600) textf(s, t.stgAgoMin, (unsigned long)(sec / 60));
  else                 textf(s, t.stgAgoHour, (unsigned long)(sec / 3600));
}

static void sessionsBlock(String &s, const Strings &t, const StageView &v) {
  sectionStart(s, "h-c", t.stgSessions);
  if (v.sessionCount <= 0) {
    s += F("<p class=hint>");
    text(s, t.webWaitingHint);
    s += F("</p><a class=go href=\"/panel#hooks\">");
    text(s, t.stgSetUpHooks);
    s += F("</a></section>");
    return;
  }
  for (int r = 0; r < v.sessionCount; r++) {
    const SessionRow &row = v.sessions[r];
    char tint[8];
    snprintf(tint, sizeof(tint), "#%02x%02x%02x", row.tint[0], row.tint[1], row.tint[2]);
    s += F("<div class=st><span class=sn><i class=tn style=\"background:");
    s += tint;
    s += F("\" aria-hidden=true></i>");
    char name[sizeof(row.name)];
    snprintf(name, sizeof(name), "%s", row.name);   // ends inside its field, whatever was copied
    text(s, name);
    s += F("</span><b>");
    text(s, row.state < AGENT_STATE_COUNT ? t.stgStates[row.state] : "");
    s += F(" <span class=num>· ");
    ageText(s, t, row.ageSec);
    s += F("</span></b></div>");
  }
  s += F("<small>");
  text(s, t.stgSessionsHint);
  s += F("</small></section>");
}

void stagePage(String &s, const Shell &shell, const StageView &v, void (*flush)(String &)) {
  const Strings &t = *shell.t;
  Shell sh = shell;
  sh.title = t.webTabs[TAB_STAGE];
  sh.tab = TAB_STAGE;
  sh.tabs = true;
  shellStart(s, sh);
  stageBlock(s, t, v);
  flush(s);
  s += F("<div class=q>");
  bringOn(s, t, v);
  brightness(s, t, v);
  s += F("</div>");
  flush(s);
  strip(s, t, v);
  s += F("<div class=q>");
  saySomething(s, t, v);
  flush(s);
  sessionsBlock(s, t, v);
  s += F("</div>");
  flush(s);
  shellEnd(s, sh, STAGE_SCRIPT);
}

// Every second, while nothing is in flight (or for 5 s), a new still from
// /preview (the address changes, so no cache answers), shown from a blob URL
// (the one before revoked); its headers, the state of that frame
// (stageHeaders), rewrite the name, the place, the next line and the still's
// name, and its brightness the steps, the level line and the ends (a knob
// press), unless a press of ours is in flight or came since the read went
// out. A screen the page's list lacks (changed on another page) shows by its
// English name, its place without a number. The formats and names come from
// the stage block's data attributes, with \x25 for the percent sign; the
// list and the interval are the page's. Hidden: nothing, until the page
// shows again. No aria-live here: the rotation would speak every few seconds.
// Dimmer, Brighter and Send post in place, as their forms would (the same
// route and body, the 303 not followed): a press moves the steps at once and
// never waits for the one before; the last answer wins; a refusal puts back
// /api/info's level and says why (the answer's notice, else data-f) in the
// region above the form. Bring on stays a plain post. The level is the first
// number of the steps' words and of the night line (test_web_stage pins it).
const char STAGE_SCRIPT[] =
    "let W=document,T='textContent',[b,m,h,p,x,F,X]=['.stage','img','h1','.live','.next','.lv','#tx'"
    "].map(e=>W.querySelector(e)),d=b.dataset,[o,l]=[d.o,d.l].map(e=>e.split(',')),D=' · ',v=p[T].s"
    "plit(D)[0],f=(s,a)=>s.replace(/\\x25\\w+/,a),E=(e,c,n)=>(e=W.createElement(e),e[T]=c,e.classNam"
    "e=n,e),t,i=0,K=0,j,S=[...F.querySelectorAll('i')],[B,C]=F.querySelectorAll('button'),A=F.queryS"
    "elector('span'),Y=F.nextSibling.firstChild,n=(e,a)=>e.replace(/\\d+/,a+1),z=a=>{let N=S.length-"
    "1;S.map((e,k)=>e.className=k>a?'':'on');B.value=a&&a-1;C.value=a<N?a+1:N;B.disabled=!a;C.disabl"
    "ed=a>=N;A.ariaLabel=n(A.ariaLabel,a);Y[T]=n(Y[T],a)},H=(F,k)=>F.onsubmit=e=>{e.preventDefault()"
    ";let s=e.submitter,M=F.previousSibling,q=F.q=-~F.q,P=new URLSearchParams(k?{b:s.value}:new Form"
    "Data(F)),w=(c,e)=>{M.className=c;M[T]=e};K+=k;k&&(z(+s.value),s.disabled&&(s==B?C:B).focus());f"
    "etch(F.action,{method:'post',body:P,redirect:'manual'}).then(y=>y.type[0]=='o'||y.text().then(e"
    "=>new DOMParser().parseFromString(e,'text/html').querySelector('.er,.ok'))).catch(()=>0).then(y"
    "=>{K-=k;let O=y==1||y&&y.className=='ok',e=y&&y[T];q-F.q||(O?k?w('vh',A.ariaLabel):(w('ok',e||d"
    ".s),X.value=''):(w('er',e||d.f),k&&fetch('/api/info').then(y=>y.json()).then(a=>K||z(a.brightne"
    "ss),()=>0)))})},u=y=>{let G=e=>y.headers.get('X-'+e),a=G('Screen'),k=o.indexOf(a),q=G('Next'),N"
    "=G('Next-In'),c=G('Brightness'),s=d.x.split(/\\x25s/);a&&(m.alt=f(d.a,h[T]=k<0?a:l[k]),p[T]=k<0"
    "?v:v+D+f(f(d.p,k+1),o.length),x.replaceChildren(...!(N&&d.e)?[d.k]:!q||N<0?[d.z]:[s[0],E('b',(k"
    "=o.indexOf(q))<0?q:l[k],''),E('span',f(s[1],N)+D+d.e,'ni')]));c&&(K||j!=F.q||z(+c))},g=()=>{cle"
    "arInterval(t);W.hidden||(t=setInterval(()=>i-->0||(i=5,j=F.q,fetch('/preview?t='+Date.now()).th"
    "en(y=>y.ok&&y.blob().then(b=>{let c=m.src;m.src=URL.createObjectURL(b);URL.revokeObjectURL(c);u"
    "(y)})).catch(()=>0).then(()=>i=0)),1e3))};H(F,1);H(X.form,0);W.onvisibilitychange=g;g()";
