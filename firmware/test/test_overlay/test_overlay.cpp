// SPDX-License-Identifier: AGPL-3.0-only
// What a brightness change or a knob turn draws over the screen: the rise,
// hold and fall every feedback shares (a new event holds again without a
// second rise), the count of the screens that are on and fit the orientation,
// every variant inside 64x32 and 32x64 with its text clear of the margins
// and of itself, and nothing at all for None.
//
//   pio test -e native -f test_overlay
#include <unity.h>

#include <cstring>

#include "../../src/i18n.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"
#include "../../src/palette.cpp"
#include "../../src/overlay.cpp"

// gallery.cpp's palette() knows the night hours; the day palette here
const Palette &palette() { return paletteFor(false); }

// A canvas of either orientation that keeps every pixel, counts writes off
// its edges, and counts text pixels drawn over other text (a clash)
class Canvas : public Adafruit_GFX {
 public:
  uint16_t px[64][64];
  unsigned outside = 0, writes = 0, clashes = 0;
  Canvas(int16_t w, int16_t h) : Adafruit_GFX(w, h) { memset(px, 0, sizeof(px)); }
  static bool ink(uint16_t c) {
    const Palette &p = palette();
    return c == p.text || c == p.label || c == p.accent;
  }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    writes++;
    if (x < 0 || y < 0 || x >= width() || y >= height()) { outside++; return; }
    if (ink(color) && ink(px[y][x]) && px[y][x] != color) clashes++;
    px[y][x] = color;
  }
  int count(uint16_t color, int x0 = 0, int y0 = 0, int x1 = 63, int y1 = 63) const {
    int n = 0;
    for (int y = y0; y <= y1 && y < height(); y++)
      for (int x = x0; x <= x1 && x < width(); x++) n += px[y][x] == color;
    return n;
  }
  // Text-role pixels outside the 2 px margins
  int inkInMargins() const {
    int n = 0;
    for (int y = 0; y < height(); y++)
      for (int x = 0; x < width(); x++)
        if (ink(px[y][x]) && (x < GRID_MARGIN || x >= width() - GRID_MARGIN)) n++;
    return n;
  }
  int topmostWritten() const {
    for (int y = 0; y < height(); y++)
      for (int x = 0; x < width(); x++) if (px[y][x]) return y;
    return -1;
  }
};

void setUp() {}
void tearDown() {}

static const int16_t SIZES[2][2] = {{64, 32}, {32, 64}};

// --- the curve ---------------------------------------------------------------

static void test_curve_rises_holds_and_falls() {
  Feedback f;
  TEST_ASSERT_EQUAL_FLOAT(0, f.shown(0));                 // nothing yet
  f.show(FeedbackKind::BRIGHTNESS, 1000);
  TEST_ASSERT_EQUAL_FLOAT(0, f.shown(1000));
  const float half = f.shown(1060);
  TEST_ASSERT_TRUE(half > 0.9f && half < 1);             // ease-out: most of the way at half time
  TEST_ASSERT_EQUAL_FLOAT(1, f.shown(1000 + FEEDBACK_RISE_MS));
  TEST_ASSERT_EQUAL_FLOAT(1, f.shown(1000 + FEEDBACK_HOLD_MS));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, f.shown(1000 + FEEDBACK_HOLD_MS + FEEDBACK_FALL_MS / 2));
  TEST_ASSERT_EQUAL_FLOAT(0, f.shown(1000 + FEEDBACK_HOLD_MS + FEEDBACK_FALL_MS));
  TEST_ASSERT_EQUAL_FLOAT(0, f.shown(1000 + 60000));
  TEST_ASSERT_TRUE(FeedbackKind::BRIGHTNESS == f.kind());
}

