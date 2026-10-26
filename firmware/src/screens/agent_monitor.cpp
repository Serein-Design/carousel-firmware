// SPDX-License-Identifier: AGPL-3.0-only
// CODE: the coding agents' sessions, as the owner chose on Running order.
// The cloud (the mascot, the focused session's state in a word, its project
// and minutes on the label line, one tinted dash per session) or the list
// (one row per session, the ones that need the owner first and the only
// orange). Auto shows the cloud below three sessions, the list from three.
// What to show is decided in code_view_logic; this file only draws it.
#include <WiFi.h>

#include "agent_bridge.h"
#include "code_view_logic.h"
#include "display.h"
#include "grid.h"
#include "i18n.h"
#include "palette.h"
#include "screens/mascot.h"
#include "screens/screens.h"
#include "settings.h"
#include "type.h"

// 5-row icons, '#' lit, from the board
static const char *const ICON_CHECK[5] = {"....#", "...#.", "#.#..", ".#...", "....."};
static const char *const ICON_BANG[5]  = {"#", "#", "#", ".", "#"};

static void drawIcon(const char *const rows[5], int x, int y, uint16_t c) {
  for (int r = 0; r < 5; r++)
    for (int i = 0; rows[r][i]; i++)
      if (rows[r][i] == '#') display->drawPixel(x + i, y + r, c);
}

// A session's tint (content colour, like the mascot's): full for the session
// in focus, half for the others
static uint16_t tintOf(const AgentSession &s, bool focused) {
  const uint8_t *c = SESSION_TINTS[s.tint % TINT_COUNT];
  const int k = focused ? 2 : 1;
  return display->color565(c[0] * k / 2, c[1] * k / 2, c[2] * k / 2);
}

static AgentState shownState(int i, uint32_t now) {
  const AgentSession &s = sessions[i];
  return codeShownState(displayedState(i, now), s.enteredAt, s.lastSeen, now);
}

// The word's colour: orange only when the owner is needed, the alert red
// for an error (the word says which), the text colour otherwise
static uint16_t wordColor(const Palette &p, AgentState st) {
  if (codeNeedsOwner(st)) return p.accent;
  if (st == AG_ERROR || st == AG_FAILED) return p.alert;
  return p.text;
}

// 1 Hz, lit 600 ms of each second: the board's "needs you" beat
static bool beat(uint32_t age) { return age % 1000 < 600; }

// --- K1, the cloud ---------------------------------------------------------
// The mascot sits right of the main and foot rows; its own colours (content).
static const int CLOUD_X = 44, CLOUD_Y = 19;
static const int WORD_Y  = GRID_MAIN_TOP + 1;   // room for the capitals' accents
static const int DASH_Y  = 27;                  // the foot rows' middle

static void drawCloudMascot(const Palette &p, AgentState st, uint32_t age, bool waiting) {
  MascotPose pose;
  pose.eyes = (age % 3200) < 120 ? EYES_CLOSED : EYES_OPEN;   // a blink now and then
  int bob = 0;
  if (waiting) {
    pose.gaze = (int)((age / 1300) % 3) - 1;   // looks around for a hook
  } else if (st == AG_IDLE) {
    pose.eyes = EYES_CLOSED;
    pose.squash = 1;
  } else if (st == AG_THINKING) {
    pose.gaze = (int)((age / 1300) % 3) - 1;
  } else if (st == AG_TOOL) {
    bob = (int)((age / 250) % 2);
    pose.legs = 1 + bob;                     // the drizzle under it alternates
  } else if (st == AG_ATTENTION || st == AG_FAILED) {
    pose.armL = pose.armR = 1;               // hands up
  } else if (st == AG_ERROR) {
    pose.eyes = EYES_KO;
    pose.squash = 1;
  } else if (st == AG_DONE) {
    pose.eyes = EYES_HAPPY;
  }
  drawMascot(CLOUD_X, CLOUD_Y + bob, 1, pose);

  if (waiting) return;
  const int cx = CLOUD_X, cy = CLOUD_Y;
  switch (st) {
    case AG_THINKING:   // three dots fill up above the cloud, then start over
      for (int i = 0; i < 3; i++)
        if ((int)((age / 400) % 4) > i) display->drawPixel(cx + 5 + i * 3, cy - 2, p.text);
      break;
    case AG_TOOL: {     // it rains code, in rows 29-30: row 31 is the status strip's
      const uint16_t rain = display->color565(76, 143, 232);
      for (int k = 0; k < 4; k++) {
        const int ph = (int)((age / 70 + k) % 2);
        display->drawPixel(cx + 3 + k * 4, cy + 10 + ph, rain);
      }
      break;
    }
    case AG_ATTENTION:
      if (beat(age)) drawIcon(ICON_BANG, cx - 3, cy, p.accent);
      break;
    case AG_ERROR:      // the turn stopped: the same call, in red
      if (beat(age)) drawIcon(ICON_BANG, cx - 3, cy, p.alert);
      break;
    case AG_FAILED:     // a tool failed, 0.8 s: one steady mark, then back to thinking
      drawIcon(ICON_BANG, cx - 3, cy, p.alert);
      break;
    case AG_DONE:
      drawIcon(ICON_CHECK, cx - 7, cy + 2, p.accent);
      break;
    case AG_IDLE:
      break;
  }
}

