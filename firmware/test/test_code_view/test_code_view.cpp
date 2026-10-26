// SPDX-License-Identifier: AGPL-3.0-only
// The CODE screen's choices: the cloud or the list (Auto from three
// sessions), a word for each state that fits its face in both languages,
// the list's order with the sessions that need the owner first, and the
// done check held 2.5 s.
//
//   pio test -e native -f test_code_view
#include <unity.h>

#include <cstdio>
#include <cstring>

#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"
#include "../../src/i18n.cpp"
#include "../../src/code_view_logic.cpp"
#include "../../src/grid.h"
#include "../../src/screen_style.h"

void setUp() {}
void tearDown() {}

static void test_auto_shows_the_list_from_three_sessions() {
  TEST_ASSERT_FALSE(codeShowsList(CODE_AUTO, 1));
  TEST_ASSERT_FALSE(codeShowsList(CODE_AUTO, 2));
  TEST_ASSERT_TRUE(codeShowsList(CODE_AUTO, 3));
  TEST_ASSERT_TRUE(codeShowsList(CODE_AUTO, 5));
}

static void test_cloud_and_list_ignore_the_count() {
  for (int n : {1, 2, 3, 5}) {
    TEST_ASSERT_FALSE(codeShowsList(CODE_CLOUD, n));
    TEST_ASSERT_TRUE(codeShowsList(CODE_LIST, n));
  }
}

// No session: nothing to list, the cloud says it waits
static void test_no_session_is_the_cloud() {
  for (uint8_t s = 0; s < CODE_STYLE_COUNT; s++) TEST_ASSERT_FALSE(codeShowsList(s, 0));
}

static void test_state_words_in_english_and_french() {
  struct { AgentState s; const char *en, *fr; } want[] = {
    {AG_IDLE, "Idle", "Au repos"},
    {AG_THINKING, "Thinking…", "Réfléchit…"},
    {AG_TOOL, "Working", "Travaille"},
    {AG_ATTENTION, "Needs you", "Vous attend"},
    {AG_ERROR, "Failed", "Échec"},
    {AG_FAILED, "Tool error", "Erreur outil"},
    {AG_DONE, "Done", "Terminé"},
  };
  TEST_ASSERT_EQUAL_INT(AGENT_STATE_COUNT, sizeof(want) / sizeof(want[0]));
  for (auto &w : want) {
    TEST_ASSERT_EQUAL_STRING(w.en, codeStateWord(w.s, STRINGS[LANG_EN]));
    TEST_ASSERT_EQUAL_STRING(w.fr, codeStateWord(w.s, STRINGS[LANG_FR]));
  }
}

static void assertFits(const char *s, Face f) {
  char msg[96];
  snprintf(msg, sizeof(msg), "\"%s\" is %u px", s, textWidth(s, f));
  TEST_ASSERT_TRUE_MESSAGE(textWidth(s, f) <= GRID_LINE_W, msg);
}

// Every word the cloud can show, in the body face, within the 60 px line
static void test_every_word_fits_the_line() {
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    for (int s = 0; s < AGENT_STATE_COUNT; s++) assertFits(codeStateWord((AgentState)s, t), Face::BODY);
    assertFits(t.codeWaiting, Face::BODY);
    assertFits(t.codeNoWifi, Face::BODY);
  }
}

// The minutes take one %u. The longest, 99, leaves the project name half
// the cloud's label line, and the list's names 8 capitals or so
static void test_minutes_labels() {
  for (int l = 0; l < LANG_COUNT; l++) {
    char b[16];
    snprintf(b, sizeof(b), STRINGS[l].codeMin, 99u);
    TEST_ASSERT_TRUE(textWidth(b, Face::LABEL) <= 25);
    snprintf(b, sizeof(b), STRINGS[l].codeMinShort, 99u);
    TEST_ASSERT_TRUE(textWidth(b, Face::LABEL) <= 13);
  }
  char b[16];
  snprintf(b, sizeof(b), STRINGS[LANG_EN].codeMin, 2u);
  TEST_ASSERT_EQUAL_STRING("2 MIN", b);
  TEST_ASSERT_EQUAL_UINT(0, codeMinutes(59999));
  TEST_ASSERT_EQUAL_UINT(7, codeMinutes(7 * 60000UL + 5));
  TEST_ASSERT_EQUAL_UINT(99, codeMinutes(500 * 60000UL));
}