static void test_a_new_event_holds_again_without_a_new_rise() {
  Feedback f;
  f.show(FeedbackKind::BRIGHTNESS, 0);
  f.show(FeedbackKind::BRIGHTNESS, 1000);                // during the hold
  TEST_ASSERT_EQUAL_FLOAT(1, f.shown(1000));
  TEST_ASSERT_EQUAL_FLOAT(1, f.shown(2000 + FEEDBACK_HOLD_MS - 1001));
  TEST_ASSERT_EQUAL_FLOAT(1, f.shown(1000 + FEEDBACK_HOLD_MS));
  TEST_ASSERT_EQUAL_FLOAT(0, f.shown(1000 + FEEDBACK_HOLD_MS + FEEDBACK_FALL_MS));

  Feedback g;                                            // during the fall: back up at once
  g.show(FeedbackKind::BRIGHTNESS, 0);
  g.show(FeedbackKind::BRIGHTNESS, FEEDBACK_HOLD_MS + 100);
  TEST_ASSERT_EQUAL_FLOAT(1, g.shown(FEEDBACK_HOLD_MS + 100));

  Feedback r;                                            // during the rise: the rise goes on
  r.show(FeedbackKind::BRIGHTNESS, 0);
  r.show(FeedbackKind::BRIGHTNESS, 60);
  const float k = r.shown(60);
  TEST_ASSERT_TRUE(k > 0 && k < 1);
  TEST_ASSERT_EQUAL_FLOAT(1, r.shown(60 + FEEDBACK_HOLD_MS - 1));
  TEST_ASSERT_EQUAL_FLOAT(0, r.shown(60 + FEEDBACK_HOLD_MS + FEEDBACK_FALL_MS));
}

static void test_once_over_it_rises_again_and_survives_the_wrap() {
  Feedback f;
  const uint32_t t0 = 0xFFFFFF00u;                       // millis() wraps during the hold
  f.show(FeedbackKind::KNOB, t0, 1);
  TEST_ASSERT_EQUAL_FLOAT(1, f.shown(t0 + 500));
  TEST_ASSERT_EQUAL_FLOAT(0, f.shown(t0 + 5000));
  f.show(FeedbackKind::KNOB, t0 + 6000, -1);
  TEST_ASSERT_EQUAL_FLOAT(0, f.shown(t0 + 6000));        // a fresh rise
  TEST_ASSERT_EQUAL_INT8(-1, f.dir());
  TEST_ASSERT_EQUAL_UINT32(100, f.since(t0 + 6100));
}

static void test_another_kind_rises_afresh() {
  Feedback f;
  f.show(FeedbackKind::KNOB, 0, 1);
  f.show(FeedbackKind::BRIGHTNESS, 1000);
  TEST_ASSERT_EQUAL_FLOAT(0, f.shown(1000));
  TEST_ASSERT_TRUE(FeedbackKind::BRIGHTNESS == f.kind());
}

// --- the count ---------------------------------------------------------------

static void test_the_count_follows_the_screens_that_are_on() {
  ScreenList l = screenListAll();
  // Running order reversed, three screens off, all three drawn both ways
  for (uint8_t p = 0; p < SCREEN_COUNT; p++) l.order[p] = SCREEN_COUNT - 1 - p;
  l.on &= ~(1u << 0) & ~(1u << 5) & ~(1u << (SCREEN_COUNT - 2));
  // Portrait: the four landscape-only screens (CODE, SLEEP, RUN, TEXT) are out too
  const ScreenPlace first = knobPlace(l, SCREEN_COUNT - 1, true);
  TEST_ASSERT_EQUAL_INT(1, first.pos);
  TEST_ASSERT_EQUAL_INT(SCREEN_COUNT - 3 - 4, first.count);
  // SCREEN_COUNT - 2 is off: SCREEN_COUNT - 3 is the second one on
  TEST_ASSERT_EQUAL_INT(2, knobPlace(l, SCREEN_COUNT - 3, true).pos);
  TEST_ASSERT_EQUAL_INT(SCREEN_COUNT - 3 - 4, knobPlace(l, 2, true).pos);   // DANCE, the last that plays
  TEST_ASSERT_EQUAL_INT(0, knobPlace(l, 5, true).pos);                      // off: no place
}

// The count is the knob's and Stage's: the screens on that fit the
// orientation, so a screen that does not fit has no place
static void test_the_count_leaves_out_the_screens_that_do_not_fit() {
  const ScreenList l = screenListAll();
  const int cat = screenFind("CAT"), code = screenFind("CODE");
  const ScreenPlace landscape = knobPlace(l, (uint8_t)code, false);
  TEST_ASSERT_EQUAL_INT(SCREEN_COUNT - 1, landscape.count);   // CAT is portrait-only
  TEST_ASSERT_EQUAL_INT(2, landscape.pos);
  TEST_ASSERT_EQUAL_INT(0, knobPlace(l, (uint8_t)cat, false).pos);
  const ScreenPlace portrait = knobPlace(l, (uint8_t)cat, true);
  TEST_ASSERT_EQUAL_INT(SCREEN_COUNT - 4, portrait.count);    // CODE, SLEEP, RUN, TEXT
  TEST_ASSERT_EQUAL_INT(cat - 4 + 1, portrait.pos);
  TEST_ASSERT_EQUAL_INT(0, knobPlace(l, (uint8_t)code, true).pos);
}

