// SPDX-License-Identifier: AGPL-3.0-only
#include "web_screens.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "posters.h"     // posterFor
#include "region_logic.h"   // currencyCode
#include "web_stage.h"   // stagePlace

// --- Each screen's own settings ---------------------------------------------------
// Today's fields, each on its screen's page with the names /sauver reads.
// None carries a constraint the browser enforces (required, min, max, a
// step, a type): a field the browser holds invalid stops the form from
// posting, its message shown away from the Save pressed (the side column),
// or, once folded as these fields were, not at all.
// type=number alone refuses a half-typed "1e", "--" or "9." (badInput, which
// no attribute lifts), so the numbers are text fields that ask for a digit
// keyboard (inputmode). The save reads them: held to their bounds
// (formNumber), the salary with its decimals (formDecimal); text that is not
// a number keeps the stored value; an emptied salary is saved as 0, as the
// server's parse already did.

// How many bytes of p are a space between groups of digits: ASCII, or in
// UTF-8 the no-break space (U+00A0) and the narrow one (U+202F) that French
// number formatting writes ("2 850,50"); 0: none
static size_t groupSpace(const char *p) {
  if (*p == ' ') return 1;
  if (!strncmp(p, "\xC2\xA0", 2)) return 2;
  if (!strncmp(p, "\xE2\x80\xAF", 3)) return 3;
  return 0;
}

// What a person types, made plain for strtol and strtof ("-2850.50"), or
// false when it is not a number. Spaces are dropped. A comma or a point
// followed by exactly three digits groups them ("1,240" is 1240, never 1;
// "2.850,50"); the last one, followed by any other count, is the decimal
// point ("2850,50", "9."); any other makes it not a number ("1,2,3"), as
// letters do ("1e", "1.2k"), so no hex, inf or nan. The save then keeps the
// stored value: a part of the text would be a value nobody typed. At most 31
// characters kept, more than any of these values needs (a float stays
// finite); a longer number is not one.
static bool plainNumber(const char *text, char (&out)[32]) {
  size_t n = 0;
  size_t digits = 0;   // since the last separator
  int last = -1;       // where the last separator is in out; -1: none
  for (const char *p = text; *p;) {
    if (const size_t skip = groupSpace(p)) {
      p += skip;
      continue;
    }
    const char c = *p++;
    const bool digit = c >= '0' && c <= '9';
    const bool sign = (c == '+' || c == '-') && n == 0;
    // after a digit, and the one before it grouped three
    const bool separator = (c == ',' || c == '.') && digits && (last < 0 || digits == 3);
    if ((!digit && !sign && !separator) || n == sizeof(out) - 1) return false;
    if (separator) {
      last = (int)n;
      digits = 0;
    }
    digits += digit;
    out[n++] = c;
  }
  if (n && !digits && last < 0) return false;   // a sign alone
  if (digits == 3) last = -1;                    // the last one grouped digits too
  size_t kept = 0;
  for (size_t i = 0; i < n; i++) {
    if ((int)i == last) out[kept++] = '.';
    else if (out[i] != ',' && out[i] != '.') out[kept++] = out[i];
  }
  out[kept] = '\0';
  return true;
}

long formNumber(const char *text, long lo, long hi, long stored) {
  char plain[32];
  // strtol saturates, where atol overflows; the decimals are dropped
  const long v = plainNumber(text, plain) ? strtol(plain, nullptr, 10) : stored;
  return v < lo ? lo : v > hi ? hi : v;
}

long formId(const char *text, long hi) {
  long v = 0;
  if (!*text) return -1;
  for (const char *c = text; *c; c++) {
    if (*c < '0' || *c > '9') return -1;
    v = v * 10 + (*c - '0');
    if (v > hi) return -1;
  }
  return v;
}

float formDecimal(const char *text, float stored) {
  char plain[32];
  return plainNumber(text, plain) ? strtof(plain, nullptr) : stored;
}

bool formEdited(const char *posted, const char *original) {
  return !original || strcmp(posted, original) != 0;
}

// One digit below count, as the page writes an option; -1: anything else
static int8_t optionPosted(const char *v, uint8_t count) {
  return v[0] >= '0' && v[0] < '0' + count && !v[1] ? (int8_t)(v[0] - '0') : -1;
}

void StylesForm::arg(const char *name, const char *value) {
  int8_t *style = !strcmp(name, "cs") ? &code_ : !strcmp(name, "ks") ? &clock_
                : !strcmp(name, "ws") ? &weather_ : nullptr;
  if (style) {
    const uint8_t count = style == &code_ ? (uint8_t)CODE_STYLE_COUNT
                        : style == &clock_ ? (uint8_t)CLOCK_STYLE_COUNT
                                           : (uint8_t)WEATHER_STYLE_COUNT;
    if (*style < 0) *style = optionPosted(value, count);
  } else if (!strcmp(name, "kwh")) {
    showsPosted_ = true;
  } else if (!strcmp(name, "kw")) {
    showsPosted_ = true;
    const int8_t bit = optionPosted(value, 4);   // CLOCK_SHOW_ALL's four bits
    if (bit >= 0) shows_ |= (uint8_t)(1u << bit);
  }
}

void StylesForm::apply(Settings &s) const {
  if (code_ >= 0) s.codeStyle = (uint8_t)code_;
  if (clock_ >= 0) s.clockStyle = (uint8_t)clock_;
  if (weather_ >= 0) s.weatherStyle = (uint8_t)weather_;
  if (showsPosted_) s.clockShow = shows_;
}

