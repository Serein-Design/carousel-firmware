// SPDX-License-Identifier: AGPL-3.0-only
// TEXT, ISS and IMAGE's states: the message scrolls at a calm speed from the
// right edge and loops, accented and long messages measure and draw whole in
// the body face, and every word these screens put on the panel fits its face
// in both languages.
//
//   pio test -e native -f test_text_screen
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"
#include "../../src/i18n.cpp"
#include "../../src/grid.h"
#include "../../src/text_logic.h"
#include "../../src/iss_text.h"

// A 64x32 canvas that remembers every pixel
class Canvas : public Adafruit_GFX {
 public:
  uint16_t px[32][64];
  unsigned outside = 0;
  Canvas() : Adafruit_GFX(64, 32) { clear(); }
  void clear() { memset(px, 0, sizeof(px)); outside = 0; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= 64 || y >= 32) { outside++; return; }
    px[y][x] = color;
  }
  int count(uint16_t color) const {
    int n = 0;
    for (auto &row : px) for (uint16_t v : row) n += v == color;
    return n;
  }
  int firstRow() const {
    for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++) if (px[y][x]) return y;
    return -1;
  }
  int lastRow() const {
    for (int y = 31; y >= 0; y--) for (int x = 0; x < 64; x++) if (px[y][x]) return y;
    return -1;
  }
};
static Canvas canvas;

void setUp() { canvas.clear(); }
void tearDown() {}

static const uint16_t INK = 0xFFFF;

// --- The scroll ----------------------------------------------------------------

static void test_scroll_starts_at_the_right_edge_at_a_calm_speed() {
  TEST_ASSERT_EQUAL_INT16(64, textScrollX(100, 64, 0));
  TEST_ASSERT_EQUAL_INT16(64 - (int)TEXT_PX_PER_S, textScrollX(100, 64, 1000));
  TEST_ASSERT_EQUAL_INT16(64 - 2 * (int)TEXT_PX_PER_S, textScrollX(100, 64, 2000));
  TEST_ASSERT_TRUE(TEXT_PX_PER_S >= 16 && TEXT_PX_PER_S <= 30);   // readable, not racing
}

static void test_scroll_leaves_on_the_left_then_loops() {
  const uint16_t w = 100;
  const uint32_t pass = textPassPx(w, 64);
  TEST_ASSERT_EQUAL_UINT32(164, pass);
  const uint32_t lastMs = ((pass - 1) * 1000 + TEXT_PX_PER_S - 1) / TEXT_PX_PER_S;
  const int16_t before = textScrollX(w, 64, lastMs);
  TEST_ASSERT_TRUE(before + (int)w > 0);                  // still showing its last column
  TEST_ASSERT_EQUAL_INT16(64, textScrollX(w, 64, textPassMs(w, 64)));   // back at the right edge
  TEST_ASSERT_EQUAL_INT16(textScrollX(w, 64, 500), textScrollX(w, 64, 500 + textPassMs(w, 64)));
}

static void test_scroll_never_jumps_by_more_than_a_pixel_per_step() {
  int16_t last = textScrollX(300, 64, 0);
  for (uint32_t ms = 17; ms < 3 * textPassMs(300, 64); ms += 17) {   // 60 fps
    const int16_t x = textScrollX(300, 64, ms);
    TEST_ASSERT_TRUE(x <= last || x == 64);               // only the loop goes back
    TEST_ASSERT_TRUE(last - x <= 1 + (int)(TEXT_PX_PER_S * 17 / 1000));
    last = x;
  }
}

static void test_scroll_holds_after_a_month_without_overflow() {
  const uint32_t ms = 0xFFFFFFFFu;                        // 49.7 days
  const int16_t x = textScrollX(700, 64, ms);
  TEST_ASSERT_TRUE(x <= 64 && x > -700);
  const uint64_t pass = textPassPx(700, 64);
  TEST_ASSERT_EQUAL_INT16(64 - (int16_t)(((uint64_t)ms * TEXT_PX_PER_S / 1000) % pass), x);
}

static void test_scroll_of_nothing_stays_still() {
  TEST_ASSERT_EQUAL_INT16(64, textScrollX(0, 64, 12345));
}

// --- The message in the body face ----------------------------------------------------

