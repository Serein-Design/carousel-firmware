// SPDX-License-Identifier: AGPL-3.0-only
#include "code_view_logic.h"

#include <string.h>

#include "screen_style.h"

bool codeShowsList(uint8_t style, int sessions) {
  if (sessions <= 0) return false;
  if (style == CODE_LIST) return true;
  if (style == CODE_CLOUD) return false;
  return sessions >= CODE_LIST_FROM;
}

// The bridge's only return to idle within the hold is done's (CELEBRATE_MS):
// the error's comes after KO_MS, and a failed tool goes back to thinking.
static_assert(CELEBRATE_MS < CODE_DONE_HOLD_MS && CODE_DONE_HOLD_MS < KO_MS,
              "only done falls back to idle within the hold");

AgentState codeShownState(AgentState state, uint32_t enteredAt, uint32_t lastSeen,
                           uint32_t now) {
  // A fallback leaves lastSeen at the last event; a hook sets both at once
  const bool fellBack = enteredAt != lastSeen;
  if (state == AG_IDLE && fellBack && now - lastSeen < CODE_DONE_HOLD_MS) return AG_DONE;
  return state;
}

const char *codeStateWord(AgentState state, const Strings &t) {
  switch (state) {
    case AG_THINKING:  return t.codeThinking;
    case AG_TOOL:      return t.codeWorking;
    case AG_ATTENTION: return t.codeNeedsYou;
    case AG_ERROR:     return t.codeFailed;
    case AG_DONE:      return t.codeDone;
    case AG_FAILED:    return t.codeToolError;
    case AG_IDLE:      break;
  }
  return t.codeIdle;
}

bool codeNeedsOwner(AgentState state) { return state == AG_ATTENTION; }

int codeListOrder(const AgentState *states, int n, uint8_t *out, int max) {
  int count = 0;
  for (int pass = 0; pass < 2; pass++)
    for (int r = 0; r < n && count < max; r++)
      if (codeNeedsOwner(states[r]) == (pass == 0)) out[count++] = (uint8_t)r;
  return count;
}

void codeFitLabel(const char *name, Face f, uint16_t maxW, char *out, size_t size) {
  size_t n = 0;
  for (; name[n] && n + 1 < size; n++) {
    const char c = name[n];
    out[n] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
  }
  out[n] = '\0';
  // Cut from the end a whole character at a time: half a UTF-8 sequence
  // would draw as a box
  while (n > 0 && textWidth(out, f) > maxW) {
    do n--; while (n > 0 && ((uint8_t)out[n] & 0xC0) == 0x80);
    out[n] = '\0';
  }
}

unsigned codeMinutes(uint32_t ms) {
  const uint32_t m = ms / 60000UL;
  return m > 99 ? 99 : (unsigned)m;
}
