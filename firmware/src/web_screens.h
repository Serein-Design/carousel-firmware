// SPDX-License-Identifier: AGPL-3.0-only
// Running order, without the web server: the owner's list (on/off boxes,
// arrows, the rotation's interval), a poster per screen, a one-line summary
// of each screen's settings and the way to its own page, /screens/<slug>,
// where the settings that screen declares are. Checked on the PC by
// test/test_web_screens; web.cpp sends what it builds.
//
// The page's one form (sv) sits in the side column; every control joins it
// through form=sv.
#pragma once

#include <Arduino.h>

#include "content_logic.h"
#include "i18n.h"
#include "screen_list.h"
#include "settings.h"
#include "web_shell.h"

// IMAGE's block: how many images are stored and which one the panel shows
// ("" = all in turn); its forms are on the Images page
struct ImagesView { bool storage; int count; const char *choice; };

struct ScreensView {
  const Settings *values;   // what the page shows: stored, or as posted after a refusal or a failed write
  const Settings *stored;   // what is stored now: the originals (FIELD_ORIGINAL) and SECRET marks come from it
  ListCheck refusal;        // LIST_OK: none
  bool locateFailed;        // weather.h: the city was not found
  ImagesView images;
};

// SOCIAL's block: the counts the panel fetched (ok false: unknown, "--") and
// how long ago
struct SocialCounts { bool ytOk, ghOk; long yt, gh; uint32_t agoMin; };

// What a save did: nothing (a plain visit), stored, refused by the flash,
// refused for its screen list (the list's banner), or stored with the list
// not taken by the gallery (its queue full for 1 s), so applied at the next
// restart
enum SaveResult : uint8_t { SAVE_NONE, SAVE_DONE, SAVE_FAILED, SAVE_REFUSED, SAVE_LATER };

struct OrderView {
  ScreensView screens;
  uint8_t current; bool portrait;   // the screen on show, among the stored list's
  SocialCounts social;
  SaveResult result;
};

// Which page a save answers: the form's hidden pg. "screens": Running
// order; "screens/<slug>" of a screen with a page: that page, *screen its
// index (the page is the table's, never the posted text); "panel/feedback":
// Feedback's page; anything else, or
// none (a script, the Panel page): Panel.
enum SavePage : uint8_t { SAVE_TO_PANEL, SAVE_TO_ORDER, SAVE_TO_SCREEN, SAVE_TO_FEEDBACK };
SavePage saveAnswerPage(const char *pg, uint8_t *screen);
// GET /screens/<slug>: the screen whose page it is (one with settings);
// -1 for any other slug, the house 404
int screenPageFind(const char *slug);

// What a screen's block holds, one entry per piece, in order. ROW, CELL and
// ROW_END lay fields side by side (.r); FOUND is the place the city found.
// SUGGEST is the city field's suggestions, under it: the chosen place's id
// (off until the script chooses one), the list and the line that counts it.
// ORIGINAL is a hidden copy of a stored value, posted back so the save can
// tell an edit from a value that changed elsewhere since the page loaded.
// IMAGES says which image shows and links to Images; COUNTS is SOCIAL's
// fetched counts. STYLE is a screen's style, one card per option (its
// name picks the options: cs CODE's, ks CLOCK's, ws WEATHER's); SHOWS is
// what CLOCK shows besides the time, one box per ClockShow bit.
enum FieldKind : uint8_t { FIELD_TEXT, FIELD_NUMBER, FIELD_SECRET, FIELD_HEADING, FIELD_HINT,
                           FIELD_ROW, FIELD_CELL, FIELD_ROW_END, FIELD_FOUND, FIELD_IMAGES,
                           FIELD_ORIGINAL, FIELD_COUNTS, FIELD_STYLE, FIELD_SHOWS, FIELD_SUGGEST };
