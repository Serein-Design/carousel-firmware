// SPDX-License-Identifier: AGPL-3.0-only
// ===========================================================================
// CODING AGENT BRIDGE
//
// The hooks of Claude Code, Codex CLI, Cursor and Gemini CLI call
// http://<ip>/agent/<state> on every event, with the hook token in
// X-Hook-Token. /claude/<state> is the first route, kept for the hooks
// already installed; each route takes both name sets:
//
//   state       English     French
//   idle        idle        veille
//   thinking    thinking    reflex
//   tool        tool        outil
//   attention   attention   action
//   error       error       erreur     the turn ended on an error
//   done        done        fini
//   failed      failed      echec      a tool failed, the turn goes on
//   (leaves)    end         fin
//
// Claude Code, for example: UserPromptSubmit and PostToolUse -> thinking,
// PreToolUse -> tool (attention on AskUserQuestion or ExitPlanMode),
// PermissionRequest -> attention, PostToolUseFailure -> failed, StopFailure
// -> error, Stop -> done, SessionStart and Notification (idle) -> idle,
// SessionEnd -> end. The other agents' events map onto the same states.
//
// PostToolUse goes back to thinking: a tool has finished, the agent takes
// over again and thinks. It used to send "tool", and the mascot kept typing
// until the Stop, while the agent spends most of its time thinking between
// two tools.
//
// Several sessions can run at once: each one is identified by the hook's
// session id and named after the last segment of its project folder.
// ===========================================================================
#include "agent_bridge.h"

#include <Preferences.h>

#include "request.h"
#include "settings.h"

const char *const AGENT_STATE_NAMES[AGENT_STATE_COUNT] = {
  "idle", "thinking", "tool", "attention", "error", "done", "failed"
};
const char *const AGENT_STATE_NAMES_FR[AGENT_STATE_COUNT] = {
  "veille", "reflex", "outil", "action", "erreur", "fini", "echec"
};

const uint8_t SESSION_TINTS[TINT_COUNT][3] = {
  {230, 100,  55},   // orange
  { 70, 170, 255},   // blue
  {110, 220, 110},   // green
  {235, 110, 205},   // pink
  {250, 200,  60},   // yellow
  {155, 130, 255},   // purple
};

#define SESSION_EXPIRY_MS (15UL * 60UL * 1000UL)   // a session silent for 15 min leaves
// A ghost has not been seen since the panel restarted. If its Claude Code
// were still running, the first hook would have claimed it: after 2 min it
// is closed, no point showing it for 15 min.
#define GHOST_EXPIRY_MS   (2UL * 60UL * 1000UL)

#define TOOL_MIN_MS           1500UL    // typing stays on screen at least this long
#define WARMUP_AFTER_THINK_MS 3000UL    // the warm-up only follows real thinking
#define MANUAL_PICK_MS        60000UL   // a button pick resists auto focus this long
#define FOCUS_QUIET_MS        10000UL   // auto focus: the shown session must have gone quiet

AgentSession sessions[MAX_SESSIONS];
int      focusedSession    = 0;
uint32_t sessionSwitchedAt = 0;
uint32_t manualPickAt      = 0;

static QueueHandle_t agentQueue = nullptr;

// --- Session table persistence ----------------------------------------------
// Without it, powering the panel off wipes the list: it restarts "looking for
// a connection" and stays there, since no hook fires until the user types
// something. So session identities are kept in NVS and restored as idle at
// boot - the first event refreshes the state.
//
// Written only when the table's makeup changes (session opened, name
// learnt), never on every event: NVS wears out.
// Hooks arrive in bursts: without a guard, a working session would trigger
// one NVS write per event. The flash wears out, and the write blocks the loop
// long enough to bring the web server down.
static uint32_t lastSaveAt  = 0;
static bool     savePending = false;
#define SAVE_MIN_INTERVAL_MS 15000UL

// force: SessionEnd and purge. A throttled write used to be lost, and a
// session closed right after another write came back at the next boot.
// Without force, the throttled write is picked up by updateSessions().
static void saveSessions(bool force = false) {
  if (!force && lastSaveAt && millis() - lastSaveAt < SAVE_MIN_INTERVAL_MS) {
    savePending = true;
    return;
  }
  lastSaveAt  = millis();
  savePending = false;

  String s;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!sessions[i].active) continue;
    s += String(sessions[i].id) + "|" + sessions[i].name + "|" +
         String(sessions[i].tint) + ";";
  }
  // Local instance: the web page saves the settings from the other core,
  // and a Preferences object cannot be shared between tasks.
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putString("sess", s);
  p.end();
}

