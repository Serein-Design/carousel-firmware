// SPDX-License-Identifier: AGPL-3.0-only
// The poster sheet the pages take their thumbnails from: one cell per screen
// of the table, in its order, then the three frames, then one per style of
// CODE, CLOCK and WEATHER; a PNG the browser can slice in 64x32 cells; within
// its flash budget. A screen added to the table
// without its poster fails here: add its line to tools/simulator/posters.mjs
// and run it.
//
//   pio test -e native -f test_posters
#include <unity.h>

#include <cstdio>
#include <cstring>

#include "../../src/i18n.cpp"
#include "../../src/json_lite.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/posters.cpp"
#include "../../src/posters_logic.cpp"
#include "../../src/screen_style.h"

void setUp() {}
void tearDown() {}

static uint32_t be32(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

void test_one_poster_per_screen_in_table_order() {
  for (int i = 0; i < SCREEN_COUNT; i++) TEST_ASSERT_EQUAL_STRING(screenNameEn(i), POSTER_NAMES[i]);
  TEST_ASSERT_EQUAL_STRING("@SIGNIN", POSTER_NAMES[POSTER_SIGN_IN]);
  TEST_ASSERT_EQUAL_STRING("@LOCKED", POSTER_NAMES[POSTER_LOCKED]);
  TEST_ASSERT_EQUAL_STRING("@NOIMAGE", POSTER_NAMES[POSTER_NO_IMAGE]);
  TEST_ASSERT_EQUAL(SCREEN_COUNT + 3 + POSTER_STYLE_CELLS, POSTER_COUNT);
}

static int screen(const char *name) {
  const int i = screenFind(name);
  TEST_ASSERT_TRUE_MESSAGE(i >= 0, name);
  return i;
}

// The style cells follow the frames, "CODE:1" for CODE in its style 1
void test_each_style_has_its_own_cell() {
  const struct { const char *screen; uint8_t count; } STYLED[] = {
      {"CODE", CODE_STYLE_COUNT}, {"CLOCK", CLOCK_STYLE_COUNT}, {"WEATHER", WEATHER_STYLE_COUNT}};
  int cells = 0;
  for (const auto &st : STYLED) {
    for (uint8_t v = 0; v < st.count; v++) {
      if (!strcmp(st.screen, "CODE") && v == CODE_AUTO) continue;
      const int cell = posterFor(screen(st.screen), v);
      char name[16];
      snprintf(name, sizeof(name), "%s:%u", st.screen, (unsigned)v);
      TEST_ASSERT_TRUE_MESSAGE(cell > POSTER_NO_IMAGE && cell < POSTER_COUNT, name);
      TEST_ASSERT_EQUAL_STRING(name, POSTER_NAMES[cell]);
      cells++;
    }
  }
  TEST_ASSERT_EQUAL(POSTER_STYLE_CELLS, cells);
}

// Auto draws the cloud while one session runs, as an owner first sees it
void test_code_auto_shows_the_cloud() {
  TEST_ASSERT_EQUAL(posterFor(screen("CODE"), CODE_CLOUD), posterFor(screen("CODE"), CODE_AUTO));
  TEST_ASSERT_NOT_EQUAL(posterFor(screen("CODE"), CODE_LIST),
                        posterFor(screen("CODE"), CODE_AUTO));
}

// A screen without styles, or a style the screen does not have, answers the
// screen's own cell
void test_other_screens_and_styles_answer_the_screen() {
  TEST_ASSERT_EQUAL(screen("TEXT"), posterFor(screen("TEXT"), 0));
  TEST_ASSERT_EQUAL(screen("TEXT"), posterFor(screen("TEXT"), 2));
  TEST_ASSERT_EQUAL(screen("CLOCK"), posterFor(screen("CLOCK"), CLOCK_STYLE_COUNT));
  TEST_ASSERT_EQUAL(screen("WEATHER"), posterFor(screen("WEATHER"), 200));
  TEST_ASSERT_EQUAL(SCREEN_COUNT, posterFor(SCREEN_COUNT, 0));
}

void test_sheet_is_a_png_of_the_right_size() {
  static const uint8_t SIGNATURE[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  TEST_ASSERT_TRUE(POSTERS_PNG_LEN > 33);
  TEST_ASSERT_EQUAL_MEMORY(SIGNATURE, POSTERS_PNG, 8);
  TEST_ASSERT_EQUAL_UINT32(13, be32(POSTERS_PNG + 8));
  TEST_ASSERT_EQUAL_MEMORY("IHDR", POSTERS_PNG + 12, 4);
  TEST_ASSERT_EQUAL_UINT32(64, be32(POSTERS_PNG + 16));
  TEST_ASSERT_EQUAL_UINT32(32 * POSTER_COUNT, be32(POSTERS_PNG + 20));
  TEST_ASSERT_EQUAL_UINT8(8, POSTERS_PNG[24]);  // bit depth
  TEST_ASSERT_EQUAL_UINT8(3, POSTERS_PNG[25]);  // colour type: indexed
  TEST_ASSERT_EQUAL_MEMORY("IEND", POSTERS_PNG + POSTERS_PNG_LEN - 8, 4);
}

void test_sheet_fits_its_budget() { TEST_ASSERT_LESS_OR_EQUAL(12288, POSTERS_PNG_LEN); }

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_one_poster_per_screen_in_table_order);
  RUN_TEST(test_each_style_has_its_own_cell);
  RUN_TEST(test_code_auto_shows_the_cloud);
  RUN_TEST(test_other_screens_and_styles_answer_the_screen);
  RUN_TEST(test_sheet_is_a_png_of_the_right_size);
  RUN_TEST(test_sheet_fits_its_budget);
  return UNITY_END();
}
