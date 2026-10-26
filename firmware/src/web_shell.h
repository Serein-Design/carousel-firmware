// SPDX-License-Identifier: AGPL-3.0-only
// What every page shares: the head with the stylesheet's versioned address,
// the brand row, the tabs, the skip link, the footer, the notices and the
// screen posters. Built without the web server, so test/test_web_shell
// checks it and test/test_web_pages writes the real pages.
#pragma once

#include <Arduino.h>

#include "i18n.h"

// A neighbouring network's name, or anything typed into a field, can contain
// anything: everything that did not come from the firmware is escaped.
String escapeHtml(const String &s);
// fmt, a page string, with its first three "%s" replaced by a, b and c, a
// later one kept as is: no buffer, however long the (escaped) values
void formatText(String &s, const char *fmt, const char *a, const char *b = "", const char *c = "");

enum Tab : int8_t { TAB_NONE = -1, TAB_STAGE, TAB_ORDER, TAB_IMAGES, TAB_PANEL };
extern const char *const TAB_HREF[];   // each tab's address, in Tab order

struct Shell {
  const Strings *t;
  const char *title;     // the page's name, plain text; <title> is webTitle with it
  const char *host;      // the panel's address, panelHostName() (escaped here); nullptr: webSetupBrand
  Tab tab;               // marked aria-current=page; TAB_NONE: none
  bool tabs;             // false: no nav (sign-in, setup, locked)
  const char *version;   // FIRMWARE_VERSION: asset paths and footer
};

// From the doctype to <main id=m><div class=w>, where the page's own
// content, its one <h1> included, goes
void shellStart(String &s, const Shell &sh);
// From the end of main to </html>: the footer, then script and more (static,
// in one <script>) when there are
void shellEnd(String &s, const Shell &sh, const char *script = nullptr, const char *more = nullptr);
// A save bar's opening: <div class="sv place">, "Unsaved changes", Save and
// Discard. The words and Discard carry hidden, so without a script Save shows
// alone, as always; on: already up (after a refused or failed save). The
// caller adds its <small> and </div>.
void saveBarStart(String &s, const Strings &t, const char *place, bool on);
// Hides the save bars (.sv) until a field of form sv differs from the page
// as loaded; c(m) rechecks and says m (and the change) on #ann. The page
// holds #ann; its own script comes after this one. A form sv with data-d
// (the stored page's address) holds posted values: dirty from the start,
// and Discard opens that address instead of resetting.
extern const char UNSAVED_SCRIPT[];
// /api, /agent, /claude and anything under them: a missing one gets a machine's 404,
// not the page (a script or a hook reads it)
bool machinePath(const char *uri);
// html is markup the firmware wrote; any user value in it is already escaped
void notice(String &s, const char *html);                                 // .ok
// id: a constant of the page's own, for aria-describedby
void errorNotice(String &s, const char *html, const char *id = nullptr);  // .er role=alert
// A screen's thumbnail: its cell of the poster sheet, as a CSS sprite. big:
// twice the size (128 x 64). The name stays text next to it.
void poster(String &s, int slot, bool big);
// "/s/<version>/<file>": a new firmware is a new address, so the browser can
// keep the file for a year. Returns the length the whole path needs, as
// snprintf does: at least size means it was cut.
size_t assetPath(char *out, size_t size, const char *version, const char *file);