// A count as the page's language writes it: 12,500 in English, 12 500
// (a narrow no-break space) in French
static String grouped(long n, const char *sep) {
  char digits[24];
  snprintf(digits, sizeof(digits), "%ld", n);
  String out;
  const char *d = digits;
  if (*d == '-') out += *d++;
  for (size_t left = strlen(d); *d; d++) {
    out += *d;
    if (--left && left % 3 == 0) out += sep;
  }
  return out;
}

// --- Each row's summary: the current choice in one line ---------------------------
// Escaped; nothing appended when there is nothing to say (an empty message).
// The parts are joined by a middle dot, the items by commas.

static const char SUMMARY_PART[] = " · ";
// A message's start: past this many bytes, cut and an ellipsis; the CSS
// cuts what still does not fit the row
static const size_t SUMMARY_TEXT_MAX = 32;

static void textSummary(String &s, const Strings &, const ScreensView &v) {
  const char *m = v.values->message;
  size_t n = strlen(m);
  const bool cut = n > SUMMARY_TEXT_MAX;
  if (cut) {
    n = SUMMARY_TEXT_MAX;
    while (n && ((uint8_t)m[n] & 0xC0) == 0x80) n--;   // never inside a UTF-8 character
  }
  char part[SUMMARY_TEXT_MAX + 1];
  memcpy(part, m, n);
  part[n] = '\0';
  s += escapeHtml(part);
  if (cut) s += "…";
}
static void weatherSummary(String &s, const Strings &t, const ScreensView &v) {
  if (v.values->city[0]) {
    s += escapeHtml(v.values->city);
    s += SUMMARY_PART;
  }
  s += t.weatherStyles[v.values->weatherStyle % WEATHER_STYLE_COUNT];
}
static void codeSummary(String &s, const Strings &t, const ScreensView &v) {
  s += t.codeStyles[v.values->codeStyle % CODE_STYLE_COUNT];
}
// "Stone · date, seconds": the style, then what it shows besides the time
static void clockSummary(String &s, const Strings &t, const ScreensView &v) {
  s += t.clockStyles[v.values->clockStyle % CLOCK_STYLE_COUNT];
  const char *joint = SUMMARY_PART;
  for (uint8_t i = 0; i < 4; i++) {   // CLOCK_SHOW_ALL's four bits
    if (!((v.values->clockShow >> i) & 1)) continue;
    s += joint;
    s += t.clockShowsShort[i];
    joint = ", ";
  }
}
static void moneySummary(String &s, const Strings &t, const ScreensView &v) {
  const Settings &st = *v.values;
  char line[96];
  snprintf(line, sizeof(line), t.ordMoneySum, grouped(lroundf(st.salary), t.webThousands).c_str(),
           currencyCode(st.currency), (unsigned)st.workStart, (unsigned)st.workEnd);
  s += line;
}
// The accounts it shows: the services with a channel, a name or a count
static void socialSummary(String &s, const Strings &, const ScreensView &v) {
  const Settings &st = *v.values;
  const struct { bool set; const char *name; } services[] = {
    {st.ytChannel[0] != '\0', "YouTube"}, {st.ghUser[0] != '\0', "GitHub"},
    {st.instaFollowers > 0, "Instagram"}, {st.xFollowers > 0, "X"}};
  const char *joint = "";
  for (const auto &x : services) {
    if (!x.set) continue;
    s += joint;
    s += x.name;
    joint = ", ";
  }
}
// The image the panel shows, or every stored one in turn
static void imageSummary(String &s, const Strings &t, const ScreensView &v) {
  if (!v.images.storage) {
    s += t.webImgNa;
  } else if (v.images.choice && v.images.choice[0]) {
    s += escapeHtml(v.images.choice);
  } else {
    char line[64];
    snprintf(line, sizeof(line), t.ordImagesAll, v.images.count);
    s += line;
  }
}

// Named functions, not lambdas: the tables below stay constants in flash
// (a lambda's conversion is not a constant expression before C++17, and
// the tables then moved to RAM, filled at start)
static String message(const Settings &s) { return String(s.message); }
static String city(const Settings &s) { return String(s.city); }
static String salary(const Settings &s) { return String(s.salary, 2); }
static String workStart(const Settings &s) { return String((unsigned long)s.workStart); }
static String workEnd(const Settings &s) { return String((unsigned long)s.workEnd); }
static String workDays(const Settings &s) { return String((unsigned long)s.workDays); }
static String ytKeyStored(const Settings &s) { return String(s.ytKey[0] ? "1" : ""); }
static String ytChannel(const Settings &s) { return String(s.ytChannel); }
static String ghUser(const Settings &s) { return String(s.ghUser); }
static String instagram(const Settings &s) { return String((unsigned long)s.instaFollowers); }
static String xFollowers(const Settings &s) { return String((unsigned long)s.xFollowers); }
static String codeStyle(const Settings &s) { return String((unsigned long)s.codeStyle); }
static String clockStyle(const Settings &s) { return String((unsigned long)s.clockStyle); }
static String clockShow(const Settings &s) { return String((unsigned long)s.clockShow); }
static String weatherStyle(const Settings &s) { return String((unsigned long)s.weatherStyle); }

