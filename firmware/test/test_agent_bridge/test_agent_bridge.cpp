// SPDX-License-Identifier: AGPL-3.0-only
// Host-side checks of the Claude Code bridge logic: states, fallbacks,
// debounce, focus, sessions, NVS, payload parsing.
//
// The real module sources are compiled here against the stubs in test/stubs:
// the code under test is the firmware's, not a copy.
//
//   pio test -e native
#include <unity.h>

#include "../../src/agent_bridge.cpp"
#include "../../src/json_lite.cpp"

// What the bridge tells the panel when a session starts needing the owner
static int agentRequests = 0;
static char agentRequestText[32] = "";
void requestShown(RequestKind kind, RequestSource from, const char *text, uint16_t, uint32_t) {
  if (kind != REQUEST_AGENT || from != SOURCE_AGENT) return;
  agentRequests++;
  strncpy(agentRequestText, text, sizeof(agentRequestText) - 1);
}

// --- Helpers ----------------------------------------------------------------
static AgentEvent event(const char *state, const char *id, const char *name = "code teste",
                         uint64_t t = 0) {
  AgentEvent e = {};
  e.state = !strcmp(state, "purge") ? EV_PURGE : parseAgentState(state);
  e.t = t;
  strncpy(e.id, id, sizeof(e.id) - 1);
  strncpy(e.name, name, sizeof(e.name) - 1);
  return e;
}

// A hook arriving, as loop() would apply it when draining the queue
static void hook(const char *state, const char *id, const char *name = "code teste") {
  applyAgentEvent(event(state, id, name));
}

// A hook from the script, timestamped by the PC (microseconds)
static void hookT(const char *state, const char *id, uint64_t t) {
  applyAgentEvent(event(state, id, "code teste", t));
}

// Time passes, one frame every 18 ms, as in loop()
static void wait(uint32_t ms) {
  const uint32_t end = fakeClock + ms;
  while (fakeClock < end) {
    fakeClock += std::min<uint32_t>(18, end - fakeClock);
    updateSessions();
  }
}

static int session(const char *id) {
  for (int i = 0; i < MAX_SESSIONS; i++)
    if (sessions[i].active && !strcmp(sessions[i].id, id)) return i;
  return -1;
}

// What the panel shows for this session
static AgentState shown(const char *id) {
  const int i = session(id);
  return i < 0 ? AG_IDLE : displayedState(i, millis());
}

static bool nvsContains(const char *text) {
  return strstr(fakeNvs["sess"].c_str(), text) != nullptr;
}

void setUp() {
  for (int i = 0; i < MAX_SESSIONS; i++) sessions[i] = AgentSession();
  focusedSession = 0;
  sessionSwitchedAt = 0;
  manualPickAt = 0;
  lastSaveAt = 0;
  savePending = false;
  agentQueue = nullptr;
  fakeNvs.clear();
  nvsWriteCount = 0;
  fakeClock = 100000;
  agentRequests = 0;
  agentRequestText[0] = '\0';
}

void tearDown() {}

// --- Scenarios --------------------------------------------------------------

// Once when a session starts waiting, not on every repeat while it waits
static void test_waiting_session_asks_for_the_owner_once() {
  hook("outil", "A", "carousel");
  TEST_ASSERT_EQUAL(0, agentRequests);
  hook("action", "A", "carousel");
  TEST_ASSERT_EQUAL(1, agentRequests);
  TEST_ASSERT_EQUAL_STRING("carousel", agentRequestText);
  hook("action", "A", "carousel");
  TEST_ASSERT_EQUAL(1, agentRequests);
  hook("outil", "A", "carousel");
  hook("action", "A", "carousel");
  TEST_ASSERT_EQUAL(2, agentRequests);
}
static void test_simple_turn() {
  // One turn: thinking, quick tool, thinking, Stop
  hook("veille", "A");                              // SessionStart
  TEST_ASSERT_EQUAL(1, activeSessionCount());
  TEST_ASSERT_EQUAL(AG_IDLE, shown("A"));

  hook("reflex", "A");                              // UserPromptSubmit
  wait(4000);
  TEST_ASSERT_EQUAL(AG_THINKING, shown("A"));

  hook("outil", "A");                               // PreToolUse (Read)
  TEST_ASSERT_EQUAL(millis(), sessions[session("A")].warmupAt);   // 4 s of thinking
  wait(40);
  hook("reflex", "A");                              // PostToolUse, 40 ms later
  TEST_ASSERT_EQUAL(AG_TOOL, shown("A"));           // debounce: typing holds
  wait(1000);
  TEST_ASSERT_EQUAL(AG_TOOL, shown("A"));
  wait(600);                                        // 1.64 s after PreToolUse
  TEST_ASSERT_EQUAL(AG_THINKING, shown("A"));

  hook("fini", "A");                                // Stop
  TEST_ASSERT_EQUAL(AG_DONE, shown("A"));
  wait(1400);
  TEST_ASSERT_EQUAL(AG_DONE, shown("A"));
  wait(200);
  TEST_ASSERT_EQUAL(AG_IDLE, shown("A"));
}

