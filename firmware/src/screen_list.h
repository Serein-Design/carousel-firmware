// SPDX-License-Identifier: AGPL-3.0-only
// The owner's list of screens, without the hardware: which screens are on,
// in what order, how it is stored, and when a list is refused. Checked on
// the PC by test/test_screen_list.
#pragma once

#include <Arduino.h>

#include "i18n.h"
#include "screen_table.h"

// Each screen declares the orientation it is drawn for. The tilt switch picks
// the gallery, exactly like the simulator: in landscape only LANDSCAPE and
// BOTH are offered, in portrait only PORTRAIT and BOTH.
enum Orientation : uint8_t { LANDSCAPE = 0, PORTRAIT = 1, BOTH = 2 };

struct ScreenInfo {
  const char *const Strings::*name;   // tr().*name; English in storage, API, MQTT
  Orientation orientation;
};

// One bit per screen in ScreenList::on. Past 32 screens, on becomes a
// uint64_t (the stored text, by name, does not change).
static const uint8_t MAX_SCREENS = 32;
#define SCREEN_ONE(name, draw, orientation, leave, settings) +1
static const uint8_t SCREEN_COUNT = 0 SCREEN_TABLE(SCREEN_ONE);
#undef SCREEN_ONE
static_assert(SCREEN_COUNT <= MAX_SCREENS, "ScreenList::on holds one bit per screen");

extern const ScreenInfo SCREEN_INFO[SCREEN_COUNT];   // the firmware's order

const char *screenNameEn(uint8_t i);
int  screenFind(const char *name);                   // English, any case; -1: unknown
bool screenFits(uint8_t i, bool portrait);           // drawn for that orientation

// A screen's address segment (/screens/<slug>): its English name lowercased,
// spaces as "-" ("ISS DEMO": "iss-demo"), cut to fit out. screenFindSlug
// finds a screen from exactly that text; -1 for any other.
static const size_t SCREEN_SLUG_MAX = 24;
void screenSlug(uint8_t i, char *out, size_t size);
int  screenFindSlug(const char *slug);

// The owner's order of every screen, and which are on. Order and on/off are
// stored together, in one key, so they land together or not at all.
struct ScreenList {
  uint8_t  order[MAX_SCREENS] = {};   // firmware indexes, owner's order, each once
  uint32_t on = 0;                    // bit i: firmware screen i is on
};

// Every screen on, in the firmware's order
inline ScreenList screenListAll() {
  ScreenList l;
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) l.order[i] = i;
  l.on = SCREEN_COUNT == 32 ? 0xFFFFFFFFu : (1u << SCREEN_COUNT) - 1;
  return l;
}
// A panel that never saved a list (v0.1.0, a development build, a factory
// reset): the firmware's order, every screen on but BALL, MATRIX, RAYS and
// BARS, demos that are not calm enough to be ambiances. They stay listed, to
// turn on by hand; a stored list keeps them as the owner left them.
ScreenList screenListDefault();
inline bool screenOn(const ScreenList &l, uint8_t i) { return (l.on >> i) & 1u; }
int  screenPlace(const ScreenList &l, uint8_t i);    // index in l.order

// Stored form: English names in order, separated by commas, '-' in front of
// a screen that is off: "NIMBUS,CAT,-SLEEP,...". A stored value longer than
// this reads as absent (defaults).
static const size_t SCREEN_LIST_TEXT_MAX = 256;
// Entries for screens the firmware does not have, given twice (the first
// wins) or unreadable are ignored, so the next save leaves them out; screens
// the text does not name (added by a later update) are on, after the others,
// in the firmware's order.
ScreenList screenListParse(const char *text);
// False when it does not fit; out is then "".
bool screenListFormat(const ScreenList &l, char *out, size_t size);
bool screenListSameView(const ScreenList &a, const ScreenList &b);   // same order and on/off

// An orientation left with no screen on (a crafted or corrupted value, an
// update that re-oriented a screen): every screen drawn for it is turned on,
// in RAM only. True when it filled one; the caller logs it once.
bool screenListFill(ScreenList &l);

// Why a list the owner posted is refused: nothing on at all, or no screen on
// for one orientation. Each one names what to turn on (the page's marks).
enum ListCheck : uint8_t { LIST_OK, LIST_EMPTY, LIST_NO_PORTRAIT, LIST_NO_LANDSCAPE };
ListCheck screenListCheck(const ScreenList &l);
// A list that may be applied: order holds every firmware screen once, no
// on bit names a screen the firmware does not have, and the check passes.
// The loop indexes its draw functions with it, so it checks again what
// crossed from another task.
bool screenListValid(const ScreenList &l);

// "Change screen": 0 = with the knob only, else every N seconds
static const uint16_t ROTATE_CHOICES[] = {0, 10, 20, 30, 60, 120, 300};
static const uint8_t ROTATE_CHOICE_COUNT = sizeof(ROTATE_CHOICES) / sizeof(ROTATE_CHOICES[0]);
bool rotateOffered(long seconds);

// The settings form's list, read field by field as the browser posts them:
// "o" a screen's English name, one per row in the order shown; "on" a ticked
// box; "mv" an arrow pressed without scripts ("<name>:u" or ":d"); "rt" the
// interval in seconds. Names, never positions: a page opened before an
// update that added a screen still names the right ones. Other fields are
// ignored. A crafted post cannot make it loop or grow: at most
// SCREEN_COUNT + 8 "o" and as many "on" values are read, each matched only
// against the firmware's names.
class ScreensForm {
public:
  ScreensForm(const ScreenList &current, uint16_t rotateSec);
  void arg(const char *name, const char *value);
  bool hasList() const { return count_ > 0; }   // at least one known "o"
  bool hasRotate() const { return rotateSet_; } // an offered "rt"
  // The posted order, then the screens the post did not name with their
  // state and order kept; the arrow applied last. Without a list, current.
  ScreenList list() const;
  uint16_t rotateSec() const { return rotate_; }

private:
  static const uint8_t READ_MAX = SCREEN_COUNT + 8;
  ScreenList current_;
  uint8_t order_[MAX_SCREENS] = {};
  uint8_t count_ = 0;            // known names in order_
  uint32_t posted_ = 0, on_ = 0;
  uint8_t orders_ = 0, boxes_ = 0;   // "o" and "on" values read
  int8_t moved_ = -1;            // the arrow's screen, -1: none
  bool up_ = false;
  uint16_t rotate_;
  bool rotateSet_ = false;
};

// POST /api/screen: 200 shown, 404 unknown or drawn only for the other
// orientation, 409 off (before 404: tilting would not show it either)
int screenRequestStatus(const ScreenList &l, const char *name, bool portrait);
// GET /api/info's screens: ,"screens":[{"name","available","enabled"}] in
// the firmware's order, available meaning drawn for this orientation, then
// ,"order":[names] in the list's order, ,"rotate_every":seconds (0 off) and
// ,"rotate_next_in":seconds or null (nextIn < 0)
void screensInfoJson(String &j, const ScreenList &l, uint16_t rotateSec, int32_t nextIn,
                     bool portrait);