// --- brightness --------------------------------------------------------------

static void test_brightness_variants_fit_both_orientations() {
  const uint8_t styles[] = {BRIGHTNESS_DOTS, BRIGHTNESS_GAUGE, BRIGHTNESS_NUMBER};
  for (uint8_t s : styles)
    for (auto &sz : SIZES)
      for (uint8_t level : {0, 3, 5}) {
        Canvas c(sz[0], sz[1]);
        drawBrightnessFeedback(c, s, level, 6, 1);
        TEST_ASSERT_TRUE_MESSAGE(c.writes > 0, "draws something");
        TEST_ASSERT_EQUAL_UINT_MESSAGE(0, c.outside, "inside the canvas");
        TEST_ASSERT_EQUAL_UINT_MESSAGE(0, c.clashes, "no text over text");
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.count(TYPE_MISSING_COLOR), "every glyph exists");
        // The gauge is a shape on the edge; text keeps the margins
        if (s != BRIGHTNESS_GAUGE) TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.inkInMargins(), "margins");
      }
}

static void test_dots_band_shows_the_level_in_lit_steps() {
  for (auto &sz : SIZES) {
    Canvas low(sz[0], sz[1]), high(sz[0], sz[1]);
    drawBrightnessFeedback(low, BRIGHTNESS_DOTS, 0, 6, 1);
    drawBrightnessFeedback(high, BRIGHTNESS_DOTS, 5, 6, 1);
    // Six steps of equal size: lit ones in text, the others in dim
    const int litLow = low.count(palette().text), litHigh = high.count(palette().text);
    TEST_ASSERT_TRUE(litHigh > litLow);
    // A bottom band: nothing above its top line
    TEST_ASSERT_TRUE(low.topmostWritten() >= sz[1] - 15);
  }
}

static void test_a_band_on_its_way_up_stays_inside() {
  for (auto &sz : SIZES)
    for (float k : {0.1f, 0.5f}) {
      Canvas c(sz[0], sz[1]);
      drawBrightnessFeedback(c, BRIGHTNESS_DOTS, 2, 6, k);
      TEST_ASSERT_EQUAL_UINT(0, c.outside);
      Canvas d(sz[0], sz[1]);
      drawKnobFeedback(d, KNOB_BAND, KnobView{"CLOCK", nullptr, nullptr, {3, 12}, 1, 0}, k);
      TEST_ASSERT_EQUAL_UINT(0, d.outside);
    }
}

// --- knob --------------------------------------------------------------------

// Every screen name of both languages, with the widest count
static void test_knob_variants_fit_both_orientations() {
  const uint8_t styles[] = {KNOB_BAND, KNOB_NEIGHBOURS, KNOB_DOTS};
  const Lang langs[] = {LANG_EN, LANG_FR};
  for (uint8_t s : styles)
    for (auto &sz : SIZES)
      for (Lang lang : langs)
        for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
          const Strings &t = STRINGS[lang];
          const char *name = t.*SCREEN_INFO[i].name;
          const char *prev = t.*SCREEN_INFO[(i + SCREEN_COUNT - 1) % SCREEN_COUNT].name;
          const char *next = t.*SCREEN_INFO[(i + 1) % SCREEN_COUNT].name;
          const KnobView v{name, prev, next, {SCREEN_COUNT, SCREEN_COUNT}, 1, 0};
          Canvas c(sz[0], sz[1]);
          drawKnobFeedback(c, s, v, 1);
          TEST_ASSERT_TRUE_MESSAGE(c.writes > 0, name);
          TEST_ASSERT_EQUAL_UINT_MESSAGE(0, c.outside, name);
          TEST_ASSERT_EQUAL_UINT_MESSAGE(0, c.clashes, name);
          TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.count(TYPE_MISSING_COLOR), name);
          // The neighbours are cut by the edges on purpose
          if (s != KNOB_NEIGHBOURS) TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.inkInMargins(), name);
        }
}