static void test_warmup_only_after_real_thinking() {
  hook("reflex", "A");
  wait(1000);
  hook("outil", "A");
  TEST_ASSERT_EQUAL(0, sessions[session("A")].warmupAt);           // 1 s: no
  wait(100);
  hook("reflex", "A");
  wait(3500);
  hook("outil", "A");
  TEST_ASSERT_EQUAL(millis(), sessions[session("A")].warmupAt);    // 3.5 s: yes
}

static void test_tool_failure_startles_without_stealing_focus() {
  hook("outil", "A");                               // A on screen
  hook("reflex", "B");
  wait(500);
  hook("outil", "B");
  wait(30);
  hook("echec", "B");                               // PostToolUseFailure
  TEST_ASSERT_EQUAL(session("A"), focusedSession);
  TEST_ASSERT_EQUAL(AG_FAILED, shown("B"));
  wait(700);
  TEST_ASSERT_EQUAL(AG_FAILED, shown("B"));
  wait(200);
  TEST_ASSERT_EQUAL(AG_THINKING, shown("B"));       // and no sneaky typing
}

static void test_stop_failure_takes_screen_then_idles() {
  hook("outil", "A");
  hook("outil", "B");
  TEST_ASSERT_EQUAL(session("A"), focusedSession);
  hook("erreur", "B");
  TEST_ASSERT_EQUAL(session("B"), focusedSession);
  TEST_ASSERT_EQUAL(millis(), sessionSwitchedAt);
  wait(9900);
  TEST_ASSERT_EQUAL(AG_ERROR, shown("B"));
  wait(200);
  TEST_ASSERT_EQUAL(AG_IDLE, shown("B"));
}

static void test_waiting_session_keeps_screen() {
  hook("action", "A");
  wait(30000);
  hook("outil", "B");
  TEST_ASSERT_EQUAL(session("A"), focusedSession);
  TEST_ASSERT_EQUAL(AG_ATTENTION, shown("A"));

  setUp();
  hook("veille", "A");
  hook("veille", "B");
  focusedSession = session("A");
  manualPickAt = millis();                          // button pick
  hook("action", "B");                              // goes through anyway
  TEST_ASSERT_EQUAL(session("B"), focusedSession);
}

static void test_auto_focus_follows_working_session() {
  hook("reflex", "A");
  hook("reflex", "B");
  TEST_ASSERT_EQUAL(session("A"), focusedSession);  // A busy
  wait(5000);
  hook("outil", "B");
  TEST_ASSERT_EQUAL(session("A"), focusedSession);  // A quiet for only 5 s
  wait(6000);
  hook("outil", "B");
  TEST_ASSERT_EQUAL(session("B"), focusedSession);  // A quiet for 11 s
  TEST_ASSERT_EQUAL(millis(), sessionSwitchedAt);
  hook("outil", "A");
  TEST_ASSERT_EQUAL(session("B"), focusedSession);  // B busy in turn

  setUp();
  hook("fini", "A");
  wait(2000);                                       // A's celebration is over
  hook("reflex", "B");
  TEST_ASSERT_EQUAL(session("B"), focusedSession);  // A resting: B takes the screen
}

static void test_button_pick_holds_60s() {
  hook("veille", "A");
  hook("veille", "B");
  TEST_ASSERT_EQUAL(session("A"), focusedSession);
  focusedSession = session("B");
  manualPickAt = millis();
  wait(1000);
  hook("reflex", "A");
  TEST_ASSERT_EQUAL(session("B"), focusedSession);
  wait(60000);
  hook("reflex", "A");
  TEST_ASSERT_EQUAL(session("A"), focusedSession);
}