static const ScreenField CODE_FIELDS[] = {
  {FIELD_STYLE, &Strings::webStyle, "cs", nullptr, codeStyle},
};
static const ScreenField CLOCK_FIELDS[] = {
  {FIELD_STYLE, &Strings::webStyle, "ks", nullptr, clockStyle},
  {FIELD_SHOWS, &Strings::webClockShows, "kw", nullptr, clockShow},
};
static const ScreenField TEXT_FIELDS[] = {
  {FIELD_TEXT, &Strings::webMessage, "msg", "maxlength=95",
   message},
  {FIELD_ORIGINAL, nullptr, "msg0", nullptr, message},
};
static const ScreenField WEATHER_FIELDS[] = {
  {FIELD_STYLE, &Strings::webStyle, "ws", nullptr, weatherStyle},
  {FIELD_TEXT, &Strings::webCity, "city", "maxlength=47 placeholder=\"Lyon\"",
   city},
  {FIELD_SUGGEST, nullptr, nullptr, nullptr, nullptr},
  {FIELD_FOUND, nullptr, nullptr, nullptr, nullptr},
  {FIELD_HINT, &Strings::webCityHint, nullptr, nullptr, nullptr},
};
static const ScreenField MONEY_FIELDS[] = {
  {FIELD_NUMBER, &Strings::webSalary, "sal", "inputmode=decimal",
   salary},
  {FIELD_ROW, nullptr, nullptr, nullptr, nullptr},
  {FIELD_NUMBER, &Strings::webDayStart, "hd", "inputmode=numeric",
   workStart},
  {FIELD_CELL, nullptr, nullptr, nullptr, nullptr},
  {FIELD_NUMBER, &Strings::webDayEnd, "hf", "inputmode=numeric",
   workEnd},
  {FIELD_CELL, nullptr, nullptr, nullptr, nullptr},
  {FIELD_NUMBER, &Strings::webDaysPerMonth, "jm", "inputmode=numeric",
   workDays},
  {FIELD_ROW_END, nullptr, nullptr, nullptr, nullptr},
  {FIELD_HINT, &Strings::webIncomeHint, nullptr, nullptr, nullptr},
};
static const ScreenField SOCIAL_FIELDS[] = {
  {FIELD_COUNTS, nullptr, nullptr, nullptr, nullptr},
  {FIELD_HEADING, nullptr, "YouTube", nullptr, nullptr},
  {FIELD_SECRET, &Strings::webYtKey, "ytk", "placeholder=\"AIza...\" autocomplete=off",
   ytKeyStored},
  {FIELD_TEXT, &Strings::webYtChannel, "ytc", "placeholder=\"UCxxxxxxxxxxxxxxxxxxxxxx\"",
   ytChannel},
  {FIELD_HINT, &Strings::webYtChannelHint, nullptr, nullptr, nullptr},
  {FIELD_HEADING, nullptr, "GitHub", nullptr, nullptr},
  {FIELD_TEXT, &Strings::webGhUser, "gh", "", ghUser},
  {FIELD_HINT, &Strings::webGhHint, nullptr, nullptr, nullptr},
  {FIELD_HEADING, &Strings::webManual, nullptr, nullptr, nullptr},
  {FIELD_ROW, nullptr, nullptr, nullptr, nullptr},
  {FIELD_NUMBER, &Strings::webInstagram, "ig", "inputmode=numeric",
   instagram},
  {FIELD_CELL, nullptr, nullptr, nullptr, nullptr},
  {FIELD_NUMBER, &Strings::webX, "xa", "inputmode=numeric",
   xFollowers},
  {FIELD_ROW_END, nullptr, nullptr, nullptr, nullptr},
  {FIELD_HINT, &Strings::webManualHint, nullptr, nullptr, nullptr},
};
static const ScreenField IMAGE_FIELDS[] = {
  {FIELD_IMAGES, nullptr, nullptr, nullptr, nullptr},
};

#define FIELDS(a) a, (uint8_t)(sizeof(a) / sizeof(a[0]))
static const ScreenSettings CODE_BLOCK = {codeSummary, FIELDS(CODE_FIELDS), nullptr};
static const ScreenSettings CLOCK_BLOCK = {clockSummary, FIELDS(CLOCK_FIELDS), nullptr};
static const ScreenSettings TEXT_BLOCK = {textSummary, FIELDS(TEXT_FIELDS), nullptr};
static const ScreenSettings WEATHER_BLOCK = {weatherSummary, FIELDS(WEATHER_FIELDS), CITY_SCRIPT};
static const ScreenSettings MONEY_BLOCK = {moneySummary, FIELDS(MONEY_FIELDS), nullptr};
static const ScreenSettings SOCIAL_BLOCK = {socialSummary, FIELDS(SOCIAL_FIELDS), nullptr};
static const ScreenSettings IMAGE_BLOCK = {imageSummary, FIELDS(IMAGE_FIELDS), nullptr};
#undef FIELDS

// screen_table.h's settings column
#define NO_SETTINGS      nullptr
#define CODE_SETTINGS    (&CODE_BLOCK)
#define CLOCK_SETTINGS   (&CLOCK_BLOCK)
#define TEXT_SETTINGS    (&TEXT_BLOCK)
#define WEATHER_SETTINGS (&WEATHER_BLOCK)
#define MONEY_SETTINGS   (&MONEY_BLOCK)
#define SOCIAL_SETTINGS  (&SOCIAL_BLOCK)
#define IMAGE_SETTINGS   (&IMAGE_BLOCK)
#define SETTINGS_COLUMN(name, draw, orientation, leave, settings) settings,
static const ScreenSettings *const SETTINGS_OF[SCREEN_COUNT] = {SCREEN_TABLE(SETTINGS_COLUMN)};
#undef SETTINGS_COLUMN

const ScreenSettings *screenSettings(uint8_t screen) {
  return screen < SCREEN_COUNT ? SETTINGS_OF[screen] : nullptr;
}

