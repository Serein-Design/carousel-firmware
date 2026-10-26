// SPDX-License-Identifier: AGPL-3.0-only
// The line that fills while the knob is held: nothing for a short press
// (under 200 ms), a third of the way at 400 ms, the full width at the long
// press's 800 ms and staying full past it, gone once released; drawn on the
// bottom row only, in the accent, in both orientations.
//
//   pio test -e native -f test_hold
#include <unity.h>

#include <cstring>

#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"
#include "../../src/palette.cpp"
#include "../../src/reach_logic.cpp"
#include "../../src/hold.cpp"

// gallery.cpp's palette() knows the night hours; the day palette here
const Palette &palette() { return paletteFor(false); }

// A canvas of either orientation over a white screen, keeping every pixel
// and counting writes off its edges
static const uint16_t SCREEN = 0xFFFF;
class Canvas : public Adafruit_GFX {
 public:
  uint16_t px[64][64];
  unsigned outside = 0;
  Canvas(int16_t w, int16_t h) : Adafruit_GFX(w, h) {
    for (auto &row : px)
      for (auto &c : row) c = SCREEN;
  }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) { outside++; return; }
    px[y][x] = color;
  }
  int count(uint16_t color, int y) const {
    int n = 0;
    for (int x = 0; x < width(); x++) n += px[y][x] == color;
    return n;
  }
  // Pixels changed above the bottom row
  int changedAbove() const {
    int n = 0;
    for (int y = 0; y < height() - 1; y++)
      for (int x = 0; x < width(); x++) n += px[y][x] != SCREEN;
    return n;
  }
};

void setUp() {}
void tearDown() {}

static const int16_t SIZES[2][2] = {{64, 32}, {32, 64}};

// --- the fill ----------------------------------------------------------------

static void test_nothing_before_200_ms() {
  TEST_ASSERT_EQUAL_FLOAT(0, holdFill(0));
  TEST_ASSERT_EQUAL_FLOAT(0, holdFill(150));
  TEST_ASSERT_EQUAL_FLOAT(0, holdFill(HOLD_SHOW_MS));
}

static void test_fills_from_200_to_800_ms() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f / 3, holdFill(400));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, holdFill(500));
  TEST_ASSERT_EQUAL_FLOAT(1, holdFill(LONG_PRESS_MS));
}

static void test_stays_full_past_800_ms() {
  TEST_ASSERT_EQUAL_FLOAT(1, holdFill(801));
  TEST_ASSERT_EQUAL_FLOAT(1, holdFill(5000));
}

// Reusable for another hold: its own thresholds
static void test_other_thresholds() {
  TEST_ASSERT_EQUAL_FLOAT(0, holdFill(400, 2000, 500));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, holdFill(1250, 2000, 500));
  TEST_ASSERT_EQUAL_FLOAT(1, holdFill(2000, 2000, 500));
}

// --- the knob's time held ----------------------------------------------------

static void test_held_time_follows_the_press() {
  PressTimer p;
  p.update(false, 0);
  TEST_ASSERT_EQUAL_UINT32(0, p.heldMs(500));            // not pressed
  p.update(true, 1000);
  TEST_ASSERT_EQUAL_UINT32(400, p.heldMs(1400));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f / 3, holdFill(p.heldMs(1400)));
}

static void test_gone_after_a_short_release() {
  PressTimer p;
  p.update(false, 0);
  p.update(true, 1000);
  TEST_ASSERT_TRUE(holdFill(p.heldMs(1500)) > 0);
  TEST_ASSERT_EQUAL(Press::SHORT, p.update(false, 1500));
  TEST_ASSERT_EQUAL_UINT32(0, p.heldMs(1600));
  TEST_ASSERT_EQUAL_FLOAT(0, holdFill(p.heldMs(1600)));
}

static void test_full_and_held_after_800_ms() {
  PressTimer p;
  p.update(false, 0);
  p.update(true, 1000);
  TEST_ASSERT_EQUAL(Press::NONE, p.update(true, 1000 + LONG_PRESS_MS));
  TEST_ASSERT_EQUAL_FLOAT(1, holdFill(p.heldMs(1000 + LONG_PRESS_MS)));
  TEST_ASSERT_EQUAL(Press::NONE, p.update(true, 4000));
  TEST_ASSERT_EQUAL_FLOAT(1, holdFill(p.heldMs(4000)));
  TEST_ASSERT_EQUAL(Press::LONG, p.update(false, 4100));   // the release opens the card
  TEST_ASSERT_EQUAL_FLOAT(0, holdFill(p.heldMs(4200)));
}

// The knob held through the boot (setup, reset) fills nothing
static void test_no_line_for_a_boot_hold() {
  PressTimer p;
  p.update(true, 0);
  TEST_ASSERT_EQUAL_UINT32(0, p.heldMs(2000));
}

// --- the drawing -------------------------------------------------------------

static void test_draws_nothing_at_zero() {
  for (const auto &s : SIZES) {
    Canvas c(s[0], s[1]);
    drawHoldLine(c, 0);
    TEST_ASSERT_EQUAL(0, c.changedAbove());
    TEST_ASSERT_EQUAL(s[0], c.count(SCREEN, s[1] - 1));
  }
}

static void test_draws_the_bottom_row_only() {
  const Palette &p = palette();
  for (const auto &s : SIZES) {
    const int16_t w = s[0], bottom = s[1] - 1;
    const int third = (int)lroundf(w / 3.0f);
    Canvas part(w, s[1]);
    drawHoldLine(part, holdFill(400));
    TEST_ASSERT_EQUAL(0, part.changedAbove());
    TEST_ASSERT_EQUAL(0, part.outside);
    TEST_ASSERT_EQUAL(third, part.count(p.accent, bottom));
    TEST_ASSERT_EQUAL_HEX16(p.accent, part.px[bottom][0]);   // filled from the left
    TEST_ASSERT_EQUAL(w - third, part.count(p.off, bottom));  // the rest of the row dark

    Canvas full(w, s[1]);
    drawHoldLine(full, holdFill(LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(w, full.count(p.accent, bottom));
    TEST_ASSERT_EQUAL(0, full.changedAbove());
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_before_200_ms);
  RUN_TEST(test_fills_from_200_to_800_ms);
  RUN_TEST(test_stays_full_past_800_ms);
  RUN_TEST(test_other_thresholds);
  RUN_TEST(test_held_time_follows_the_press);
  RUN_TEST(test_gone_after_a_short_release);
  RUN_TEST(test_full_and_held_after_800_ms);
  RUN_TEST(test_no_line_for_a_boot_hold);
  RUN_TEST(test_draws_nothing_at_zero);
  RUN_TEST(test_draws_the_bottom_row_only);
  return UNITY_END();
}
