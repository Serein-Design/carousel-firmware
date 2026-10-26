// SPDX-License-Identifier: AGPL-3.0-only
#include "web_images.h"

#include <stdio.h>

#include "posters.h"

static const char *const REINSTALL_URL =
    "https://github.com/Serein-Design/carousel-firmware#build-and-flash";

// go: the new pages' link style; lk: the old settings page's 44 px target,
// until its Images section moves to the new look
void reinstallLink(String &s, const Strings &t) {
  s += "<a class=\"go lk\" href=\"";
  s += REINSTALL_URL;
  s += "\">";
  s += t.webImgNaLink;
  s += "</a>";
}

void resetFailedHeading(String &s) {
  s += STRINGS[LANG_EN].resetFailedTitle;
  s += " / ";
  s += STRINGS[LANG_FR].resetFailedTitle;
}

static void kb(String &s, const Strings &t, uint32_t kilobytes) {
  s += String((unsigned long)kilobytes);
  s += "\u00A0";
  s += t.webKb;
}

// One of the upload messages, the sent name escaped. The longest (60 bytes
// sent, each escaped to 6, in the longest French message) is under 600
// bytes: test_section_fits_reserve checks that none is cut.
static void noticeText(String &s, const Strings &t, const ImagesNotice &n) {
  char line[640];
  const String sent = escapeHtml(n.sent ? n.sent : "");
  switch (n.result) {
    case UP_ADDED: case UP_REPLACED:
      snprintf(line, sizeof(line), n.result == UP_ADDED ? t.webImgAdded : t.webImgReplaced,
               escapeHtml(n.name ? n.name : "").c_str());
      break;
    case UP_NOT_GIF: {
      const char *type = n.header.kind == KIND_PNG ? "PNG" : n.header.kind == KIND_JPEG ? "JPEG"
                       : n.header.kind == KIND_WEBP ? "WebP" : nullptr;
      if (type) snprintf(line, sizeof(line), t.webImgNotGif, sent.c_str(), type);
      else      snprintf(line, sizeof(line), t.webImgNotGifOther, sent.c_str());
      break;
    }
    case UP_WRONG_SIZE:
      snprintf(line, sizeof(line), t.webImgWrongSize, sent.c_str(), (unsigned)n.header.width,
               (unsigned)n.header.height);
      break;
    case UP_NO_ROOM:   // What it needs rounded up, what is left rounded down
      snprintf(line, sizeof(line), t.webImgNoRoom, sent.c_str(), (unsigned long)kbUp(n.needed),
               (unsigned long)kbDown(n.left));
      break;
    case UP_TOO_MANY: snprintf(line, sizeof(line), t.webImgTooMany, sent.c_str()); break;
    case UP_TOO_BIG:  snprintf(line, sizeof(line), "%s", t.webImgTooBig); break;
    case UP_NO_FILE:  snprintf(line, sizeof(line), "%s", t.webImgNoFile); break;
    default:          snprintf(line, sizeof(line), "%s", t.webImgFailed); break;
  }
  s += line;
}

static bool stored(UploadResult r) { return r == UP_ADDED || r == UP_REPLACED; }

// id: a constant, so the file field can name a refusal (aria-describedby)
static void noticeBlock(String &s, const Strings &t, const ImagesNotice &n,
                        const char *id = nullptr) {
  if (n.result == UP_PENDING) return;
  if (stored(n.result)) {
    s += "<div class=ok role=status>";
  } else {
    s += "<div class=er role=alert";
    if (id) {
      s += " id=";
      s += id;
    }
    s += "><b>";
    s += t.webImgNotAdded;
    s += "</b> ";
  }
  noticeText(s, t, n);
  if (n.imageOff && stored(n.result)) {
    s += ' ';
    s += t.webImgOff;
  }
  s += "</div>";
}

// --- The Images page -----------------------------------------------------------

// Used rounded up, total rounded down; the bar as a whole percentage
static void spaceRow(String &s, const Strings &t, const Space &sp) {
  char used[96];
  snprintf(used, sizeof(used), t.webImgUsedOf, (unsigned long)kbUp(sp.used),
           (unsigned long)kbDown(sp.total));
  s += "<div class=sec style=\"margin-top:24px\"><p>";
  formatText(s, t.webTitleValue, t.webImgSpace, (String("<b class=num>") + used + "</b>").c_str());
  s += "</p><div class=space aria-hidden=true><i style=\"width:";
  const uint64_t pct = sp.total ? ((uint64_t)sp.used * 100 + sp.total / 2) / sp.total : 0;
  s += String((unsigned long)(pct > 100 ? 100 : pct));
  s += "%\"></i></div></div>";
}