// A saved key is never sent back: the field only says there is one
static const char SECRET_STORED[] =
    "placeholder=\"&bull;&bull;&bull;&bull;&bull;&bull;\" autocomplete=off";

const char *mqttPassAttrs(const Settings &stored) {
  return stored.mqttPass[0]
      ? "type=password maxlength=63 autocomplete=off placeholder=\"&bull;&bull;&bull;&bull;&bull;&bull;\""
      : "type=password maxlength=63 autocomplete=off";
}

// Label and input, tied by the id: the label is the field's name for a
// screen reader. The value is escaped; attrs is this file's own markup.
static void input(String &s, const char *label, const char *name, const String &value,
                  const char *attrs) {
  s += "<label for=";
  s += name;
  s += '>';
  s += label;
  s += "</label><input id=";
  s += name;
  s += " name=";
  s += name;
  s += " form=sv value=\"";
  s += escapeHtml(value);
  s += '"';
  if (attrs[0]) {
    s += ' ';
    s += attrs;
  }
  s += '>';
}

static void found(String &s, const Strings &t, const ScreensView &v) {
  const Settings &st = *v.values;
  const char *place = nullptr;
  String escaped;
  bool searching = false;
  if (st.place[0]) {
    escaped = escapeHtml(st.place);
    place = escaped.c_str();
  } else if (st.city[0]) {
    searching = !v.locateFailed;
    place = searching ? t.webCitySearching : t.webCityNotFound;
  }
  if (!place) return;
  s += "<div class=st><span>";
  s += t.webCityFound;
  s += "</span><b";
  // The HTTPS task finds the place seconds after the save: CITY_SCRIPT
  // polls /place for this row, and says data-f if the city is not found
  if (searching) {
    s += " id=fd data-f=\"";
    s += escapeHtml(t.webCityNotFound);
    s += '"';
  }
  s += '>';
  s += place;
  s += "</b></div>";
}

// The city field's suggestions (CITY_SCRIPT fills them): the chosen place's
// id, off so a stale one is never posted, the list, hidden until there is
// something to list, and the live line, whose two sentences the script
// reads ("*" stands for the count)
static void suggest(String &s, const Strings &t) {
  s += "<input type=hidden id=cid name=cid form=sv disabled><div id=pl role=listbox aria-label=\"";
  s += escapeHtml(t.webCity);
  s += "\" hidden></div><p id=pn class=vh aria-live=polite data-m=\"";
  String matches;
  formatText(matches, t.webCityMatches, "*");
  s += escapeHtml(matches);
  s += "\" data-z=\"";
  s += escapeHtml(t.webCityNone);
  s += "\"></p>";
}

// IMAGE's block: which image the panel shows, and the way to Images, where
// its forms are
static void imagesField(String &s, const Strings &t, const ImagesView &im) {
  s += "<small style=\"margin-top:0\">";
  if (!im.storage) s += t.webImgNaText;
  else if (im.choice && im.choice[0]) formatText(s, t.ordImageShows, escapeHtml(im.choice).c_str());
  else s += t.ordImageAll;
  s += "</small><a class=go href=\"/images\">";
  s += t.ordOpenImages;
  s += "</a>";
}

static void countRow(String &s, const char *label, const String &value) {
  s += "<div class=st><span>";
  s += label;
  s += "</span><b>";
  s += value;
  s += "</b></div>";
}

// SOCIAL's block: what the panel last fetched, "not yet" while unknown
static void counts(String &s, const Strings &t, const SocialCounts &c) {
  countRow(s, "YouTube", c.ytOk ? grouped(c.yt, t.webThousands) : String(t.webNotYet));
  countRow(s, "GitHub", c.ghOk ? grouped(c.gh, t.webThousands) : String(t.webNotYet));
  char ago[48];
  if (c.ytOk || c.ghOk) snprintf(ago, sizeof(ago), t.webMinutesAgo, (unsigned long)c.agoMin);
  countRow(s, t.webLastUpdate, c.ytOk || c.ghOk ? String(ago) : String(t.webNotYet));
}

// A style's options, by the field's name: names and descriptions in the
// enum's order, and the screen whose posters show them
struct StyleOptions { const char *const *names, *const *hints; uint8_t count; const char *screen; };
static StyleOptions styleOptions(const Strings &t, const char *name) {
  if (!strcmp(name, "cs")) return {t.codeStyles, t.codeStyleHints, CODE_STYLE_COUNT, "CODE"};
  if (!strcmp(name, "ks")) return {t.clockStyles, t.clockStyleHints, CLOCK_STYLE_COUNT, "CLOCK"};
  return {t.weatherStyles, t.weatherStyleHints, WEATHER_STYLE_COUNT, "WEATHER"};
}

// One row per option or item, the whole row the target (.op), in a
// fieldset its legend names; checked are the ones held. Works without the
// script; the unsaved bar sees every change (UNSAVED_SCRIPT).
static void optionRow(String &s, const char *type, const char *name, uint8_t value, bool checked,
                      const char *label, const char *hint) {
  s += "<label class=op><input type=";
  s += type;
  s += " name=";
  s += name;
  s += " value=";
  s += (char)('0' + value);
  s += checked ? " form=sv checked><span>" : " form=sv><span>";
  s += label;
  if (hint) {
    s += "<small>";
    s += hint;
    s += "</small>";
  }
  s += "</span></label>";
}

