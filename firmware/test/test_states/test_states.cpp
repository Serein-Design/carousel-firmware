// SPDX-License-Identifier: AGPL-3.0-only
// The panel's states nobody chooses: the sign-in code (S1), the locked panel
// (L1), a service with no fresh data (E1) and no image (I1). What they say
// fits its line in both languages, where the digits sit, which reason shows
// for which state, how the address breaks when it is too long for a line.
//
//   pio test -e native -f test_states
#include <unity.h>

#include <cstring>
#include <string>

#include "../../src/fonts_data.cpp"
#include "../../src/panel_name.cpp"
#include "../../src/i18n.cpp"
#include "../../src/states_logic.cpp"
#include "../../src/type.cpp"
#include "../../src/weather_logic.cpp"
#include "../../src/json_lite.cpp"

void setUp() {}
void tearDown() {}

static const uint16_t LINE = 60, TALL_LINE = 28;   // grid.h: a system line

static void assertFits(const char *s, uint16_t width, Face f, const char *what) {
  char msg[128];
  snprintf(msg, sizeof(msg), "%s: \"%s\" is %u px, room for %u", what, s, textWidth(s, f), width);
  TEST_ASSERT_TRUE_MESSAGE(s && s[0], msg);
  TEST_ASSERT_TRUE_MESSAGE(textWidth(s, f) <= width, msg);
}

// --- Strings -----------------------------------------------------------------

static void test_sign_in_titles_fit() {
  for (int l = 0; l < LANG_COUNT; l++) {
    assertFits(STRINGS[l].signInPanel, LINE, Face::LABEL, "signInPanel");
    assertFits(STRINGS[l].signInPanelTall, TALL_LINE, Face::LABEL, "signInPanelTall");
  }
  TEST_ASSERT_EQUAL_STRING("SIGN-IN CODE", STRINGS[LANG_EN].signInPanel);
  TEST_ASSERT_EQUAL_STRING("CODE D\xE2\x80\x99" "ACC\xC3\x88S", STRINGS[LANG_FR].signInPanel);
}

static void test_locked_says_what_happened_and_what_to_do() {
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("LOCKED", en.locked[0]);
  TEST_ASSERT_EQUAL_STRING("RESET NOT DONE", en.locked[1]);
  TEST_ASSERT_EQUAL_STRING("RESTART IT", en.locked[2]);
  TEST_ASSERT_EQUAL_STRING("VERROUILL\xC3\x89", fr.locked[0]);
  TEST_ASSERT_EQUAL_STRING("RAZ INACHEV\xC3\x89" "E", fr.locked[1]);
  TEST_ASSERT_EQUAL_STRING("RED\xC3\x89MARREZ", fr.locked[2]);
  for (int l = 0; l < LANG_COUNT; l++) {
    // The first line sits beside the lock's 6 px
    assertFits(STRINGS[l].locked[0], LINE - 6, Face::LABEL, "locked");
    assertFits(STRINGS[l].locked[1], LINE, Face::LABEL, "locked");
    assertFits(STRINGS[l].locked[2], LINE, Face::LABEL, "locked");
    for (const char *s : STRINGS[l].lockedTall) assertFits(s, TALL_LINE, Face::LABEL, "lockedTall");
  }
}

static void test_no_image_asks_for_one() {
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("No image", en.noImage);
  TEST_ASSERT_EQUAL_STRING("ADD ONE AT", en.noImageAsk);
  TEST_ASSERT_EQUAL_STRING("Pas d\xE2\x80\x99image", fr.noImage);
  TEST_ASSERT_EQUAL_STRING("AJOUTEZ-EN SUR", fr.noImageAsk);
  for (int l = 0; l < LANG_COUNT; l++) {
    // The body line starts after the picture's 6 px: the line less those
    assertFits(STRINGS[l].noImage, LINE - 6, Face::BODY, "noImage");
    assertFits(STRINGS[l].noImageAsk, LINE, Face::LABEL, "noImageAsk");
    for (int i = 0; i < 2; i++)
      assertFits(STRINGS[l].noImageTall[i], TALL_LINE, Face::BODY, "noImageTall");
    for (int i = 2; i < 5; i++)
      assertFits(STRINGS[l].noImageTall[i], TALL_LINE, Face::LABEL, "noImageTall");
  }
}

