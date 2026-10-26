// SPDX-License-Identifier: AGPL-3.0-only
// What the CODE screen shows, decided apart from the drawing: the cloud or
// the list, the word for a state, the order of the list's rows. Pure logic
// on the session states (agent_bridge.h for the enum only): the screen
// (screens/agent_monitor.cpp) feeds it and draws; test/test_code_view checks
// it on the PC.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "agent_bridge.h"
#include "i18n.h"
#include "type.h"

// Auto shows the list from this many sessions: below it, the cloud's one
// dash per session still tells them apart.
static const int CODE_LIST_FROM = 3;
static const int CODE_LIST_ROWS = 4;   // 8 rows each fill the 32 of the panel

// The board's done check stays this long before idle. The bridge's own
// return to idle (CELEBRATE_MS) comes earlier; the screen holds the check
// instead of changing the session data.
static const uint32_t CODE_DONE_HOLD_MS = 2500;

// style: a CodeStyle (screen_style.h); sessions: the active ones. Without
// a session there is no row to list: the cloud says it is waiting.
bool codeShowsList(uint8_t style, int sessions);

// The state as the screen draws it: an idle that the bridge's done
// fallback entered (after the Stop, nothing seen since) still shows done
// until CODE_DONE_HOLD_MS after that Stop. An idle the hooks sent is idle.
AgentState codeShownState(AgentState state, uint32_t enteredAt, uint32_t lastSeen,
                           uint32_t now);

// The state in a word, in the body face (each fits GRID_LINE_W)
const char *codeStateWord(AgentState state, const Strings &t);

// The one state that needs the owner, the only orange row
bool codeNeedsOwner(AgentState state);

// The list's rows: states[] in rank order (nthActiveSession), n of them.
// The sessions that need the owner come first, then the others, each group
// in rank order; at most max ranks written to out. Returns the count.
int codeListOrder(const AgentState *states, int n, uint8_t *out, int max);

// name in capitals (the label face has no lower case), cut to fit maxW
// pixels in face f
void codeFitLabel(const char *name, Face f, uint16_t maxW, char *out, size_t size);

// Elapsed minutes from milliseconds, capped at 99 so the label keeps its width
unsigned codeMinutes(uint32_t ms);