// One card per style: what the panel draws in it (its cell of the poster
// sheet, twice the size), its radio, its name and its one line; the whole
// card is the target
static void styleChoice(String &s, const Strings &t, const ScreenField &f, const Settings &v) {
  const StyleOptions o = styleOptions(t, f.name);
  const long chosen = f.value(v).toInt();
  const uint8_t screen = (uint8_t)screenFind(o.screen);
  s += "<fieldset><legend>";
  s += t.*f.text;
  s += "</legend><div class=cards>";
  for (uint8_t i = 0; i < o.count; i++) {
    s += "<label class=card>";
    poster(s, posterFor(screen, i), true);
    s += "<input type=radio name=";
    s += f.name;
    s += " value=";
    s += (char)('0' + i);
    s += i == chosen ? " form=sv checked><span>" : " form=sv><span>";
    s += o.names[i];
    s += "<small>";
    s += o.hints[i];
    s += "</small></span></label>";
  }
  s += "</div></fieldset>";
}

// The hidden kwh says the boxes were on the page: a save with all of them
// unticked posts no kw at all
static void showsBoxes(String &s, const Strings &t, const ScreenField &f, const Settings &v) {
  const long shown = f.value(v).toInt();
  s += "<fieldset><legend>";
  s += t.*f.text;
  s += "</legend><input type=hidden name=";
  s += f.name;
  s += "h value=1 form=sv>";
  for (uint8_t i = 0; i < 4; i++)
    optionRow(s, "checkbox", f.name, i, (shown >> i) & 1, t.clockShows[i], nullptr);
  s += "</fieldset>";
}

static void field(String &s, const Strings &t, const OrderView &o, const ScreenField &f) {
  const ScreensView &v = o.screens;
  const char *text = f.text ? t.*f.text : f.name;
  switch (f.kind) {
    case FIELD_TEXT: case FIELD_NUMBER:
      if (!strcmp(f.name, "sal")) {   // the salary is counted in the chosen currency
        String label;
        formatText(label, text, currencyCode(v.values->currency));
        input(s, label.c_str(), f.name, f.value(*v.values), f.attrs);
        break;
      }
      input(s, text, f.name, f.value(*v.values), f.attrs);
      break;
    case FIELD_SECRET:
      // From what is stored: a key typed in a refused save is not kept
      input(s, text, f.name, String(), f.value(*v.stored).length() ? SECRET_STORED : f.attrs);
      break;
    case FIELD_HEADING:
      s += "<h3 class=lh>";
      s += text;
      s += "</h3>";
      break;
    case FIELD_HINT:
      s += "<small>";
      s += text;
      s += "</small>";
      break;
    case FIELD_ROW:     s += "<div class=r><div>"; break;
    case FIELD_CELL:    s += "</div><div>"; break;
    case FIELD_ROW_END: s += "</div></div>"; break;
    case FIELD_FOUND:   found(s, t, v); break;
    case FIELD_SUGGEST: suggest(s, t); break;
    case FIELD_IMAGES:  imagesField(s, t, v.images); break;
    case FIELD_COUNTS:  counts(s, t, o.social); break;
    case FIELD_STYLE:   styleChoice(s, t, f, *v.values); break;
    case FIELD_SHOWS:   showsBoxes(s, t, f, *v.values); break;
    case FIELD_ORIGINAL:
      s += "<input type=hidden name=";
      s += f.name;
      s += " form=sv value=\"";
      s += escapeHtml(f.value(*v.stored));
      s += "\">";
      break;
  }
}

// --- The list ------------------------------------------------------------------------

// The arrows draw themselves (.mv::before); their names say what they do
static void arrow(String &s, const char *label, const char *en, const char *shown, bool up,
                  bool disabled) {
  s += up ? "<button class=mv form=sv name=mv value=\"" : "<button class=\"mv d\" form=sv name=mv value=\"";
  s += en;
  s += up ? ":u\" aria-label=\"" : ":d\" aria-label=\"";
  formatText(s, label, shown);
  s += disabled ? "\" disabled></button>" : "\"></button>";
}

// The way to a screen's page, a 44 px gear (.gt::before) named for its
// screen; a screen without settings holds the column empty (.gp), so the
// handles line up on every row
static void settingsLink(String &s, const Strings &t, uint8_t i, const char *shown) {
  char slug[SCREEN_SLUG_MAX];
  screenSlug(i, slug, sizeof(slug));
  s += "<a class=gt href=\"/screens/";
  s += slug;
  s += "\" aria-label=\"";
  formatText(s, t.ordSettingsOf, shown);
  s += "\"></a>";
}

// The drag handle, shown only with the script (the CSS), in the arrows'
// place: a button named for its screen and its place, which a drag or
// ArrowUp and ArrowDown move (SCREENS_SCRIPT, which names it again once its
// row moved). form=sv: a button belongs to a form; type=button posts
// nothing.
static void handle(String &s, const Strings &t, const char *shown, uint8_t place) {
  char at[4], count[4];
  snprintf(at, sizeof(at), "%u", (unsigned)place + 1);
  snprintf(count, sizeof(count), "%u", (unsigned)SCREEN_COUNT);
  s += "<button type=button class=dg form=sv aria-label=\"";
  formatText(s, t.ordHandle, shown, at, count);
  s += "\"></button>";
}

// The style a screen is drawn in now (its thumbnail follows it); 0xFF for a
// screen without styles, which posterFor answers with the screen's own cell
static uint8_t chosenStyle(const Settings &v, uint8_t screen) {
  const char *name = screenNameEn(screen);
  if (!strcmp(name, "CODE")) return v.codeStyle;
  if (!strcmp(name, "CLOCK")) return v.clockStyle;
  if (!strcmp(name, "WEATHER")) return v.weatherStyle;
  return 0xFF;
}