static void test_every_reason_fits_the_foot_band() {
  static_assert(sizeof(Strings::staleWords) / sizeof(const char *) == 3, "one word per reason");
  for (int l = 0; l < LANG_COUNT; l++)
    for (const char *w : STRINGS[l].staleWords) {
      // Landscape: after the 3 px mark, one line. Portrait: two lines of 28 px
      assertFits(w, LINE - 4, Face::LABEL, "staleWord");
      char a[32], b[32];
      splitLines(w, Face::LABEL, TALL_LINE, a, b, sizeof(a));
      assertFits(a, TALL_LINE, Face::LABEL, "staleWord, first line");
      if (b[0]) assertFits(b, TALL_LINE, Face::LABEL, "staleWord, second line");
    }
  TEST_ASSERT_EQUAL_STRING("Offline", STRINGS[LANG_EN].staleWords[0]);
  TEST_ASSERT_EQUAL_STRING("No key", STRINGS[LANG_EN].staleWords[1]);
  TEST_ASSERT_EQUAL_STRING("Server", STRINGS[LANG_EN].staleWords[2]);
  TEST_ASSERT_EQUAL_STRING("Hors ligne", STRINGS[LANG_FR].staleWords[0]);
  TEST_ASSERT_EQUAL_STRING("Sans cl\xC3\xA9", STRINGS[LANG_FR].staleWords[1]);
  TEST_ASSERT_EQUAL_STRING("Serveur", STRINGS[LANG_FR].staleWords[2]);
}

static void test_a_reason_names_its_word() {
  const Strings &en = STRINGS[LANG_EN];
  TEST_ASSERT_NULL(reasonWord(REASON_NONE, en));
  TEST_ASSERT_EQUAL_STRING("Offline", reasonWord(REASON_OFFLINE, en));
  TEST_ASSERT_EQUAL_STRING("No key", reasonWord(REASON_NO_KEY, en));
  TEST_ASSERT_EQUAL_STRING("Server", reasonWord(REASON_SERVER, en));
}

// --- Sign-in code ----------------------------------------------------------------

static void test_digits_keep_their_zeros() {
  char d[8];
  signInDigits(482913, d);
  TEST_ASSERT_EQUAL_STRING("482913", d);
  signInDigits(7, d);
  TEST_ASSERT_EQUAL_STRING("000007", d);
}

static void test_digits_group_three_and_three_inside_the_line() {
  const uint16_t digit = textWidth("4", Face::PIERRE);
  for (int tall = 0; tall < 2; tall++) {
    const int16_t line = tall ? 28 : 60;
    for (uint8_t i = 0; i < 6; i++) {
      const DigitSlot s = signInSlot(i, tall);
      char msg[40];
      snprintf(msg, sizeof(msg), "digit %u, tall %d", i, tall);
      TEST_ASSERT_TRUE_MESSAGE(s.x >= 2 && s.x + digit <= 2 + line, msg);
      TEST_ASSERT_TRUE_MESSAGE(s.y >= 9 && s.y + 13 <= (tall ? 56 : 29), msg);
    }
    // Inside a group the digits are 9 px apart; a group is set apart
    for (uint8_t i : {0, 1, 3, 4})
      TEST_ASSERT_EQUAL_INT16(9, signInSlot(i + 1, tall).x - signInSlot(i, tall).x);
  }
  // Landscape: one row, a gap wider than a digit's pitch between the groups
  TEST_ASSERT_EQUAL_INT16(signInSlot(0, false).y, signInSlot(5, false).y);
  TEST_ASSERT_TRUE(signInSlot(3, false).x - signInSlot(2, false).x > 9);
  // Portrait: the groups stack, the same columns
  TEST_ASSERT_TRUE(signInSlot(3, true).y >= signInSlot(0, true).y + 13 + 2);
  TEST_ASSERT_EQUAL_INT16(signInSlot(0, true).x, signInSlot(3, true).x);
}