static void test_accents_measure_as_their_base_letter() {
  TEST_ASSERT_EQUAL_UINT16(textWidth("Cafe a Noel", Face::BODY),
                           textWidth("Caf\xC3\xA9 \xC3\xA0 No\xC3\xABl", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(textWidth("Ca va", Face::BODY),
                           textWidth("\xC3\x87" "a v\xC3\xA0", Face::BODY));
  TEST_ASSERT_TRUE(textWidth("HELLO * CAROUSEL * ", Face::BODY) > 0);
}

// Everything a message can hold, once through toMessageText and messageDrawable,
// is drawn by the body face: no box in the alert colour
static void test_every_kept_character_is_drawn_without_a_box() {
  for (uint32_t cp = 0x20; cp < 0x180; cp++) {
    char in[5] = {0}, kept[8], out[8];
    if (cp < 0x80) in[0] = (char)cp;
    else { in[0] = (char)(0xC0 | cp >> 6); in[1] = (char)(0x80 | (cp & 0x3F)); }
    toMessageText(in, kept, sizeof(kept));
    messageDrawable(kept, out, sizeof(out));
    canvas.clear();
    drawText(canvas, out, 2, TEXT_Y, Face::BODY, INK);
    char what[40];
    snprintf(what, sizeof(what), "U+%04X", (unsigned)cp);
    TEST_ASSERT_EQUAL_MESSAGE(0, canvas.count(TYPE_MISSING_COLOR), what);
    TEST_ASSERT_TRUE_MESSAGE(canvas.count(INK) > 0 || cp == 0x20 || cp == 0xA0, what);
  }
}

// Every printable ASCII character is drawn by the body face: a lit glyph, no box
static void test_the_body_face_draws_every_printable_ascii() {
  for (int c = 0x21; c < 0x7F; c++) {
    char in[2] = {(char)c, 0};
    canvas.clear();
    drawText(canvas, in, 2, TEXT_Y, Face::BODY, INK);
    char what[24];
    snprintf(what, sizeof(what), "'%c' (0x%02X)", c, c);
    TEST_ASSERT_EQUAL_MESSAGE(0, canvas.count(TYPE_MISSING_COLOR), what);
    TEST_ASSERT_TRUE_MESSAGE(canvas.count(INK) > 0, what);
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(GRID_MAIN_TOP, canvas.firstRow(), what);
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(GRID_MAIN_BOT, canvas.lastRow(), what);
  }
}

// The default message has stars and shows them as they are
static void test_the_default_message_draws_no_missing_box() {
  char out[96];
  messageDrawable("HELLO * CAROUSEL * ", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("HELLO * CAROUSEL * ", out);
  drawText(canvas, out, 0, TEXT_Y, Face::BODY, INK);
  TEST_ASSERT_EQUAL(0, canvas.count(TYPE_MISSING_COLOR));
  messageDrawable("Caf\xC3\xA9 \"ok\"_<3 @#", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Caf\xC3\xA9 \"ok\"_<3 @#", out);
  messageDrawable("abcdef", out, 4);
  TEST_ASSERT_EQUAL_STRING("abc", out);
}

// Only what the face still lacks is swapped: a stored byte outside ASCII and the
// composed French letters becomes a '?', whole sequence at a time
static void test_messageDrawable_swaps_only_what_is_still_missing() {
  char out[32];
  messageDrawable("a\xE2\x98\x83" "b", out, sizeof(out));   // a snowman
  TEST_ASSERT_EQUAL_STRING("a?b", out);
  messageDrawable("\xF0\x9F\x98\x80!", out, sizeof(out));   // an emoji
  TEST_ASSERT_EQUAL_STRING("?!", out);
  messageDrawable("\xC3\x89t\xC3\xA9 \xC5\x93", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("\xC3\x89t\xC3\xA9 \xC5\x93", out);
  messageDrawable("\xC3" "x", out, sizeof(out));             // a broken byte
  TEST_ASSERT_EQUAL_STRING("?x", out);
  messageDrawable("ab\xC3\xA9", out, 4);                    // a letter is never cut
  TEST_ASSERT_EQUAL_STRING("ab", out);
  messageDrawable("\xE0\x83\xA9!", out, sizeof(out));       // an overlong \u00E9 is missing
  TEST_ASSERT_EQUAL_STRING("?!", out);
}

// Marks above the capitals and the cedilla and descenders below stay in the main rows
static void test_the_line_sits_in_the_main_rows() {
  drawText(canvas, "\xC3\x89\xC3\x80\xC3\x94\xC3\x87 gjpqy \xC5\x92 \xC3\x8B", 2, TEXT_Y, Face::BODY, INK);
  TEST_ASSERT_GREATER_OR_EQUAL(GRID_MAIN_TOP, canvas.firstRow());
  TEST_ASSERT_LESS_OR_EQUAL(GRID_MAIN_BOT, canvas.lastRow());
  TEST_ASSERT_EQUAL(0, canvas.outside);
}

// Longest stored message: 95 bytes of the widest letters, and of accented ones
static void test_long_messages_loop_inside_a_short_int() {
  const std::string wide(95, 'W');
  std::string accented;
  while (accented.size() + 2 <= 95) accented += "\xC3\x89";
  for (const std::string &s : {wide, accented}) {
    char msg[96];
    toMessageText(s.c_str(), msg, sizeof(msg));
    const uint16_t w = textWidth(msg, Face::BODY);
    TEST_ASSERT_TRUE(w > 64);
    TEST_ASSERT_TRUE(textPassPx(w, 64) < 30000);
    // Every position of the loop draws without writing off the canvas to the right
    // of its edge: the text only enters from there
    for (uint32_t ms = 0; ms < textPassMs(w, 64); ms += 997) {
      canvas.clear();
      const int16_t x = textScrollX(w, 64, ms);
      TEST_ASSERT_TRUE(x <= 64 && x >= -(int)w);
      drawText(canvas, msg, x, TEXT_Y, Face::BODY, INK);
      TEST_ASSERT_EQUAL(0, canvas.count(TYPE_MISSING_COLOR));
    }
  }
}

// --- ISS and IMAGE: every word fits its face ---------------------------------------------

static void test_iss_words_fit_both_orientations() {
  for (uint8_t l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    char txt[24];
    for (int km : {0, 7, 1999, 2000, 12345, 20015}) {
      for (bool nearby : {true, false}) {
        issDistanceText(txt, sizeof(txt), nearby, km, t);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(GRID_LINE_W, textWidth(txt, Face::LABEL), txt);
      }
    }
    for (bool alt : {true, false}) {
      issSpeedText(txt, sizeof(txt), alt);
      TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(GRID_LINE_W, textWidth(txt, Face::LABEL), txt);
    }
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(GRID_PORTRAIT_LINE_W, textWidth(ISS_DEMO_LABEL, Face::LABEL));
    for (const char *w : {t.land, t.ocean, "LAT", "LON"})
      TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(GRID_PORTRAIT_LINE_W, textWidth(w, Face::LABEL), w);
    for (const char *n : {"-90", "-180", "180", "-0"})   // latitude and longitude, whole degrees
      TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(GRID_PORTRAIT_LINE_W, textWidth(n, Face::BODY), n);
  }
}

static void test_state_face_follows_the_width() {
  TEST_ASSERT_TRUE(stateFace("NO", "IMAGE", GRID_PORTRAIT_LINE_W) == Face::BODY);
  TEST_ASSERT_TRUE(stateFace("NO", "STORE", GRID_PORTRAIT_LINE_W) == Face::LABEL);
  TEST_ASSERT_TRUE(stateFace("SANS", "IMAGE", GRID_PORTRAIT_LINE_W) == Face::BODY);
}

static void test_image_states_fit_both_orientations() {
  for (uint8_t l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    for (const char *const *w : {t.imgOther, t.imgNoStore}) {
      TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(GRID_LINE_W, textWidth(w[0], Face::BODY), w[0]);
      TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(GRID_LINE_W, textWidth(w[1], Face::LABEL), w[1]);
    }
    for (const char *const *w : {t.imgOtherTall, t.imgNoStoreTall}) {
      const Face top = stateFace(w[0], w[1], GRID_PORTRAIT_LINE_W);   // STORE is too wide for the body face
      for (int i = 0; i < 2; i++)
        TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(GRID_PORTRAIT_LINE_W, textWidth(w[i], top), w[i]);
      for (int i = 2; i < 4; i++)
        TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(GRID_PORTRAIT_LINE_W, textWidth(w[i], Face::LABEL), w[i]);
    }
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_scroll_starts_at_the_right_edge_at_a_calm_speed);
  RUN_TEST(test_scroll_leaves_on_the_left_then_loops);
  RUN_TEST(test_scroll_never_jumps_by_more_than_a_pixel_per_step);
  RUN_TEST(test_scroll_holds_after_a_month_without_overflow);
  RUN_TEST(test_scroll_of_nothing_stays_still);
  RUN_TEST(test_accents_measure_as_their_base_letter);
  RUN_TEST(test_every_kept_character_is_drawn_without_a_box);
  RUN_TEST(test_the_body_face_draws_every_printable_ascii);
  RUN_TEST(test_the_default_message_draws_no_missing_box);
  RUN_TEST(test_messageDrawable_swaps_only_what_is_still_missing);
  RUN_TEST(test_the_line_sits_in_the_main_rows);
  RUN_TEST(test_long_messages_loop_inside_a_short_int);
  RUN_TEST(test_iss_words_fit_both_orientations);
  RUN_TEST(test_state_face_follows_the_width);
  RUN_TEST(test_image_states_fit_both_orientations);
  return UNITY_END();
}