static void test_session_end_closes_and_saves_now() {
  hook("reflex", "A");
  hook("reflex", "B");                              // throttled write: pending
  const int before = nvsWriteCount;
  hook("fin", "A");
  TEST_ASSERT_TRUE(session("A") < 0);
  TEST_ASSERT_EQUAL(1, activeSessionCount());
  TEST_ASSERT_EQUAL(before + 1, nvsWriteCount);     // forced despite the throttle
  TEST_ASSERT_FALSE(nvsContains("A|"));
  TEST_ASSERT_TRUE(nvsContains("B|"));
  hook("fin", "Z");                                 // unknown: no effect
  TEST_ASSERT_EQUAL(1, activeSessionCount());
}

static void test_expiry_and_return_with_same_colour() {
  hook("reflex", "B0");                             // takes tint 0
  hook("reflex", "A");
  const uint8_t tint = sessions[session("A")].tint;
  wait(SESSION_EXPIRY_MS + 1000);
  TEST_ASSERT_TRUE(session("A") < 0);
  TEST_ASSERT_EQUAL(0, activeSessionCount());
  hook("reflex", "A");
  TEST_ASSERT_TRUE(session("A") >= 0);
  TEST_ASSERT_EQUAL(tint, sessions[session("A")].tint);

  setUp();
  hook("reflex", "A");
  wait(SESSION_EXPIRY_MS + 1000);
  hook("reflex", "B");                              // takes the freed tint
  hook("reflex", "A");                              // A changes rather than duplicate
  TEST_ASSERT_NOT_EQUAL(sessions[session("B")].tint, sessions[session("A")].tint);
}

static bool tintsDistinct() {
  for (int i = 0; i < MAX_SESSIONS; i++)
    for (int j = i + 1; j < MAX_SESSIONS; j++)
      if (sessions[i].tint == sessions[j].tint) return false;
  return true;
}

static void test_full_table_recycles_oldest() {
  const char *ids[] = {"S1", "S2", "S3", "S4", "S5", "S6"};
  for (const char *id : ids) { hook("reflex", id); wait(20); }
  TEST_ASSERT_EQUAL(6, activeSessionCount());
  TEST_ASSERT_TRUE(tintsDistinct());

  hook("reflex", "S7");
  TEST_ASSERT_TRUE(session("S1") < 0);
  TEST_ASSERT_TRUE(session("S7") >= 0);
  TEST_ASSERT_EQUAL(6, activeSessionCount());
  TEST_ASSERT_TRUE(tintsDistinct());
}

static void test_nvs_ghosts_claimed_by_name_or_expire() {
  fakeNvs["sess"] = "old1|code teste|2;old2|fitbuddy|4;";
  loadSessions();
  TEST_ASSERT_EQUAL(2, activeSessionCount());

  hook("reflex", "new", "code teste");              // Claude Code restarted
  const int i = session("new");
  TEST_ASSERT_TRUE(i >= 0);
  TEST_ASSERT_EQUAL(2, sessions[i].tint);
  TEST_ASSERT_FALSE(sessions[i].ghost);
  TEST_ASSERT_EQUAL(2, activeSessionCount());       // no duplicate
  TEST_ASSERT_TRUE(nvsContains("new|code teste|2;"));

  wait(GHOST_EXPIRY_MS + 1000);
  TEST_ASSERT_EQUAL(1, activeSessionCount());       // fitbuddy, never seen again
  TEST_ASSERT_TRUE(session("new") >= 0);
  TEST_ASSERT_FALSE(nvsContains("fitbuddy"));

  setUp();                                          // same identity: direct resume
  fakeNvs["sess"] = "still|code teste|3;";
  loadSessions();
  hook("outil", "still");
  TEST_ASSERT_EQUAL(1, activeSessionCount());
  TEST_ASSERT_EQUAL(3, sessions[session("still")].tint);
}

static void test_nvs_write_throttled_then_caught_up() {
  hook("reflex", "A");
  const int n = nvsWriteCount;
  TEST_ASSERT_EQUAL(1, n);
  wait(1000);
  hook("reflex", "B");
  TEST_ASSERT_EQUAL(n, nvsWriteCount);
  TEST_ASSERT_TRUE(savePending);
  wait(15000);
  TEST_ASSERT_EQUAL(n + 1, nvsWriteCount);
  TEST_ASSERT_TRUE(nvsContains("B|"));
  wait(60000);
  TEST_ASSERT_EQUAL(n + 1, nvsWriteCount);          // nothing more without a change
}