static void test_band_shows_name_and_count() {
  for (auto &sz : SIZES) {
    Canvas c(sz[0], sz[1]);
    drawKnobFeedback(c, KNOB_BAND, KnobView{"CLOCK", nullptr, nullptr, {4, 12}, 1, 0}, 1);
    TEST_ASSERT_TRUE(c.count(palette().text) > 0);     // the name
    TEST_ASSERT_TRUE(c.count(palette().label) > 0);    // "4/12"
    TEST_ASSERT_TRUE(c.topmostWritten() >= sz[1] - 15);
  }
}

static void test_dots_count_the_screens_that_are_on() {
  for (auto &sz : SIZES)
    for (int n : {1, 5, 12, (int)SCREEN_COUNT}) {
      Canvas c(sz[0], sz[1]);
      // Past the name's time: the dots alone
      drawKnobFeedback(c, KNOB_DOTS, KnobView{"CLOCK", nullptr, nullptr, {(int16_t)n, (int16_t)n}, 1, 1000}, 1);
      TEST_ASSERT_EQUAL_INT(1, c.count(palette().accent));
      TEST_ASSERT_EQUAL_INT(n - 1, c.count(palette().dim));
      TEST_ASSERT_EQUAL_UINT(0, c.outside);
    }
}

static void test_neighbours_portrait_stacks_without_sliding() {
  Canvas a(32, 64), b(32, 64);
  drawKnobFeedback(a, KNOB_NEIGHBOURS, KnobView{"CLOCK", "DATE", "WEATHER", {4, 12}, 1, 0}, 1);
  drawKnobFeedback(b, KNOB_NEIGHBOURS, KnobView{"CLOCK", "DATE", "WEATHER", {4, 12}, 1, 500}, 1);
  TEST_ASSERT_EQUAL_MEMORY(a.px, b.px, sizeof(a.px));    // nothing moves sideways in portrait
  TEST_ASSERT_EQUAL_INT(0, a.inkInMargins());
  Canvas c(64, 32), d(64, 32);                           // landscape slides in, then settles
  drawKnobFeedback(c, KNOB_NEIGHBOURS, KnobView{"CLOCK", "DATE", "WEATHER", {4, 12}, 1, 0}, 1);
  drawKnobFeedback(d, KNOB_NEIGHBOURS, KnobView{"CLOCK", "DATE", "WEATHER", {4, 12}, 1, 500}, 1);
  TEST_ASSERT_TRUE(memcmp(c.px, d.px, sizeof(c.px)) != 0);
}

// --- none --------------------------------------------------------------------

static void test_none_and_hidden_draw_nothing() {
  for (auto &sz : SIZES) {
    Canvas c(sz[0], sz[1]);
    drawBrightnessFeedback(c, BRIGHTNESS_NONE, 3, 6, 1);
    drawKnobFeedback(c, KNOB_NONE, KnobView{"CLOCK", "DATE", "WEATHER", {4, 12}, 1, 0}, 1);
    for (uint8_t s = 0; s < BRIGHTNESS_FEEDBACK_COUNT; s++) drawBrightnessFeedback(c, s, 3, 6, 0);
    for (uint8_t s = 0; s < KNOB_FEEDBACK_COUNT; s++)
      drawKnobFeedback(c, s, KnobView{"CLOCK", "DATE", "WEATHER", {4, 12}, 1, 0}, 0);
    TEST_ASSERT_EQUAL_UINT(0, c.writes);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_curve_rises_holds_and_falls);
  RUN_TEST(test_a_new_event_holds_again_without_a_new_rise);
  RUN_TEST(test_once_over_it_rises_again_and_survives_the_wrap);
  RUN_TEST(test_another_kind_rises_afresh);
  RUN_TEST(test_the_count_follows_the_screens_that_are_on);
  RUN_TEST(test_the_count_leaves_out_the_screens_that_do_not_fit);
  RUN_TEST(test_brightness_variants_fit_both_orientations);
  RUN_TEST(test_dots_band_shows_the_level_in_lit_steps);
  RUN_TEST(test_a_band_on_its_way_up_stays_inside);
  RUN_TEST(test_knob_variants_fit_both_orientations);
  RUN_TEST(test_band_shows_name_and_count);
  RUN_TEST(test_dots_count_the_screens_that_are_on);
  RUN_TEST(test_neighbours_portrait_stacks_without_sliding);
  RUN_TEST(test_none_and_hidden_draw_nothing);
  return UNITY_END();
}