// One screen: "o" its English name (the form identifies screens by name),
// its box, its place (hidden from screen readers: the box is named by the
// screen and its mark), its poster, its summary, the arrows (submit buttons;
// without the script, the only way to move it), the way to its page (or an
// empty column of its width), then the drag handle. data-n is what the
// script announces. The screen on show (now: its place among those that
// play, else 0) is marked class=at, which draws a small dot on its poster
// (absolute: no row changes size), and says "on stage" to screen readers; the
// script moves both (SCREENS_SCRIPT).
static void screenRow(String &s, const Strings &t, const OrderView &o, uint8_t place,
                      const StagePlace &now) {
  const ScreenList &l = o.screens.values->screens;
  const uint8_t i = l.order[place];
  const char *en = screenNameEn(i);
  const char *shown = t.*SCREEN_INFO[i].name;
  const bool live = now.n && i == o.current;
  const ScreenSettings *b = screenSettings(i);
  s += "<li data-n=\"";
  s += shown;
  s += live ? "\" class=at><div class=sr><input type=hidden name=o value=\""
            : "\"><div class=sr><input type=hidden name=o value=\"";
  s += en;
  s += "\" form=sv><label class=ck for=s";
  s += String(i);
  s += "><input id=s";
  s += String(i);
  s += " type=checkbox name=on value=\"";
  s += en;
  s += screenOn(l, i) ? "\" form=sv checked><span class=n aria-hidden=true>"
                      : "\" form=sv><span class=n aria-hidden=true>";
  s += String((unsigned)place + 1);
  s += "</span>";
  poster(s, posterFor(i, chosenStyle(*o.screens.values, i)), false);
  s += "<span class=nm>";
  s += shown;
  if (live) {
    s += "<span class=vh> ";
    s += t.stgLive;
    s += "</span>";
  }
  if (b) {
    String sum;
    b->summary(sum, t, o.screens);
    if (sum.length()) {
      s += "<small class=sm>";
      s += sum;
      s += "</small>";
    }
  }
  const Orientation orientation = SCREEN_INFO[i].orientation;
  if (orientation != BOTH) {
    s += " <small>";
    s += orientation == LANDSCAPE ? t.webLandscapeOnly : t.webPortraitOnly;
    s += "</small>";
  }
  s += "</span></label>";
  arrow(s, t.webMoveUp, en, shown, true, place == 0);
  arrow(s, t.webMoveDown, en, shown, false, place == SCREEN_COUNT - 1);
  if (b) settingsLink(s, t, i, shown);
  else s += "<span class=gp></span>";
  handle(s, t, shown, place);
  s += "</div></li>";
}

static_assert(sizeof(Strings::webRotate) / sizeof(Strings::webRotate[0]) == ROTATE_CHOICE_COUNT,
              "one label per offered interval");

int screenPageFind(const char *slug) {
  const int i = screenFindSlug(slug);
  return i >= 0 && screenSettings((uint8_t)i) ? i : -1;
}

static const char SCREEN_PAGE_PG[] = "screens/";

SavePage saveAnswerPage(const char *pg, uint8_t *screen) {
  if (!pg) return SAVE_TO_PANEL;
  if (!strcmp(pg, "screens")) return SAVE_TO_ORDER;
  if (!strcmp(pg, "panel/feedback")) return SAVE_TO_FEEDBACK;
  const size_t n = sizeof(SCREEN_PAGE_PG) - 1;
  const int i = strncmp(pg, SCREEN_PAGE_PG, n) ? -1 : screenPageFind(pg + n);
  if (i < 0) return SAVE_TO_PANEL;
  *screen = (uint8_t)i;
  return SAVE_TO_SCREEN;
}

// A refused or failed save: the page holds the posted values, not the
// stored ones, so the bars start up and Discard opens the stored page
static bool orderPosted(const OrderView &o) {
  return o.screens.refusal != LIST_OK || o.result == SAVE_FAILED;
}

// Save and the count of screens on: in view on a phone (.mb, under the
// list), in the side column from 900 px (.dk); with the script, only once
// something differs (UNSAVED_SCRIPT) or after a refused or failed save
static void saveBar(String &s, const Strings &t, const char *place, const OrderView &o,
                    unsigned on) {
  saveBarStart(s, t, place, orderPosted(o));
  s += "<small class=num>";
  char line[48];
  snprintf(line, sizeof(line), t.ordCount, on, (unsigned)SCREEN_COUNT);
  s += line;
  s += "</small></div>";
}

// What the save did, under the form: a refused list says why, and the
// interval takes the focus so the browser brings the column into view
static void resultBanner(String &s, const Strings &t, const OrderView &o) {
  const ListCheck r = o.screens.refusal;
  if (r != LIST_OK)
    errorNotice(s, r == LIST_EMPTY ? t.webRefusedEmpty
                 : r == LIST_NO_PORTRAIT ? t.webRefusedPortrait : t.webRefusedLandscape);
  else if (o.result == SAVE_DONE) notice(s, t.webSaved);
  else if (o.result == SAVE_LATER) notice(s, t.webSavedLater);
  else if (o.result == SAVE_FAILED) errorNotice(s, t.webNotSaved);
}