static void test_the_line_counts_the_time_left_down() {
  TEST_ASSERT_EQUAL_INT16(60, signInBarWidth(1.0f, 60));
  TEST_ASSERT_EQUAL_INT16(30, signInBarWidth(0.5f, 60));
  TEST_ASSERT_EQUAL_INT16(0, signInBarWidth(0.0f, 60));
  TEST_ASSERT_EQUAL_INT16(0, signInBarWidth(-0.2f, 60));
  TEST_ASSERT_EQUAL_INT16(60, signInBarWidth(1.7f, 60));
  // A code with time left is never an empty line
  TEST_ASSERT_EQUAL_INT16(1, signInBarWidth(0.001f, 60));
  int16_t last = 61;
  for (float k = 1.0f; k >= 0; k -= 0.05f) {
    const int16_t w = signInBarWidth(k, 60);
    TEST_ASSERT_TRUE(w <= last);
    last = w;
  }
}

// --- A service with no fresh data ------------------------------------------------

static const uint32_t MIN = 60000UL;

static ServiceFacts facts(bool has, uint32_t at, bool wifi, bool keyMissing) {
  return ServiceFacts{has, at, wifi, keyMissing};
}

static void test_fresh_data_shows_no_reason() {
  TEST_ASSERT_EQUAL(REASON_NONE, serviceReason(facts(true, 100 * MIN, true, false), 110 * MIN, 35 * MIN));
  // Even with the Wi-Fi down for a while: the reading is still good
  TEST_ASSERT_EQUAL(REASON_NONE, serviceReason(facts(true, 100 * MIN, false, false), 110 * MIN, 35 * MIN));
  // The edge: exactly as old as allowed is still fresh
  TEST_ASSERT_EQUAL(REASON_NONE, serviceReason(facts(true, 100 * MIN, true, false), 135 * MIN, 35 * MIN));
}

static void test_nothing_to_keep_shows_no_reason() {
  // No reading yet: the screen's own loading state stays, E1 keeps a last reading
  TEST_ASSERT_EQUAL(REASON_NONE, serviceReason(facts(false, 0, false, false), 500 * MIN, 35 * MIN));
}

static void test_old_data_says_why_in_priority_order() {
  const uint32_t at = 100 * MIN, now = at + 40 * MIN, fresh = 35 * MIN;
  TEST_ASSERT_EQUAL(REASON_SERVER, serviceReason(facts(true, at, true, false), now, fresh));
  TEST_ASSERT_EQUAL(REASON_NO_KEY, serviceReason(facts(true, at, true, true), now, fresh));
  TEST_ASSERT_EQUAL(REASON_OFFLINE, serviceReason(facts(true, at, false, false), now, fresh));
  // No Wi-Fi explains the rest: it is fixed first
  TEST_ASSERT_EQUAL(REASON_OFFLINE, serviceReason(facts(true, at, false, true), now, fresh));
}

static void test_the_clock_wrapping_changes_nothing() {
  // millis() wraps after 49 days: the age is a difference
  const uint32_t at = 0xFFFFFFFFUL - 10 * MIN, now = 20 * MIN;   // 30 min later
  TEST_ASSERT_EQUAL(REASON_NONE, serviceReason(facts(true, at, true, false), now, 35 * MIN));
  TEST_ASSERT_EQUAL(REASON_SERVER, serviceReason(facts(true, at, true, false), now, 25 * MIN));
}

static void test_a_reading_is_kept_at_half_its_light() {
  TEST_ASSERT_EQUAL_HEX16(0, dimmed(0));
  TEST_ASSERT_EQUAL_HEX16(0x7BEF, dimmed(0xFFFF));
  // Each channel halves alone: no bit slides into its neighbour
  TEST_ASSERT_EQUAL_HEX16(0x4000, dimmed(0x8000));
  TEST_ASSERT_EQUAL_HEX16(0x0000, dimmed(0x0001));
  TEST_ASSERT_EQUAL_HEX16(0x0000, dimmed(0x0820));   // the lowest bit of red and green: gone
  TEST_ASSERT_EQUAL_HEX16(0x0820, dimmed(0x1040));
  // Twice is a quarter
  TEST_ASSERT_EQUAL_HEX16(0x39E7, dimmed(dimmed(0xFFFF)));
}

// --- The address: one line, or the name and ".local" on two ----------------------

