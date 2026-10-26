// SPDX-License-Identifier: AGPL-3.0-only
// Stage, the home page: the stage block (the live preview, the name of the
// screen on show, its place and the next change), Bring on a screen, House
// lights, the running-order strip, Say something and the coding sessions. Built without the web server: web.cpp gathers the
// gallery's state and a copy of the session table. Checked on the PC by
// test/test_web_stage; test/test_web_pages writes it for the browser checks.
#pragma once

#include <Arduino.h>

#include "screen_list.h"
#include "web_shell.h"

// One session as the page shows it, copied from the table by web.cpp
struct SessionRow {
  char name[14];      // the project, as the hook sent it: escaped here
  uint8_t tint[3];    // its colour on the CODE screen
  uint8_t state;      // a AgentState
  uint32_t ageSec;    // since it was last heard
};

// What a refused or half-done action says, in the section it came from
enum StageNotice : uint8_t { STAGE_NO_NOTICE, STAGE_SHOW_OFF, STAGE_SHOW_OTHER, STAGE_SHOW_UNKNOWN,
                             STAGE_TEXT_OFF, STAGE_TEXT_PORTRAIT, STAGE_TEXT_FAILED };

struct StageView {
  ScreenList list; uint8_t current; bool portrait;
  uint16_t rotateSec; int32_t nextIn; uint32_t cannotRotate;   // IMAGE's bit when !contentPlayable(portrait)
  const SessionRow *sessions; int sessionCount;
  int level, levels;                                  // brightness step, 0 .. levels - 1
  uint8_t nightStart, nightEnd, nightBrightness;      // as stored; same hours: no night mode
  const char *message;                                // the stored one, escaped here
  StageNotice notice; const char *noticeName;         // the screen a Show notice names
};

// What Show does with a posted name, in the API's order: off before the
// orientation. Over 23 characters (the longest name): unknown.
enum ShowOutcome : uint8_t { SHOW_OK, SHOW_OFF, SHOW_OTHER, SHOW_UNKNOWN };
ShowOutcome showOutcome(const ScreenList &l, const char *name, bool portrait);
// Send stored the message: what the page says when TEXT will not show it
// (on: in the list; fits: drawn for this orientation). Off says so first:
// turning the panel would not show it either.
StageNotice textNotice(bool on, bool fits);

// n: the place of current among the screens on and fitting the orientation,
// in list order, from 1 (0: not among them); count: how many they are
struct StagePlace { uint8_t n, count; };
StagePlace stagePlace(const ScreenList &l, uint8_t current, bool portrait);
// The screen the rotation brings on after current, as the gallery picks it:
// on, fitting, not in skip (bit i: screen i). -1: none but current.
int  stageNext(const ScreenList &l, uint8_t current, bool portrait, uint32_t skip);
// Up to max screens on and fitting, from the one before current, wrapping;
// out gets their firmware indexes. Returns how many.
static const int STAGE_STRIP_MAX = 7;
int  stageStrip(const ScreenList &l, uint8_t current, bool portrait, uint8_t *out, int max);

// What /preview answers beside its frame, as it stood when the frame was
// read, so every open Stage page follows the panel within a second:
// X-Screen (English name), X-Brightness (step), and while the rotation runs
// X-Next-In (s, -1: not counting) and, when another screen comes on, X-Next
// (English name, as stageNext picks it). Returns how many.
struct StageHeader { const char *name; char value[24]; };
static const int STAGE_HEADER_MAX = 4;
int  stageHeaders(const StageView &v, StageHeader *out);

// How long ago, in s, min or h, escaped (Stage's sessions, Panel's last restart)
void ageText(String &s, const Strings &t, uint32_t sec);

// The whole page; flush sends what s holds so far and empties it
void stagePage(String &s, const Shell &sh, const StageView &v, void (*flush)(String &));

// Refreshes the preview every second and, from its headers, the name,
// place, next line and level, only while the page is visible. Static: what
// it needs is in the stage block's data attributes.
extern const char STAGE_SCRIPT[];   // <= 1900 bytes, no data, no '%'