static void loadSessions() {
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  String s = p.getString("sess", "");
  p.end();
  if (!s.length()) return;

  int d = 0, n = 0;
  while (d < (int)s.length() && n < MAX_SESSIONS) {
    const int end = s.indexOf(';', d);
    if (end < 0) break;
    String e = s.substring(d, end);
    d = end + 1;

    const int a = e.indexOf('|');
    const int b = e.indexOf('|', a + 1);
    if (a < 0 || b < 0) continue;

    strncpy(sessions[n].id,   e.substring(0, a).c_str(),     sizeof(sessions[n].id) - 1);
    strncpy(sessions[n].name, e.substring(a + 1, b).c_str(), sizeof(sessions[n].name) - 1);
    sessions[n].id[sizeof(sessions[n].id) - 1]     = '\0';
    sessions[n].name[sizeof(sessions[n].name) - 1] = '\0';
    sessions[n].tint      = (uint8_t)e.substring(b + 1).toInt();
    sessions[n].state     = AG_IDLE;      // state unknown: back to idle
    sessions[n].lastSeen  = millis();     // they get the expiry delay
    sessions[n].enteredAt = millis();
    sessions[n].active    = true;
    sessions[n].ghost     = true;         // waiting to be recognised
    n++;
  }
  if (n) Serial.printf("[AGENT] %d session(s) restored from NVS\n", n);
}

// Automatic fallbacks: without a new event, a state gives way to the next.
// Otherwise the last state received loops forever: the celebration never
// ends, and an error stays on screen after it is fixed.
// One-off animations (celebration, startle, KO) also end here: the state
// machine lives in updateSessions(), never in the rendering, so that what is
// shown does not change what is true.
// Indexed by AgentState, delay 0 = no automatic return.
struct Fallback { uint32_t delay; AgentState next; };
static const Fallback FALLBACK[] = {
  {0,            AG_IDLE},       // AG_IDLE: already resting
  // Escape sends no Stop: without a fallback, the mascot would work forever.
  // But 90 s was too short: a long build, or an upload waiting for BOOT/EN,
  // put him to sleep while the agent was still working.
  // The idle_prompt Notification brings him back to idle well before 10 min.
  {600000UL,     AG_IDLE},       // AG_THINKING
  {600000UL,     AG_IDLE},       // AG_TOOL
  {600000UL,     AG_IDLE},       // AG_ATTENTION: he is really waiting for you
  {KO_MS,        AG_IDLE},       // AG_ERROR: the turn is over, he does not resume alone
  {CELEBRATE_MS, AG_IDLE},       // AG_DONE: once celebrated, back to rest
  {STARTLE_MS,   AG_THINKING},   // AG_FAILED: the agent reads the error and fixes it
};
static_assert(sizeof(FALLBACK) / sizeof(FALLBACK[0]) == AGENT_STATE_COUNT, "one delay per state");
static_assert(sizeof(AGENT_STATE_NAMES_FR) / sizeof(AGENT_STATE_NAMES_FR[0]) == AGENT_STATE_COUNT, "one name per state");
static_assert(sizeof(AGENT_STATE_NAMES) / sizeof(AGENT_STATE_NAMES[0]) == AGENT_STATE_COUNT, "one name per state");

// Expiries, fallbacks, deferred NVS write. Called by loop() on every frame:
// only the display loop modifies the table.
void updateSessions() {
  const uint32_t now = millis();
  for (int i = 0; i < MAX_SESSIONS; i++) {
    AgentSession &s = sessions[i];
    if (!s.active) continue;

    const uint32_t age = now - s.lastSeen;
    if (age > (s.ghost ? GHOST_EXPIRY_MS : SESSION_EXPIRY_MS)) {
      s.active = false;                  // the id stays: if it comes back, it gets its colour back
      saveSessions();
      continue;
    }

    // `lastSeen` is not touched: the long expiry keeps running
    const Fallback &f = FALLBACK[s.state];
    if (f.delay && age > f.delay) {
      s.state     = f.next;
      s.enteredAt = now;
    }
  }
  if (savePending && now - lastSaveAt >= SAVE_MIN_INTERVAL_MS) saveSessions();
}

int activeSessionCount() {
  int n = 0;
  for (int i = 0; i < MAX_SESSIONS; i++)
    if (sessions[i].active) n++;
  return n;
}

// State to show, debounce included. Read, Grep or Edit last a few
// milliseconds: without a minimum duration, PreToolUse then PostToolUse would
// make the panel flicker between typing and thinking on every tool.
AgentState displayedState(int i, uint32_t now) {
  const AgentSession &s = sessions[i];
  if (s.state == AG_THINKING && s.lastToolAt && now - s.lastToolAt < TOOL_MIN_MS) return AG_TOOL;
  return s.state;
}

int nthActiveSession(int rank) {
  int n = 0;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!sessions[i].active) continue;
    if (n == rank) return i;
    n++;
  }
  return -1;
}