struct ScreenField {
  FieldKind kind;
  const char *const Strings::*text;   // label, heading, hint or legend; nullptr for marks (HEADING: name is a literal then)
  const char *name;                    // form name and id, the names /sauver already reads
  const char *attrs;                   // keyboard, length, placeholder; SECRET: when nothing is stored
  String (*value)(const Settings &);   // TEXT, NUMBER; SECRET: non-empty in stored = one is stored (never sent);
                                       // STYLE: the chosen option, SHOWS: the bits, as a number
};
struct ScreenSettings {
  // Running order's one line under the name: the current choice, escaped;
  // nothing appended when there is nothing to say
  void (*summary)(String &, const Strings &, const ScreensView &);
  const ScreenField *fields; uint8_t count;
  const char *script;   // the page's own script, after the unsaved bar's; nullptr: none
};

// A number field as the save reads it: a number as people type it (the
// fields are plain text, with no browser bounds), digits grouped by spaces,
// commas or points ("1,240", "1 240"), decimals dropped, held to [lo, hi].
// Emptied: lo. Any other text ("1e", "1.2k") keeps stored, held too. Read
// into a uint8_t unbounded, 300 would be 44.
long formNumber(const char *text, long lo, long hi, long stored);
// An id the page posts (a suggestion's number in the city table): digits
// only and at most hi, else -1, none. Not formNumber, which would read
// "abc" or "-1" as lo, a real id.
long formId(const char *text, long hi);
// The salary as typed: "2850.5", "2 850,50", "2.850,50" and "2,850.50"
// alike. Emptied: 0. Any other text keeps stored.
float formDecimal(const char *text, float stored);

// Whether the save writes a field: what was posted differs from the
// original the page carried (FIELD_ORIGINAL); without one (an older page, a
// script), always. The message is set over MQTT and the API too: a save
// that did not touch the field must not put back the one the page showed.
bool formEdited(const char *posted, const char *original);

// The style controls as the browser posts them, by name: cs, ks and ws an
// option's number, kw a ClockShow bit's number per ticked box, kwh present
// whenever the boxes were on the page (so all unticked is told from absent).
// What a post leaves out, or is not an option, keeps the stored value; of
// two values for one style the first option counts, as a radio group posts one.
class StylesForm {
public:
  void arg(const char *name, const char *value);
  void apply(Settings &s) const;

private:
  int8_t code_ = -1, clock_ = -1, weather_ = -1;   // -1: not posted
  bool showsPosted_ = false;
  uint8_t shows_ = 0;
};

// The MQTT password field's attributes (web.cpp's form): the placeholder
// says one is stored from what is stored, never from a post, so a password
// typed in a refused save is not shown as kept. The password is never sent.
const char *mqttPassAttrs(const Settings &stored);

// The block screen_table.h's settings column names for a screen; nullptr:
// none, so no block
const ScreenSettings *screenSettings(uint8_t screen);

// The page goes out in pieces: s is handed to flush (which sends and
// empties it) after the side column and once it passes SCREENS_CHUNK after
// a row, so no piece outgrows ORDER_PAGE_RESERVE, even with every value at
// its longest and escaped (test_page_fits_24k). What is left in s at the
// end is the caller's to send.
static const size_t SCREENS_CHUNK = 3072;
static const size_t ORDER_PAGE_RESERVE = 6144;
void orderPage(String &s, const Shell &sh, const OrderView &v, void (*flush)(String &));
// A screen's page (screen: one with settings), sent the same way, its
// pieces within ORDER_PAGE_RESERVE too (test_page_fits_24k)
void screenPage(String &s, const Shell &sh, const OrderView &v, uint8_t screen,
                void (*flush)(String &));

// The board's script, under 1600 bytes and without any data: moves a row at
// once by its handle, dragged or with ArrowUp and ArrowDown, renumbers,
// names the handles again, keeps the focus, announces "NAME 2/24".
// Without it there is no handle: an arrow is a submit button and the save
// moves the screen.
extern const char SCREENS_SCRIPT[];

// WEATHER's page script, about 2 KB and without any data: from 2 characters,
// 250 ms after the last key, asks GET /places?q= and lists up to 5 places
// under the city field as buttons ("Paris · Île-de-France, France"); a
// choice fills the field with the name and the hidden cid with the id (and
// the field stops posting city), the arrows, Enter and Escape work from the
// field, and the count is said in the live line. Without it the field posts
// city as ever.
extern const char CITY_SCRIPT[];
