// SPDX-License-Identifier: AGPL-3.0-only
#include "status_strip.h"

#include <math.h>

#include "code_view_logic.h"
#include "screen_table.h"

namespace {
#define STRIP_INDEX(name, draw, orientation, leave, settings) IDX_##name,
enum ScreenIndex : uint8_t { SCREEN_TABLE(STRIP_INDEX) };
#undef STRIP_INDEX

// The mark takes the last 2 columns, 3 dark ones keep it apart from the line
const int16_t MARK_W = 2, MARK_GAP = 3;
}  // namespace

AgentMark agentMark(const AgentState *states, int n) {
  AgentMark m = AgentMark::NONE;
  for (int i = 0; i < n; i++) {
    if (codeNeedsOwner(states[i])) return AgentMark::NEEDS_OWNER;
    // A failed tool is a blip: the session carries on thinking
    if (states[i] == AG_THINKING || states[i] == AG_TOOL || states[i] == AG_FAILED)
      m = AgentMark::WORKING;
  }
  return m;
}

bool screenHasStrip(uint8_t screen) {
  switch (screen) {
    case IDX_scrCode:
    case IDX_scrClock:
    case IDX_scrWeather:
    case IDX_scrDate:
    case IDX_scrSystem:
    case IDX_scrMoney:
    case IDX_scrSocial:
    case IDX_scrText:
      return true;
    default:
      return false;
  }
}

void drawStatusStrip(Adafruit_GFX &g, const Palette &p, uint8_t screen, float fill, AgentMark mark) {
  if (!screenHasStrip(screen)) return;
  const int16_t y = g.height() - 1, w = g.width(), len = w - MARK_W - MARK_GAP;
  if (fill > 0) {
    const int16_t lit = fill >= 1 ? len : (int16_t)lroundf(len * fill);
    if (lit > 0) g.drawFastHLine(0, y, lit, p.ghost);
  }
  if (mark == AgentMark::NONE || screen == IDX_scrCode) return;
  g.drawFastHLine(w - MARK_W, y, MARK_W, mark == AgentMark::NEEDS_OWNER ? p.accent : p.dim);
}