static void test_list_puts_the_sessions_that_need_you_first() {
  const AgentState st[] = {AG_THINKING, AG_ATTENTION, AG_DONE, AG_ATTENTION, AG_TOOL};
  uint8_t out[CODE_LIST_ROWS];
  TEST_ASSERT_EQUAL_INT(4, codeListOrder(st, 5, out, CODE_LIST_ROWS));
  const uint8_t want[] = {1, 3, 0, 2};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, out, 4);
}

static void test_list_keeps_rank_order_otherwise() {
  const AgentState st[] = {AG_DONE, AG_TOOL, AG_IDLE};
  uint8_t out[CODE_LIST_ROWS];
  TEST_ASSERT_EQUAL_INT(3, codeListOrder(st, 3, out, CODE_LIST_ROWS));
  const uint8_t want[] = {0, 1, 2};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, out, 3);
}

// Only the state that waits for the owner is the orange row; an error has
// its own word and colour
static void test_only_needs_you_is_highlighted() {
  for (int s = 0; s < AGENT_STATE_COUNT; s++)
    TEST_ASSERT_EQUAL(s == AG_ATTENTION, codeNeedsOwner((AgentState)s));
}

// The bridge puts done back to idle after CELEBRATE_MS; the screen holds the
// check until 2.5 s after the Stop
static void test_done_is_held_two_and_a_half_seconds() {
  const uint32_t stop = 10000, fallback = stop + CELEBRATE_MS + 20;
  TEST_ASSERT_EQUAL(AG_DONE, codeShownState(AG_DONE, stop, stop, stop + 100));
  TEST_ASSERT_EQUAL(AG_DONE, codeShownState(AG_IDLE, fallback, stop, stop + 2400));
  TEST_ASSERT_EQUAL(AG_IDLE, codeShownState(AG_IDLE, fallback, stop, stop + CODE_DONE_HOLD_MS));
  // an idle the hooks sent shows at once
  TEST_ASSERT_EQUAL(AG_IDLE, codeShownState(AG_IDLE, stop, stop, stop + 100));
  // the other states pass through
  TEST_ASSERT_EQUAL(AG_TOOL, codeShownState(AG_TOOL, stop, stop, stop + 100));
}

static void test_project_label_in_capitals_and_cut_to_fit() {
  char b[16];
  codeFitLabel("carousel", Face::LABEL, 60, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("CAROUSEL", b);
  codeFitLabel("very-long-name", Face::LABEL, 30, b, sizeof(b));
  TEST_ASSERT_TRUE(textWidth(b, Face::LABEL) <= 30);
  TEST_ASSERT_TRUE(strlen(b) > 0);
  TEST_ASSERT_EQUAL_INT(0, strncmp("VERY-LONG-NAME", b, strlen(b)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_auto_shows_the_list_from_three_sessions);
  RUN_TEST(test_cloud_and_list_ignore_the_count);
  RUN_TEST(test_no_session_is_the_cloud);
  RUN_TEST(test_state_words_in_english_and_french);
  RUN_TEST(test_every_word_fits_the_line);
  RUN_TEST(test_minutes_labels);
  RUN_TEST(test_list_puts_the_sessions_that_need_you_first);
  RUN_TEST(test_list_keeps_rank_order_otherwise);
  RUN_TEST(test_only_needs_you_is_highlighted);
  RUN_TEST(test_done_is_held_two_and_a_half_seconds);
  RUN_TEST(test_project_label_in_capitals_and_cut_to_fit);
  return UNITY_END();
}
