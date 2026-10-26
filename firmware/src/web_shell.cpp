// SPDX-License-Identifier: AGPL-3.0-only
#include "web_shell.h"

#include <stdio.h>
#include <string.h>

String escapeHtml(const String &s) {
  String r;
  r.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    const char c = s[i];
    switch (c) {
      case '&':  r += F("&amp;");  break;
      case '<':  r += F("&lt;");   break;
      case '>':  r += F("&gt;");   break;
      case '"':  r += F("&quot;"); break;
      case '\'': r += F("&#39;");  break;
      default:   r += c;           break;
    }
  }
  return r;
}

void formatText(String &s, const char *fmt, const char *a, const char *b, const char *c) {
  const char *args[] = {a, b, c};
  int n = 0;
  for (const char *p = fmt; *p; p++) {
    if (p[0] == '%' && p[1] == 's' && n < 3) {
      s += args[n++];
      p++;
    } else {
      s += *p;
    }
  }
}

size_t assetPath(char *out, size_t size, const char *version, const char *file) {
  const int n = snprintf(out, size, "/s/%s/%s", version, file);
  return n < 0 ? 0 : (size_t)n;
}

const char *const TAB_HREF[] = {"/", "/screens", "/images", "/panel"};

void shellStart(String &s, const Shell &sh) {
  const Strings &t = *sh.t;
  s += F("<!doctype html><html lang=");
  s += t.code;
  s += F("><head><meta charset=utf-8>"
         "<meta name=viewport content=\"width=device-width,initial-scale=1\"><title>");
  // Page names are the firmware's own strings; escaped all the same, since
  // a format with a stray character would otherwise end the title early
  char title[96];
  snprintf(title, sizeof(title), t.webTitle, sh.title ? sh.title : "");
  s += escapeHtml(title);
  s += F("</title>");
  char css[48];
  assetPath(css, sizeof(css), sh.version, "c.css");
  s += F("<link rel=stylesheet href=\"");
  s += escapeHtml(css);
  s += F("\"></head><body><a class=skip href=\"#m\">");
  s += t.webSkip;
  // Without tabs, home is not a page this visitor can open: the brand points
  // at the content instead
  s += F("</a><header><div class=w><a class=brand href=\"");
  s += sh.tabs ? F("/") : F("#m");
  s += F("\">Carousel</a><span class=host>");
  s += sh.host ? escapeHtml(sh.host) : String(t.webSetupBrand);
  s += F("</span></div></header>");
  if (sh.tabs) {
    s += F("<nav aria-label=\"");
    s += t.webNavName;
    s += F("\"><div class=w>");
    for (int i = TAB_STAGE; i <= TAB_PANEL; i++) {
      s += F("<a href=\"");
      s += TAB_HREF[i];
      s += '"';
      if (i == sh.tab) s += F(" aria-current=page");
      s += '>';
      s += t.webTabs[i];
      s += F("</a>");
    }
    s += F("</div></nav>");
  }
  s += F("<main id=m><div class=w>");
}

// AGPL section 13: whoever uses the panel over the network is offered its
// source. And the compatibility disclaimer.
void shellEnd(String &s, const Shell &sh, const char *script, const char *more) {
  s += F("</div></main><footer><div class=w>"
         "<a href=\"https://github.com/Serein-Design/carousel-firmware\">");
  s += sh.t->webSource;
  s += F("</a> · ");
  s += escapeHtml(sh.version);
  s += F(" · AGPL-3.0");
  // CC BY 4.0 asks for the credit where the data shows: Stage's preview,
  // Running order's WEATHER and its city
  if (sh.tabs && (sh.tab == TAB_STAGE || sh.tab == TAB_ORDER)) {
    s += F("<small>");
    formatText(s, sh.t->webCredits,
               "<a href=\"https://api.met.no/\">MET Norway</a> (<a href=\"https://"
               "creativecommons.org/licenses/by/4.0/\">CC BY 4.0</a>)",
               "<a href=\"https://www.geonames.org/\">GeoNames</a> (<a href=\"https://"
               "creativecommons.org/licenses/by/4.0/\">CC BY 4.0</a>)");
    s += F("</small>");
  }
  s += F("<small>");
  s += sh.t->webDisclaimer;
  s += F("</small></div></footer>");
  if (script) {
    s += F("<script>");
    s += script;
    if (more) s += more;
    s += F("</script>");
  }
  s += F("</body></html>");
}

void saveBarStart(String &s, const Strings &t, const char *place, bool on) {
  s += F("<div class=\"sv ");
  s += place;
  if (on) s += F(" on");
  s += F("\"><p class=u hidden>");
  s += t.webUnsaved;
  s += F("</p><button class=p form=sv>");
  s += t.webSave;
  s += F("</button><button type=button class=dc form=sv hidden>");
  s += t.webDiscard;
  s += F("</button>");
}

// k(a): every control of sv (checked, then value; a select's options), in
// the page's order, so a moved row counts too; k(1) reads the markup's
// defaults, so a browser that restored typed values before the script ran
// still shows the bar (c() runs once at load). R (data-d): the page holds
// posted values, dirty until a submit; Discard opens R, the stored page.
// Else Discard resets the form (the order page puts its rows back on
// reset) and gives the focus to the h1, since the bar goes. A submit of sv
// leaves without the browser's question.
const char UNSAVED_SCRIPT[] =
    "let F=sv,B=document.querySelectorAll('.sv'),U=B[0].querySelector('.u').textContent,"
    "R=F.dataset.d,k=a=>[...F.elements].map(e=>e.options?[...e.options].map(o=>a?o.defaultSelected:o.selected)"
    ":a?e.defaultChecked+(e.defaultValue??e.value):e.checked+e.value)+'',S=k(1),D=!!R,"
    "c=m=>{let d=!!R||k()!=S;if(d!=D){D=d;B.forEach(b=>b.classList.toggle('on',d));"
    "m=[m,d&&U].filter(x=>x).join('. ')}if(m!=null)ann.textContent=m};c();"
    "document.oninput=()=>c();F.onsubmit=()=>{D=!1};"
    "onbeforeunload=e=>{if(D)e.preventDefault(),e.returnValue=''};"
    "document.querySelectorAll('.dc').forEach(b=>b.onclick=()=>{if(R){D=!1;location=R;return}F.reset();c();"
    "let h=document.querySelector('h1');h.tabIndex=-1;h.focus()});";

bool machinePath(const char *uri) {
  static const char *const ROOTS[] = {"/api", "/agent", "/claude"};
  for (const char *p : ROOTS) {
    const size_t n = strlen(p);
    if (!strncmp(uri, p, n) && (uri[n] == '\0' || uri[n] == '/')) return true;
  }
  return false;
}

void notice(String &s, const char *html) {
  s += F("<div class=ok>");
  s += html;
  s += F("</div>");
}

void errorNotice(String &s, const char *html, const char *id) {
  s += F("<div class=er role=alert");
  if (id) {
    s += F(" id=\"");
    s += id;
    s += '"';
  }
  s += '>';
  s += html;
  s += F("</div>");
}

void poster(String &s, int slot, bool big) {
  char b[80];
  snprintf(b, sizeof(b), "<i class=%s style=\"--p:%d\" aria-hidden=true></i>",
           big ? "\"th b\"" : "th", slot);
  s += b;
}