static void test_purge_from_web_page() {
  hook("reflex", "A");
  hook("reflex", "B");
  applyAgentEvent(event("purge", ""));
  TEST_ASSERT_EQUAL(0, activeSessionCount());
  TEST_ASSERT_TRUE(fakeNvs["sess"].empty());
  hook("reflex", "A");
  TEST_ASSERT_EQUAL(1, activeSessionCount());
}

static void test_long_thinking_does_not_fall_asleep() {
  // 9 min of thinking or building: he stays awake
  hook("reflex", "A");
  wait(9 * 60000UL);
  TEST_ASSERT_EQUAL(AG_THINKING, shown("A"));
  wait(61000);
  TEST_ASSERT_EQUAL(AG_IDLE, shown("A"));           // safety net if Escape without idle_prompt

  setUp();
  hook("outil", "A");
  wait(30000);
  hook("veille", "A");                              // idle_prompt Notification
  TEST_ASSERT_EQUAL(AG_IDLE, shown("A"));
}

static void test_queue_applies_events_in_order() {
  agentQueue = xQueueCreate(32, sizeof(AgentEvent));
  const AgentEvent e[] = {event("reflex", "A"), event("outil", "A"), event("action", "A")};
  for (const auto &x : e) TEST_ASSERT_TRUE(postAgentEvent(x));
  processAgentEvents();
  TEST_ASSERT_EQUAL(AG_ATTENTION, shown("A"));
  AgentEvent rest;
  TEST_ASSERT_NOT_EQUAL(pdTRUE, xQueueReceive(agentQueue, &rest, 0));
  delete agentQueue;
  agentQueue = nullptr;
}

static void test_late_or_duplicate_events_ignored() {
  const uint64_t t0 = 1790891813000000ULL;
  hookT("reflex", "A", t0);
  hookT("outil", "A", t0 + 300000);               // PreToolUse, received first...
  hookT("reflex", "A", t0 + 200000);              // ...its predecessor arrives after
  TEST_ASSERT_EQUAL(AG_TOOL, sessions[session("A")].state);
  hookT("outil", "A", t0 + 300000);               // same hook resent: duplicate
  TEST_ASSERT_TRUE(sessions[session("A")].clientTime == t0 + 300000);
  hookT("reflex", "A", t0 + 400000);              // the normal sequence goes through
  TEST_ASSERT_EQUAL(AG_THINKING, sessions[session("A")].state);

  hookT("fin", "A", t0 + 500000);
  TEST_ASSERT_TRUE(session("A") < 0);
  hookT("outil", "A", t0 + 450000);               // straggler: does not reopen
  TEST_ASSERT_TRUE(session("A") < 0);
  TEST_ASSERT_EQUAL(0, activeSessionCount());
  hookT("veille", "A", t0 + 600000);              // the session resumes (claude --resume)
  TEST_ASSERT_TRUE(session("A") >= 0);

  hook("outil", "B");                             // no timestamp: always applied
  hook("reflex", "B");
  TEST_ASSERT_EQUAL(AG_THINKING, sessions[session("B")].state);
}