static void test_a_short_address_stays_on_one_line() {
  const AddressLines a = addressLines("carousel.local", LINE);
  TEST_ASSERT_EQUAL_UINT8(1, a.count);
  TEST_ASSERT_EQUAL_STRING("carousel.local", a.line[0]);
  assertFits(a.line[0], LINE, Face::LABEL, "carousel.local");
}

static void test_the_unique_address_breaks_before_local() {
  const AddressLines a = addressLines("carousel-695c.local", LINE);
  TEST_ASSERT_EQUAL_UINT8(2, a.count);
  TEST_ASSERT_EQUAL_STRING("carousel-695c", a.line[0]);
  TEST_ASSERT_EQUAL_STRING(".local", a.line[1]);
  assertFits(a.line[0], LINE, Face::LABEL, "unique name");
  assertFits(a.line[1], LINE, Face::LABEL, ".local");
}

static void test_every_unique_name_fits_on_two_lines() {
  // carousel-xxxx.local, xxxx the last four hex digits of the MAC: the
  // widest digits are tried in every position
  for (const char *d : {"0000", "8888", "eeee", "ffff", "aaaa", "bbbb", "cccc", "dddd", "1234",
                        "5678", "9999", "7777", "2222", "3333", "4444", "6666"}) {
    char name[32];
    snprintf(name, sizeof(name), "carousel-%s.local", d);
    const AddressLines a = addressLines(name, LINE);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, a.count, name);
    TEST_ASSERT_EQUAL_STRING(".local", a.line[1]);
    assertFits(a.line[0], LINE, Face::LABEL, name);
    // Nothing lost: the two lines are the name
    TEST_ASSERT_EQUAL_STRING(name, (std::string(a.line[0]) + a.line[1]).c_str());
  }
}

static void test_a_name_too_wide_for_any_line_is_cut_not_dropped() {
  const AddressLines a = addressLines("a-very-long-panel-name-indeed.local", LINE);
  TEST_ASSERT_TRUE(a.count >= 1);
  for (int i = 0; i < a.count; i++) assertFits(a.line[i], LINE, Face::LABEL, "cut name");
  TEST_ASSERT_EQUAL_UINT8(0, addressLines("", LINE).count);
  TEST_ASSERT_EQUAL_UINT8(0, addressLines(nullptr, LINE).count);
}

static void test_the_panel_name_can_be_shown() {
  // The fallback is carousel.local, the panel's own carousel-xxxx.local once
  // it is known: either way a .local name the screens can lay out
  const char *name = panelHostName();
  TEST_ASSERT_NOT_NULL(name);
  const size_t n = strlen(name);
  TEST_ASSERT_TRUE(n > 6 && !strcmp(name + n - 6, ".local"));
  const AddressLines a = addressLines(name, LINE);
  TEST_ASSERT_TRUE(a.count >= 1);
  for (int i = 0; i < a.count; i++) assertFits(a.line[i], LINE, Face::LABEL, name);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sign_in_titles_fit);
  RUN_TEST(test_locked_says_what_happened_and_what_to_do);
  RUN_TEST(test_no_image_asks_for_one);
  RUN_TEST(test_every_reason_fits_the_foot_band);
  RUN_TEST(test_a_reason_names_its_word);
  RUN_TEST(test_digits_keep_their_zeros);
  RUN_TEST(test_digits_group_three_and_three_inside_the_line);
  RUN_TEST(test_the_line_counts_the_time_left_down);
  RUN_TEST(test_fresh_data_shows_no_reason);
  RUN_TEST(test_nothing_to_keep_shows_no_reason);
  RUN_TEST(test_old_data_says_why_in_priority_order);
  RUN_TEST(test_the_clock_wrapping_changes_nothing);
  RUN_TEST(test_a_reading_is_kept_at_half_its_light);
  RUN_TEST(test_a_short_address_stays_on_one_line);
  RUN_TEST(test_the_unique_address_breaks_before_local);
  RUN_TEST(test_every_unique_name_fits_on_two_lines);
  RUN_TEST(test_a_name_too_wide_for_any_line_is_cut_not_dropped);
  RUN_TEST(test_the_panel_name_can_be_shown);
  return UNITY_END();
}