static void drawCloud(const Palette &p, uint32_t now, int nSess, int idx) {
  const Strings &t = tr();
  if (idx < 0) {
    // No session yet: the address to put in the hooks, on the label line
    const bool wifi = WiFi.status() == WL_CONNECTED;
    if (wifi) {
      const IPAddress a = WiFi.localIP();   // printed by hand: toString() allocates
      char ip[16];
      snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
      drawText(*display, ip, GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.label);
    }
    drawText(*display, wifi ? t.codeWaiting : t.codeNoWifi, GRID_MARGIN, WORD_Y, Face::BODY,
             wifi ? p.text : p.alert);
    drawCloudMascot(p, AG_IDLE, now, true);
    return;
  }

  const AgentSession &s = sessions[idx];
  const AgentState st = shownState(idx, now);
  const uint32_t age = now - s.enteredAt;

  char mins[12], name[sizeof(s.name)];
  snprintf(mins, sizeof(mins), t.codeMin, codeMinutes(age));
  const int right = W() - GRID_MARGIN;
  drawText(*display, mins, right, GRID_LABEL_TOP, Face::LABEL, p.label, Align::RIGHT);
  const int room = right - GRID_MARGIN - textWidth(mins, Face::LABEL) - 4;
  codeFitLabel(s.name, Face::LABEL, room, name, sizeof(name));
  drawText(*display, name, GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.label);

  drawText(*display, codeStateWord(st, t), GRID_MARGIN, WORD_Y, Face::BODY, wordColor(p, st));
  drawCloudMascot(p, st, age, false);

  // One dash per session in its tint, the one in focus longer and brighter
  int x = GRID_MARGIN;
  for (int r = 0; r < nSess; r++) {
    const int i = nthActiveSession(r);
    if (i < 0) continue;
    const bool f = (i == idx);
    display->drawFastHLine(x, DASH_Y, f ? 5 : 3, tintOf(sessions[i], f));
    x += f ? 7 : 5;
  }
}

// --- K2, the list ----------------------------------------------------------
static const int ROW_H = 8, NAME_X = 7, MINS_RIGHT = 54, GLYPH_X = 56;

static void drawGlyph(const Palette &p, AgentState st, uint32_t age, int x, int y) {
  switch (st) {
    case AG_THINKING:   // three dots, the lit one walking
      for (int i = 0; i < 3; i++)
        display->drawPixel(x + i * 2, y + 4, (int)((age / 300) % 3) == i ? p.text : p.dim);
      break;
    case AG_TOOL: {     // a bar that grows
      const int h = 1 + (int)((age / 180) % 4);
      display->fillRect(x + 2, y + 5 - h, 2, h, p.text);
      break;
    }
    case AG_ATTENTION:
      if (beat(age)) drawIcon(ICON_BANG, x + 2, y, p.accent);
      break;
    case AG_ERROR:
      if (beat(age)) drawIcon(ICON_BANG, x + 2, y, p.alert);
      break;
    case AG_FAILED:
      drawIcon(ICON_BANG, x + 2, y, p.alert);
      break;
    case AG_DONE:
      drawIcon(ICON_CHECK, x, y, p.label);
      break;
    case AG_IDLE:
      display->drawFastHLine(x + 1, y + 2, 3, p.dim);
      break;
  }
}

static void drawList(const Palette &p, uint32_t now, int nSess, int idx) {
  AgentState st[MAX_SESSIONS];
  int which[MAX_SESSIONS];
  int n = 0;
  for (int r = 0; r < nSess && n < MAX_SESSIONS; r++) {
    const int i = nthActiveSession(r);
    if (i < 0) continue;
    which[n] = i;
    st[n++] = shownState(i, now);
  }
  uint8_t rows[CODE_LIST_ROWS];
  const int count = codeListOrder(st, n, rows, CODE_LIST_ROWS);

  for (int k = 0; k < count; k++) {
    const int i = which[rows[k]];
    const AgentSession &s = sessions[i];
    const AgentState state = st[rows[k]];
    const uint32_t age = now - s.enteredAt;
    const int y = GRID_LABEL_TOP + k * ROW_H;

    display->drawFastHLine(GRID_MARGIN, y + 2, 3, tintOf(s, i == idx));

    char mins[8], name[sizeof(s.name)];
    snprintf(mins, sizeof(mins), tr().codeMinShort, codeMinutes(age));
    drawText(*display, mins, MINS_RIGHT, y, Face::LABEL, p.label, Align::RIGHT);
    const int room = MINS_RIGHT - textWidth(mins, Face::LABEL) - 3 - NAME_X;
    codeFitLabel(s.name, Face::LABEL, room, name, sizeof(name));
    drawText(*display, name, NAME_X, y, Face::LABEL, codeNeedsOwner(state) ? p.accent : p.text);

    drawGlyph(p, state, age, GLYPH_X, y);
  }
}

// Everything is derived from the session table and the time, both kept by
// applyAgentEvent() and updateSessions(): drawing changes nothing.
void screenAgentMonitor(uint32_t, bool) {
  const uint32_t now = millis();
  const int nSess = activeSessionCount();
  int idx = -1;
  if (nSess > 0) {
    if (focusedSession < 0 || focusedSession >= MAX_SESSIONS || !sessions[focusedSession].active)
      focusedSession = nthActiveSession(0);
    idx = focusedSession;
  }

  display->clearScreen();
  const Palette &p = palette();
  if (codeShowsList(settings.codeStyle, nSess)) drawList(p, now, nSess, idx);
  else                                          drawCloud(p, now, nSess, idx);
}