// The side column holds the page's one form, with only its page's name and
// the hidden Save: the first submit button of form sv in the page's order,
// the one Enter presses (the first row's up arrow is disabled). Every other
// control joins it by form=sv.
static void sideColumn(String &s, const Strings &t, const OrderView &o, unsigned on) {
  const Settings &st = *o.screens.values;
  s += "<div class=two><aside><h1>";
  s += t.webTabs[TAB_ORDER];
  s += "</h1><p class=lead>";
  s += t.ordLead;
  s += orderPosted(o) ? "</p><form id=sv data-d=/screens method=post action=\"/sauver\">"
                      : "</p><form id=sv method=post action=\"/sauver\">";
  s += "<input type=hidden name=pg value=screens><button hidden>";
  s += t.webSave;
  s += "</button></form>";
  resultBanner(s, t, o);
  s += "<label for=rt>";
  s += t.webChangeScreen;
  s += o.screens.refusal != LIST_OK ? "</label><select id=rt name=rt form=sv autofocus>"
                                    : "</label><select id=rt name=rt form=sv>";
  for (uint8_t c = 0; c < ROTATE_CHOICE_COUNT; c++) {
    s += "<option value=";
    s += String((unsigned)ROTATE_CHOICES[c]);
    s += ROTATE_CHOICES[c] == st.rotateSec ? " selected>" : ">";
    s += t.webRotate[c];
    s += "</option>";
  }
  s += "</select><small>";
  s += t.webRotateHint;
  s += "</small>";
  saveBar(s, t, "dk", o, on);
  s += "</aside>";
}

void orderPage(String &s, const Shell &shell, const OrderView &o, void (*flush)(String &)) {
  const Strings &t = *shell.t;
  Shell sh = shell;
  sh.title = t.webTabs[TAB_ORDER];
  sh.tab = TAB_ORDER;
  sh.tabs = true;
  const ScreenList &l = o.screens.values->screens;
  unsigned on = 0;
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) on += screenOn(l, i);
  shellStart(s, sh);
  sideColumn(s, t, o, on);
  flush(s);
  char line[96];
  snprintf(line, sizeof(line), t.webInOrder, on, (unsigned)SCREEN_COUNT);
  s += "<section class=sec aria-labelledby=h-i><h2 id=h-i>";
  s += line;
  s += "</h2><small style=\"margin:-4px 0 12px\">";
  s += t.webOrderHint;
  // The handles' name, "*" for the screen, its place and the count, for
  // the script to name a moved row's handle again
  s += "</small><ol class=list data-h=\"";
  formatText(s, t.ordHandle, "*", "*", "*");
  s += "\" data-l=\"";
  s += t.stgLive;
  s += "\">";
  // On stage: the gallery plays the stored list, whatever a refused post holds
  const StagePlace now = stagePlace(o.screens.stored->screens, o.current, o.portrait);
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) {
    screenRow(s, t, o, p, now);
    if (s.length() > SCREENS_CHUNK) flush(s);
  }
  s += "</ol><p id=ann class=vh aria-live=polite></p>";
  saveBar(s, t, "mb", o, on);
  s += "</section></div>";
  flush(s);
  shellEnd(s, sh, UNSAVED_SCRIPT, SCREENS_SCRIPT);
}

// --- A screen's page -----------------------------------------------------------------

// Whether a block holds anything to post: IMAGE's only says what shows and
// links to Images, so its page has no form
static bool hasInputs(const ScreenSettings &b) {
  for (uint8_t k = 0; k < b.count; k++) {
    switch (b.fields[k].kind) {
      case FIELD_TEXT: case FIELD_NUMBER: case FIELD_SECRET: case FIELD_STYLE: case FIELD_SHOWS:
        return true;
      default:
        break;
    }
  }
  return false;
}

// The side column as Running order's: the way back, the screen's name, the
// one form (its page's return path, a slug from the table, never posted
// text; the hidden Save first, the one Enter presses), what the save did,
// then Save. The settings beside it, every control joining by form=sv.
void screenPage(String &s, const Shell &shell, const OrderView &o, uint8_t screen,
                void (*flush)(String &)) {
  const Strings &t = *shell.t;
  const ScreenSettings *b = screenSettings(screen);
  Shell sh = shell;
  sh.title = t.*SCREEN_INFO[screen].name;
  sh.tab = TAB_ORDER;
  sh.tabs = true;
  char slug[SCREEN_SLUG_MAX];
  screenSlug(screen, slug, sizeof(slug));
  const bool form = b && hasInputs(*b);
  const bool posted = orderPosted(o);
  shellStart(s, sh);
  s += "<div class=two><aside><a class=bk href=\"/screens\">";
  s += t.webTabs[TAB_ORDER];
  s += "</a><h1>";
  s += sh.title;
  s += "</h1>";
  if (form) {
    if (posted) {
      s += "<form id=sv data-d=/screens/";
      s += slug;
      s += " method=post action=\"/sauver\">";
    } else {
      s += "<form id=sv method=post action=\"/sauver\">";
    }
    s += "<input type=hidden name=pg value=screens/";
    s += slug;
    s += "><button hidden>";
    s += t.webSave;
    s += "</button></form>";
    resultBanner(s, t, o);
    saveBarStart(s, t, "dk", posted);
    s += "</div>";
  }
  s += "</aside><div class=sp>";
  flush(s);
  for (uint8_t k = 0; b && k < b->count; k++) {
    field(s, t, o, b->fields[k]);
    if (s.length() > SCREENS_CHUNK) flush(s);
  }
  if (form) {
    s += "<p id=ann class=vh aria-live=polite></p>";
    saveBarStart(s, t, "mb", posted);
    s += "</div>";
  }
  s += "</div></div>";
  flush(s);
  shellEnd(s, sh, form ? UNSAVED_SCRIPT : nullptr, b ? b->script : nullptr);
}