void projectName(const char *path, char *out, size_t size) {
  const char *d = path;
  for (const char *p = path; *p; p++)
    if (*p == '/' || *p == '\\') d = p + 1;
  strncpy(out, *d ? d : "session", size - 1);
  out[size - 1] = '\0';
}

static bool tintTaken(uint8_t t, int except) {
  for (int i = 0; i < MAX_SESSIONS; i++)
    if (i != except && sessions[i].active && sessions[i].tint == t) return true;
  return false;
}

// First tint no other active session wears. The tint used to depend on the
// slot: two sessions could share the same one.
static uint8_t freeTint(int except) {
  for (uint8_t t = 0; t < TINT_COUNT; t++)
    if (!tintTaken(t, except)) return t;
  return except % TINT_COUNT;
}

// Resets a slot. Without it, a recycled slot kept the name, tint and ghost
// flag of its previous occupant.
static void openSession(int i, const char *id) {
  sessions[i] = AgentSession();
  strncpy(sessions[i].id, id, sizeof(sessions[i].id) - 1);
  sessions[i].tint   = freeTint(i);
  sessions[i].active = true;
}

// Finds the session, or opens one. `isNew` says whether the table's makeup
// changed, the only case where NVS is written.
//
// An identity already seen gets its slot back even if it expired in the
// meantime: it keeps its colour instead of changing it after 15 min of silence.
//
// The project name is a second criterion: a session restored from NVS keeps
// the old session_id, which is worthless once Claude Code restarts. The first
// session with the same name therefore gets its slot - and with it its
// colour - back instead of creating a duplicate.
static int findSession(const char *id, const char *name, bool &isNew) {
  isNew = false;

  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (strcmp(sessions[i].id, id) != 0) continue;
    if (!sessions[i].active) {
      sessions[i].active    = true;
      sessions[i].state     = AG_IDLE;
      sessions[i].enteredAt = millis();
      // It keeps its tint, unless another session took it in the meantime
      if (tintTaken(sessions[i].tint, i)) sessions[i].tint = freeTint(i);
      isNew = true;
    }
    sessions[i].ghost = false;
    return i;
  }

  if (name[0]) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
      if (!sessions[i].active || !sessions[i].ghost) continue;
      if (strcmp(sessions[i].name, name) != 0) continue;
      strncpy(sessions[i].id, id, sizeof(sessions[i].id) - 1);
      sessions[i].id[sizeof(sessions[i].id) - 1] = '\0';
      sessions[i].ghost = false;
      isNew = true;                          // the id changed: to be rewritten
      Serial.printf("[AGENT] %s takes its slot back (tint %u)\n", name, sessions[i].tint);
      return i;
    }
  }

  // Never-used slot first, to keep the memory of expired sessions as long as
  // possible; otherwise the oldest free one; otherwise, table full, recycle
  // the oldest.
  int slot = -1;
  for (int i = 0; i < MAX_SESSIONS && slot < 0; i++)
    if (!sessions[i].active && !sessions[i].id[0]) slot = i;
  if (slot < 0)
    for (int i = 0; i < MAX_SESSIONS; i++)
      if (!sessions[i].active && (slot < 0 || sessions[i].lastSeen < sessions[slot].lastSeen)) slot = i;
  if (slot < 0) {
    slot = 0;
    for (int i = 1; i < MAX_SESSIONS; i++)
      if (sessions[i].lastSeen < sessions[slot].lastSeen) slot = i;
  }
  openSession(slot, id);
  isNew = true;
  return slot;
}

// Empties the table and NVS: useful after tests, or when a session was
// killed without sending SessionEnd.
static void clearSessions() {
  for (int i = 0; i < MAX_SESSIONS; i++) sessions[i] = AgentSession();
  focusedSession = 0;
  saveSessions(true);
  Serial.println("[AGENT] session table cleared");
}

// Auto focus: the panel follows the session that is working. It used to move
// only on an attention request: with the terminal and VS Code both open, it
// stayed on the one sleeping. The screen is not taken away from a session
// calling you, from a button pick, nor from a session still busy - otherwise
// two active Claude Codes would flip it on every tool.
static void autoFocus(int i, uint32_t now) {
  if (focusedSession == i) return;
  if (manualPickAt && now - manualPickAt < MANUAL_PICK_MS) return;
  if (focusedSession >= 0 && focusedSession < MAX_SESSIONS && sessions[focusedSession].active) {
    const AgentSession &f = sessions[focusedSession];
    if (f.state == AG_ATTENTION || f.state == AG_ERROR) return;
    if (f.state != AG_IDLE && now - f.lastSeen < FOCUS_QUIET_MS) return;
  }
  focusedSession    = i;
  sessionSwitchedAt = now;
}

