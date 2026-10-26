// SPDX-License-Identifier: AGPL-3.0-only
// Coding agent bridge: the session table fed by the hooks of Claude Code,
// Codex CLI, Cursor and Gemini CLI.
//
// Requests arrive on the web server, which runs on core 0. It never touches
// the table: it posts a AgentEvent to a queue, and loop() applies it between
// two frames. loop() is the only writer of the table.
//
// No display code here: this module also builds on the host for its tests.
#pragma once

#include <Arduino.h>

enum AgentState : uint8_t {
  AG_IDLE = 0, AG_THINKING, AG_TOOL, AG_ATTENTION, AG_ERROR, AG_DONE, AG_FAILED
};
#define AGENT_STATE_COUNT 7

// Names used on the wire, in AgentState order: English (/agent/<name>, the
// documented ones) and French (the first hooks', still installed). Both sets
// work on both routes, plus "end" and "fin". They are the protocol the
// installed hooks speak: changing one breaks existing setups.
extern const char *const AGENT_STATE_NAMES[AGENT_STATE_COUNT];
extern const char *const AGENT_STATE_NAMES_FR[AGENT_STATE_COUNT];

// Session tints, in assignment order
#define TINT_COUNT 6
extern const uint8_t SESSION_TINTS[TINT_COUNT][3];

#define MAX_SESSIONS 6

// Durations in milliseconds, not frames: frame time varies with the scene,
// and an animation counted in frames stretched or shrank.
#define CELEBRATE_MS     1500UL    // "OK!" after a Stop
#define KO_MS            10000UL   // StopFailure: the turn ended on an error
#define STARTLE_MS       800UL     // tool failure: he jumps, then carries on
#define WARMUP_MS        470UL     // thinking -> coding: he cracks his knuckles

struct AgentSession {
  char        id[20]     = "";       // start of the session_id, enough to tell them apart
  char        name[14]   = "";       // last segment of the cwd
  AgentState state      = AG_IDLE;
  uint32_t    lastSeen   = 0;        // millis() of the last event
  uint32_t    enteredAt  = 0;        // millis() when the current state was entered
  uint32_t    lastToolAt = 0;        // millis() of the last PreToolUse
  uint32_t    warmupAt   = 0;        // millis() of the last warm-up, 0 = none
  uint64_t    clientTime = 0;        // PC timestamp of the last applied event
  uint8_t     tint       = 0;
  bool        active     = false;
  bool        ghost      = false;    // restored from NVS, not seen since
};

extern AgentSession sessions[MAX_SESSIONS];
extern int      focusedSession;
extern uint32_t sessionSwitchedAt;
extern uint32_t manualPickAt;        // last session picked with the button, 0 = none

// What the web server hands over to loop(). state is a AgentState, or one of
// the commands below. t: timestamp set by the PC when the hook fired
// (microseconds), 0 if the request carries none.
struct AgentEvent {
  uint64_t t;
  int8_t   state;
  char     id[20];
  char     name[14];
};
#define EV_END     (-1)              // SessionEnd: the session leaves the table
#define EV_PURGE   (-2)              // "Clear the list" button on the web page
#define EV_UNKNOWN (-3)

// Wire name, English or French -> AgentState, EV_END or EV_UNKNOWN
int8_t parseAgentState(const char *name);

void agentBridgeBegin();                       // restores sessions, creates the queue
bool postAgentEvent(const AgentEvent &ev);    // web task side, never blocks
void processAgentEvents();                     // loop() side
void applyAgentEvent(const AgentEvent &ev);
void updateSessions();                          // expiries, fallbacks, deferred NVS write

// Read-only, also called from the web page on the other core
int activeSessionCount();
int nthActiveSession(int rank);                 // index of the rank-th active session, -1 if none
AgentState displayedState(int i, uint32_t now);

// Last segment of a path, whatever the separator
void projectName(const char *path, char *out, size_t size);