// With the script the handles stand in for the arrows (the CSS hides
// those). ArrowUp or ArrowDown on a handle moves its row a place, keeps the
// focus on the handle and says the screen's new place through c()
// (UNSAVED_SCRIPT, which runs first); n() renumbers the rows and names each
// handle again from the list's pattern (data-h, "*" for the screen, the
// place and the count). On Discard's reset, the rows go back to the order
// they were loaded in.
// A drag by the handle (pointer events: a mouse, a finger, a pen): the row
// follows the pointer, the rows it passes step aside by its height, and its
// place t is the count of the other rows whose middle is above its own.
// Once it has moved, near the window's top or bottom edge (60 px) the page
// scrolls, once per frame, so a row can cross the whole list on a phone.
// The drop moves the row in the page, and with it its order field: the
// numbers, the names, the bar and the announcement follow as after a key.
// A cancelled gesture (the system taking the touch) puts the row back. No
// "<" in the script: the comparisons are written with ">".
// The dot on the screen on show follows the panel: every 2 s, the page
// being visible, the Stage page's own GET of /preview (the server answers no
// HEAD there) names the screen in X-Screen; its row gets class=at and the
// hidden "on stage" (data-l), the others lose them. A reply without the
// header, or none, leaves things as they are.
const char SCREENS_SCRIPT[] =
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

// The suggestions under the city field. The field becomes a combobox (the
// markup stays a plain field, which posts city, until this runs); the list is
// buttons, an option each, the focus staying in the field (the arrows move
// aria-activedescendant). A reply is shown only if no key came since its
// question (K); a refusal or an error hides the list and leaves the field to
// post what was typed. A 503 (the panel busy, or the search still running)
// is asked again a second later, nine times at most, while no key comes. A
// choice fills the name and the id; the save posts both, and the panel
// takes the id (web.cpp); typing, or Discard, takes the id back.
// The Found row of a city still being looked up (#fd) asks /place every 2 s,
// 30 times at most, and shows the place, or the sentence of data-f when the
// city is not found; a failed question is asked again.
// The buttons are in no form (a form=sv one would count in the unsaved bar),
// and sit at the width of the column with the style the stylesheet has no
// room for. No "<" in the script: the comparisons are written with ">".
const char CITY_SCRIPT[] =
    "{let I=city,L=pl,N=pn,C=cid,T,K=0,X=-1,o=()=>[...L.children],"
    "a=(n,v)=>I.setAttribute(n,v),"
    "h=()=>{K++;clearTimeout(T);L.hidden=true;N.textContent='';a('aria-expanded',false);"
    "I.removeAttribute('aria-activedescendant');X=-1},"
    "v=i=>{o().forEach((b,j)=>b.setAttribute('aria-selected',j==i));X=i;"
    "i>=0?(a('aria-activedescendant','o'+i),o()[i].scrollIntoView({block:'nearest'}))"
    ":I.removeAttribute('aria-activedescendant')},"
    "p=b=>{I.value=b.dataset.n;C.value=b.dataset.i;C.disabled=false;"
    "h();I.focus();c(b.textContent)},"
    "s=r=>{L.replaceChildren(...r.slice(0,5).map((e,i)=>{let b=document.createElement('button'),"
    "t=document.createElement('span');b.type='button';b.id='o'+i;b.className='op';b.tabIndex=-1;"
    "b.setAttribute('role','option');b.setAttribute('aria-selected',false);b.style.width='100%';"
    "t.style.cssText='text-align:left;white-space:normal';"
    "t.textContent=[e.n,[e.a,e.c].filter(x=>x).join(', ')].filter(x=>x).join(' \\u00b7 ');"
    "b.append(t);b.dataset.n=e.n;b.dataset.i=e.id;b.onclick=()=>p(b);return b}));"
    "let n=L.children.length;L.hidden=!n;a('aria-expanded',!!n);v(-1);"
    "N.textContent=n?N.dataset.m.replace('*',n):N.dataset.z},"
    "f=(q,t=0)=>{let k=++K;fetch('/places?q='+encodeURIComponent(q))"
    ".then(r=>r.ok?r.json():Promise.reject(r.status)).then(r=>k==K&&s(r))"
    ".catch(e=>{if(k==K){h();if(e==503&&9>t)T=setTimeout(()=>f(q,t+1),1e3)}})};"
    "C.disabled=true;C.value='';I.autocomplete='off';a('role','combobox');a('aria-autocomplete','list');"
    "a('aria-controls','pl');a('aria-expanded',false);"
    "I.oninput=()=>{K++;clearTimeout(T);C.disabled=true;C.value='';I.name='city';v(-1);"
    "let q=I.value.trim();if([...q].length>1&&!(new TextEncoder().encode(q).length>32))"
    "T=setTimeout(()=>f(q),250);else h()};"
    "I.onkeydown=e=>{let n=o().length,k=e.key,d=k=='ArrowDown';"
    "if(n&&!L.hidden&&(d||k=='ArrowUp')){e.preventDefault();v(d?Math.min(X+1,n-1):Math.max(X-1,0))}"
    "else if(k=='Enter'&&X>=0&&!L.hidden){e.preventDefault();p(o()[X])}"
    "else if(k=='Escape'&&!L.hidden){e.preventDefault();h()}else if(k=='Tab')h()};"
    "document.onclick=e=>L.contains(e.target)||e.target==I||h();"
    "sv.addEventListener('reset',()=>{C.disabled=true;C.value='';I.name='city';h()});"
    "if(window.fd){let n=0,w=()=>{n++>29||fetch('/place').then(r=>r.json())"
    ".then(r=>{if(r.p||r.f)fd.textContent=r.p||fd.dataset.f;else setTimeout(w,2e3)})"
    ".catch(()=>setTimeout(w,2e3))};setTimeout(w,2e3)}}";