// Applies an event received by the web server. Called from loop(): with
// updateSessions(), the only place that modifies the table.
void applyAgentEvent(const AgentEvent &ev) {
  if (ev.state == EV_PURGE) { clearSessions(); return; }

  // The hook script sends its request in the background and resends it if it
  // gets lost: on a Wi-Fi that drops packets, an event can duplicate another.
  // Hooks run one after the other, so their timestamps strictly increase: an
  // event no newer than the last one applied is stale (or a duplicate) and
  // must not take the mascot backwards. Checked before findSession, so that a
  // straggler does not reopen a session already closed.
  if (ev.t) {
    for (int k = 0; k < MAX_SESSIONS; k++) {
      if (strcmp(sessions[k].id, ev.id) != 0) continue;
      if (ev.t <= sessions[k].clientTime) {
        Serial.printf("[AGENT] %s  %s stale, ignored\n", sessions[k].name,
                      ev.state == EV_END ? "fin" : AGENT_STATE_NAMES_FR[ev.state]);
        return;
      }
      break;
    }
  }

  // SessionEnd closes the session instead of putting it to sleep. "fin" used
  // to be rejected as an unknown state: a closed session stayed for 15 min.
  // An unknown identity does not open a slot just to close it.
  if (ev.state == EV_END) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
      if (!sessions[i].active || strcmp(sessions[i].id, ev.id) != 0) continue;
      sessions[i].active = false;
      if (ev.t) sessions[i].clientTime = ev.t;
      saveSessions(true);
      Serial.printf("[AGENT] %s  fin  (%s)\n", sessions[i].name, ev.id);
    }
    return;
  }

  bool isNew;
  const int i = findSession(ev.id, ev.name, isNew);
  AgentSession &s = sessions[i];
  const uint32_t now = millis();

  bool changed = isNew;
  if (ev.name[0] && strcmp(s.name, ev.name) != 0) {
    strncpy(s.name, ev.name, sizeof(s.name) - 1);
    s.name[sizeof(s.name) - 1] = '\0';
    changed = true;
  } else if (!s.name[0]) {
    strncpy(s.name, "session", sizeof(s.name) - 1);
    changed = true;
  }
  if (changed) saveSessions();

  if (ev.t) s.clientTime = ev.t;

  const AgentState state  = (AgentState)ev.state;
  const AgentState before = s.state;

  // The mascot cracks its knuckles before coding, but only after real thinking:
  // between two quick tools, it goes straight back to typing.
  if (state == AG_TOOL && before == AG_THINKING && now - s.enteredAt > WARMUP_AFTER_THINK_MS)
    s.warmupAt = now;
  if (state == AG_TOOL) s.lastToolAt = now;

  // One-off states replay their animation on every event; the others keep
  // their entry time for as long as they last.
  const bool oneOff = (state == AG_DONE || state == AG_ERROR || state == AG_FAILED);
  if (state != before || oneOff) s.enteredAt = now;
  // The debounce only applies to tool -> thinking. Without this reset, after
  // the startle of a failure, the mascot went back to typing for a moment before
  // thinking.
  if (oneOff || state == AG_IDLE) s.lastToolAt = 0;
  s.state    = state;
  s.lastSeen = now;

  // An attention request takes the focus: that is what must be seen.
  // A tool failure is not one: the agent fixes it on its own.
  if (state == AG_ATTENTION || state == AG_ERROR) {
    if (focusedSession != i) { focusedSession = i; sessionSwitchedAt = now; }
  } else if (state != AG_IDLE) {
    autoFocus(i, now);
  }
  // A session that starts waiting for the owner is a request of its own:
  // CODE comes on stage, as the owner picked on Panel
  if (state == AG_ATTENTION && before != AG_ATTENTION)
    requestShown(REQUEST_AGENT, SOURCE_AGENT, s.name);

  Serial.printf("[AGENT] %s  %s  (%s)\n", s.name, AGENT_STATE_NAMES_FR[state], ev.id);
}

int8_t parseAgentState(const char *name) {
  if (!strcmp(name, "end") || !strcmp(name, "fin")) return EV_END;
  for (uint8_t i = 0; i < AGENT_STATE_COUNT; i++)
    if (!strcmp(name, AGENT_STATE_NAMES[i]) || !strcmp(name, AGENT_STATE_NAMES_FR[i]))
      return (int8_t)i;
  return EV_UNKNOWN;
}

void agentBridgeBegin() {
  loadSessions();        // the list survives a panel power cut
  // 32 slots cover a burst of tools run in parallel.
  agentQueue = xQueueCreate(32, sizeof(AgentEvent));
}

bool postAgentEvent(const AgentEvent &ev) {
  return agentQueue && xQueueSend(agentQueue, &ev, 0) == pdTRUE;
}

// Drains the web server's queue. Non-blocking: nothing pending, carry on.
void processAgentEvents() {
  AgentEvent ev;
  while (agentQueue && xQueueReceive(agentQueue, &ev, 0) == pdTRUE) applyAgentEvent(ev);
}