// The upload's notice sits in Add a GIF, above the field it is about
static void addGif(String &s, const Strings &t, const ImagesNotice *n) {
  const bool refused = n && n->result != UP_PENDING && !stored(n->result);
  s += "<section class=sec aria-labelledby=h-a><h2 id=h-a>";
  s += t.webImgAdd;
  s += "</h2>";
  if (n) noticeBlock(s, t, *n, "e1");
  s += "<form method=post action=\"/images\" enctype=multipart/form-data><label for=f>";
  s += t.imgFileLabel;
  s += "</label><input id=f type=file name=f accept=image/gif";
  if (refused) s += " aria-invalid=true aria-describedby=e1 autofocus";
  s += "><small>";
  s += t.webImgAddHint;
  s += "</small><button class=p style=\"margin-top:16px\">";
  s += t.webImgUpload;
  s += "</button></form></section>";
}

// Every name in the list's order: the stored names are what the field posts.
// A saved choice the list lacks (an index read in part) is still an option,
// selected, so pressing Show keeps it instead of choosing all in turn.
static void panelShows(String &s, const Strings &t, const ImagesPageView &v, const char *choice) {
  s += "<section class=sec aria-labelledby=h-s><h2 id=h-s>";
  s += t.webImgShows;
  s += "</h2><form method=post action=\"/images/show\" class=row><div><label for=ch>";
  s += t.imgSelectLabel;
  s += "</label><select id=ch name=sh>";
  s += choice[0] ? "<option value=\"\">" : "<option value=\"\" selected>";
  s += t.webImgAll;
  s += "</option>";
  bool listed = !choice[0];
  for (int i = 0; i <= v.count; i++) {   // the last turn: the choice, if unlisted
    const char *name = i < v.count ? v.list[i].name : listed ? nullptr : choice;
    if (!name) break;
    const bool chosen = strcmp(name, choice) == 0;
    listed |= chosen;
    s += chosen ? "<option selected>" : "<option>";
    s += escapeHtml(name);
    s += "</option>";
  }
  s += "</select></div><button>";
  s += t.webImgShow;
  s += "</button></form><small>";
  s += t.webImgShowHint;
  s += "</small></section>";
}

// One image: its own file as the thumbnail, loaded only as it scrolls in,
// at an address that moves with the generation (a replace is a new file)
static void imageRow(String &s, const Strings &t, const ImageInfo &img, bool chosen,
                     uint32_t generation) {
  const String name = escapeHtml(img.name);
  s += img.portrait ? "<li><img class=\"th pt\" src=\"/images/f?n="
                    : "<li><img class=th src=\"/images/f?n=";
  s += name;
  s += "&amp;g=";
  s += String((unsigned long)generation);
  s += img.portrait ? "\" loading=lazy decoding=async width=32 height=64 alt=\"\"><div><b>"
                    : "\" loading=lazy decoding=async width=64 height=32 alt=\"\"><div><b>";
  s += name;
  s += "</b><small class=num style=\"margin-top:2px\">";
  s += img.portrait ? "32\u00D764 \u00B7 " : "64\u00D732 \u00B7 ";
  s += img.portrait ? t.webImgPortrait : t.webImgLandscape;
  s += " \u00B7 ";
  kb(s, t, kbUp(img.size));
  s += "</small>";
  if (chosen) {
    s += "<span class=tag>";
    s += t.imgShowsThis;
    s += "</span>";
  }
  s += "</div><form method=post action=\"/images/delete\"><input type=hidden name=n value=\"";
  s += name;
  s += "\"><button class=del aria-label=\"";
  formatText(s, t.imgDeleteName, name.c_str());
  s += "\">";
  s += t.webImgDelete;
  s += "</button></form></li>";
}

// No image: IMAGE's own no-image frame, so the owner sees what the panel shows
static void emptyList(String &s, const Strings &t) {
  s += "<div class=empty>";
  poster(s, POSTER_NO_IMAGE, false);
  s += "<p><b>";
  s += t.webImgNone;
  s += "</b></p><small style=\"margin:8px auto 0\">";
  s += t.imgEmptyHint;
  s += "</small></div>";
}