static void test_payload_parsing() {
  char id[20] = "", cwd[160] = "", name[14] = "";
  // As sent by Claude Code on Windows: backslashes are doubled in JSON
  const char *body =
    "{\"session_id\":\"e4747970-47c1-4cc0-a288-c17da42dcdda\","
    "\"prompt_id\":\"550e8400-e29b-41d4-a716-446655440000\","
    "\"transcript_path\":\"C:\\\\Users\\\\user\\\\.claude\\\\x.jsonl\","
    "\"cwd\":\"c:\\\\Users\\\\user\\\\Desktop\\\\code teste\","
    "\"hook_event_name\":\"PreToolUse\",\"tool_name\":\"Bash\","
    "\"tool_input\":{\"command\":\"ls\",\"cwd\":\"trap\"}}";
  TEST_ASSERT_TRUE(extractJsonString(body, "\"session_id\"", id, sizeof(id)));
  TEST_ASSERT_EQUAL_STRING("e4747970-47c1-4cc0-", id);
  TEST_ASSERT_TRUE(extractJsonString(body, "\"cwd\"", cwd, sizeof(cwd)));
  projectName(cwd, name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("code teste", name);

  projectName("/home/me/my-very-long-project", name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("my-very-long-", name);  // truncated to 13 characters
  projectName("C:\\\\", name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("session", name);
  TEST_ASSERT_FALSE(extractJsonString("{\"other\":1}", "\"session_id\"", id, sizeof(id)));
  TEST_ASSERT_FALSE(extractJsonString("{\"session_id\":\"abc", "\"session_id\"", id, sizeof(id)));
}

static void test_state_names_round_trip() {
  for (uint8_t i = 0; i < AGENT_STATE_COUNT; i++)
    TEST_ASSERT_EQUAL(i, parseAgentState(AGENT_STATE_NAMES_FR[i]));
  TEST_ASSERT_EQUAL(EV_END, parseAgentState("fin"));
  TEST_ASSERT_EQUAL(EV_UNKNOWN, parseAgentState("nope"));
}

// /agent speaks English: each name has exactly the effect of the French name
// in the same place, on either route
static void test_english_state_names_match_french() {
  static const char *const EN[] = {"idle", "thinking", "tool", "attention", "error", "done",
                                   "failed"};
  static_assert(sizeof(EN) / sizeof(EN[0]) == AGENT_STATE_COUNT, "one name per state");
  for (uint8_t i = 0; i < AGENT_STATE_COUNT; i++) {
    TEST_ASSERT_EQUAL_STRING(EN[i], AGENT_STATE_NAMES[i]);
    TEST_ASSERT_EQUAL(i, parseAgentState(AGENT_STATE_NAMES[i]));
    TEST_ASSERT_EQUAL(parseAgentState(AGENT_STATE_NAMES_FR[i]), parseAgentState(EN[i]));
  }
  // Applied the same: the English attention brings CODE on stage too
  hook("thinking", "en", "app");
  TEST_ASSERT_EQUAL(AG_THINKING, sessions[session("en")].state);
  hook("attention", "en", "app");
  TEST_ASSERT_EQUAL(AG_ATTENTION, sessions[session("en")].state);
  TEST_ASSERT_EQUAL(1, agentRequests);
}

static void test_end_and_fin_close_a_session() {
  TEST_ASSERT_EQUAL(EV_END, parseAgentState("end"));
  TEST_ASSERT_EQUAL(EV_END, parseAgentState("fin"));
  hook("idle", "a", "one");
  hook("veille", "b", "two");
  TEST_ASSERT_EQUAL(2, activeSessionCount());
  hook("end", "a", "one");
  TEST_ASSERT_EQUAL(1, activeSessionCount());
  hook("fin", "b", "two");
  TEST_ASSERT_EQUAL(0, activeSessionCount());
}

// Installed hooks and scripts speak French: those names stay for good
static void test_french_names_still_parse() {
  static const char *const FR[] = {"veille", "reflex", "outil", "action", "erreur", "fini",
                                   "echec"};
  for (uint8_t i = 0; i < AGENT_STATE_COUNT; i++) {
    TEST_ASSERT_EQUAL_STRING(FR[i], AGENT_STATE_NAMES_FR[i]);
    TEST_ASSERT_EQUAL(i, parseAgentState(FR[i]));
  }
}

static void test_unknown_name_still_unknown() {
  for (const char *n : {"", "nope", "Idle", "IDLE", "ending", "en", "idle ", "fini2", "purge"})
    TEST_ASSERT_EQUAL_MESSAGE(EV_UNKNOWN, parseAgentState(n), n);
}

static void test_json_number() {
  float v = 0;
  TEST_ASSERT_TRUE(extractJsonNumber(String("{\"temperature_2m\":[12.5]}"), "\"temperature_2m\"", v));
  TEST_ASSERT_EQUAL_FLOAT(12.5f, v);
  TEST_ASSERT_TRUE(extractJsonNumber(String("{\"subscriberCount\": \"12345\"}"), "\"subscriberCount\"", v));
  TEST_ASSERT_EQUAL_FLOAT(12345.0f, v);
  TEST_ASSERT_FALSE(extractJsonNumber(String("{\"x\":1}"), "\"y\"", v));
}

// Open-Meteo puts "<key>_units" objects, with the same keys holding strings,
// before the values themselves.
static void test_json_number_skips_unit_strings() {
  const String body =
    "{\"current_units\":{\"time\":\"iso8601\",\"temperature_2m\":\"\xC2\xB0" "C\","
    "\"weather_code\":\"wmo code\"},"
    "\"current\":{\"time\":\"2026-10-09T18:45\",\"interval\":900,"
    "\"temperature_2m\":15.9,\"weather_code\":3},"
    "\"daily_units\":{\"temperature_2m_max\":\"\xC2\xB0" "C\",\"temperature_2m_min\":\"\xC2\xB0" "C\"},"
    "\"daily\":{\"time\":[\"2026-10-09\"],\"temperature_2m_max\":[16.7],"
    "\"temperature_2m_min\":[-9.8]}}";
  float v = 0;
  TEST_ASSERT_TRUE(extractJsonNumber(body, "\"temperature_2m\"", v));
  TEST_ASSERT_EQUAL_FLOAT(15.9f, v);
  TEST_ASSERT_TRUE(extractJsonNumber(body, "\"weather_code\"", v));
  TEST_ASSERT_EQUAL_FLOAT(3.0f, v);
  TEST_ASSERT_TRUE(extractJsonNumber(body, "\"temperature_2m_max\"", v));
  TEST_ASSERT_EQUAL_FLOAT(16.7f, v);
  TEST_ASSERT_TRUE(extractJsonNumber(body, "\"temperature_2m_min\"", v));
  TEST_ASSERT_EQUAL_FLOAT(-9.8f, v);
}

static void test_json_value() {
  char v[32];
  const char *body = "{\"text\":\"Hi \\\"you\\\" caf\\u00e9\", \"seconds\" : 5, \"on\":true,\"o\":{}}";
  TEST_ASSERT_TRUE(jsonValue(body, "text", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("Hi \"you\" caf\xC3\xA9", v);
  TEST_ASSERT_TRUE(jsonValue(body, "seconds", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("5", v);
  TEST_ASSERT_TRUE(jsonValue(body, "on", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("true", v);
  TEST_ASSERT_FALSE(jsonValue(body, "o", v, sizeof(v)));          // object
  TEST_ASSERT_FALSE(jsonValue(body, "missing", v, sizeof(v)));
  // A value equal to a key name is not taken for the key
  TEST_ASSERT_TRUE(jsonValue("{\"a\":\"level\",\"level\":3}", "level", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("3", v);
  // Truncated, still terminated
  TEST_ASSERT_TRUE(jsonValue("{\"t\":\"abcdefgh\"}", "t", v, 4));
  TEST_ASSERT_EQUAL_STRING("abc", v);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple_turn);
  RUN_TEST(test_warmup_only_after_real_thinking);
  RUN_TEST(test_tool_failure_startles_without_stealing_focus);
  RUN_TEST(test_stop_failure_takes_screen_then_idles);
  RUN_TEST(test_waiting_session_keeps_screen);
  RUN_TEST(test_waiting_session_asks_for_the_owner_once);
  RUN_TEST(test_auto_focus_follows_working_session);
  RUN_TEST(test_button_pick_holds_60s);
  RUN_TEST(test_session_end_closes_and_saves_now);
  RUN_TEST(test_expiry_and_return_with_same_colour);
  RUN_TEST(test_full_table_recycles_oldest);
  RUN_TEST(test_nvs_ghosts_claimed_by_name_or_expire);
  RUN_TEST(test_nvs_write_throttled_then_caught_up);
  RUN_TEST(test_purge_from_web_page);
  RUN_TEST(test_long_thinking_does_not_fall_asleep);
  RUN_TEST(test_queue_applies_events_in_order);
  RUN_TEST(test_late_or_duplicate_events_ignored);
  RUN_TEST(test_payload_parsing);
  RUN_TEST(test_state_names_round_trip);
  RUN_TEST(test_english_state_names_match_french);
  RUN_TEST(test_end_and_fin_close_a_session);
  RUN_TEST(test_french_names_still_parse);
  RUN_TEST(test_unknown_name_still_unknown);
  RUN_TEST(test_json_number);
  RUN_TEST(test_json_number_skips_unit_strings);
  RUN_TEST(test_json_value);
  return UNITY_END();
}