static void titleAndLead(String &s, const Strings &t) {
  s += "<h1>";
  s += t.webTabs[TAB_IMAGES];
  s += "</h1><p class=lead>";
  s += t.imgLead;
  s += "</p>";
}

// As the validated frames: the side column (title, lead, Space, Add a GIF,
// Panel shows), then the list beside it; rows flushed a few at a time
void imagesPage(String &s, const Shell &shell, const ImagesPageView &v, void (*flush)(String &)) {
  const Strings &t = *shell.t;
  Shell sh = shell;
  sh.title = t.webTabs[TAB_IMAGES];
  sh.tab = TAB_IMAGES;
  sh.tabs = true;
  shellStart(s, sh);
  if (!v.storage) {
    titleAndLead(s, t);
    s += "<div class=sec style=\"margin-top:24px\"><p>";
    formatText(s, t.webTitleValue, t.webImgSpace, (String("<b>") + t.webImgNa + "</b>").c_str());
    s += "</p><p>";
    s += t.webImgNaText;
    s += "</p>";
    reinstallLink(s, t);
    s += "</div>";
    flush(s);
    shellEnd(s, sh);
    return;
  }
  const char *choice = v.choice ? v.choice : "";
  s += "<div class=two><aside>";
  titleAndLead(s, t);
  spaceRow(s, t, v.space);
  addGif(s, t, v.notice);
  if (v.count > 0) panelShows(s, t, v, choice);
  s += "</aside>";
  flush(s);
  char heading[48];
  snprintf(heading, sizeof(heading), t.webImgOnPanel, v.count);
  s += "<section class=sec aria-labelledby=h-p><h2 id=h-p>";
  s += heading;
  s += "</h2>";
  if (v.count == 0) {
    emptyList(s, t);
  } else {
    s += "<ul class=imgs>";
    for (int i = 0; i < v.count; i++) {
      imageRow(s, t, v.list[i], strcmp(v.list[i].name, choice) == 0, v.generation);
      if (i % 8 == 7) flush(s);
    }
    s += "</ul>";
  }
  s += "</section></div>";
  flush(s);
  shellEnd(s, sh);
}

// A JSON string: quotes, backslashes and control characters escaped
static void jsonString(String &s, const char *v) {
  s += '"';
  for (; *v; v++) {
    const unsigned char c = (unsigned char)*v;
    if (c == '"' || c == '\\') {
      s += '\\';
      s += (char)c;
    } else if (c < 0x20) {
      char u[7];
      snprintf(u, sizeof(u), "\\u%04x", c);
      s += u;
    } else {
      s += (char)c;
    }
  }
  s += '"';
}

static void jsonNumber(String &s, const char *key, uint32_t v) {
  s += ",\"";
  s += key;
  s += "\":";
  s += String((unsigned long)v);
}

void imagesJson(String &s, bool storage, const ImageInfo *list, int count, const Space &sp,
                const char *choice) {
  if (!storage) {
    s += "{\"error\":\"no storage area\"}";
    return;
  }
  s += "{\"images\":[";
  for (int i = 0; i < count; i++) {
    s += i ? ",{\"name\":" : "{\"name\":";
    jsonString(s, list[i].name);
    s += list[i].portrait ? ",\"orientation\":\"portrait\"" : ",\"orientation\":\"landscape\"";
    jsonNumber(s, "size", list[i].size);
    s += '}';
  }
  s += ']';
  jsonNumber(s, "count", (uint32_t)count);
  jsonNumber(s, "max", MAX_IMAGES);
  jsonNumber(s, "used", sp.used);
  jsonNumber(s, "left", sp.left);
  jsonNumber(s, "total", sp.total);
  s += ",\"choice\":";
  jsonString(s, choice && choice[0] ? choice : "all");
  s += '}';
}

GateVerdict uploadGate(const GateInput &in) {
  if (!in.hostOk || in.crossSite) return GATE_CLOSE_403;
  if (in.api ? !(in.bearerOk || in.cookieOk) : !in.cookieOk)
    return in.api ? GATE_CLOSE_401 : GATE_SIGN_IN;
  if (in.announced > MAX_IMAGE_BYTES + MULTIPART_SLACK)
    return !in.api && in.announced <= PAGE_DRAIN_MAX ? GATE_DRAIN_413 : GATE_CLOSE_413;
  return GATE_ACCEPT;
}
