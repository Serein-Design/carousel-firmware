// SPDX-License-Identifier: AGPL-3.0-only
// The calibration screen of development builds: one swatch row per colour
// role, its name in the label face then a 20x5 block of the colour, and one
// line per face; c shows and hides it, a flips Chaud and Ambre whatever the
// hour. Nothing is drawn off the 64x32 frame.
//
//   pio test -e native -f test_calibration
#define SCREENS_TEST_CONSOLE
#include <unity.h>

#include <cstring>

#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"
#include "../../src/palette.cpp"
#include "../../src/calibration.cpp"

class Canvas : public Adafruit_GFX {
 public:
  uint16_t px[32][64];
  unsigned outside = 0;
  Canvas() : Adafruit_GFX(64, 32) { memset(px, 0, sizeof(px)); }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= 64 || y >= 32) { outside++; return; }
    px[y][x] = color;
  }
  // The columns of row y painted c, from x on
  int run(int x, int y, uint16_t c) const {
    int n = 0;
    while (x + n < 64 && px[y][x + n] == c) n++;
    return n;
  }
  bool has(uint16_t c) const {
    for (auto &row : px) for (uint16_t v : row) if (v == c) return true;
    return false;
  }
};

void setUp() {}
void tearDown() {}

static void test_one_swatch_row_per_role() {
  for (bool night : {false, true}) {
    const Palette &p = paletteFor(night);
    const uint16_t roles[] = {p.text, p.label, p.accent, p.alert, p.dim, p.ghost};
    Canvas c;
    drawCalibration(c, p, false);
    TEST_ASSERT_EQUAL_UINT(0, c.outside);
    for (int i = 0; i < 6; i++) {
      // the block: 20 columns, 5 rows, at the right margin
      const int y = 1 + i * 5;
      for (int r = 0; r < 5; r++) TEST_ASSERT_EQUAL_INT(20, c.run(42, y + r, roles[i]));
      // the name, left of it, in the same colour
      int named = 0;
      for (int r = 0; r < 5; r++) for (int x = 2; x < 40; x++) named += c.px[y + r][x] == roles[i];
      TEST_ASSERT_GREATER_THAN_INT(0, named);
    }
  }
}

static void test_one_line_per_face() {
  const Palette &p = paletteFor(false);
  Canvas c;
  drawCalibration(c, p, true);
  TEST_ASSERT_EQUAL_UINT(0, c.outside);
  // The label line (rows 2-6) in the label colour, the body line (rows 9-15)
  // and the digits (rows 18-30) in the text colour, nothing at the margins
  int label = 0, body = 0, digits = 0;
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 64; x++) {
      const uint16_t v = c.px[y][x];
      if (!v) continue;
      TEST_ASSERT_TRUE(x >= 2 && x <= 61);
      if (y >= 2 && y <= 6) label += v == p.label;
      else if (y >= 9 && y <= 15) body += v == p.text;
      else if (y >= 18 && y <= 30) digits += v == p.text;
      else TEST_FAIL_MESSAGE("a pixel between the lines");
    }
  TEST_ASSERT_GREATER_THAN_INT(0, label);
  TEST_ASSERT_GREATER_THAN_INT(0, body);
  TEST_ASSERT_GREATER_THAN_INT(0, digits);
  TEST_ASSERT_FALSE(c.has(TYPE_MISSING_COLOR));   // every character has its glyph
}

static void test_console_letters() {
  TEST_ASSERT_FALSE(calibrationShown());
  calibrationToggle();
  TEST_ASSERT_TRUE(calibrationShown());
  TEST_ASSERT_EQUAL_PTR(&paletteFor(false), &calibrationPalette(false));   // Chaud first
  TEST_ASSERT_EQUAL_PTR(&paletteFor(false, true), &calibrationPalette(true));   // as dim as the panel
  calibrationFlipPalette();
  TEST_ASSERT_EQUAL_PTR(&paletteFor(true), &calibrationPalette(false));
  calibrationFlipPalette();
  TEST_ASSERT_EQUAL_PTR(&paletteFor(false), &calibrationPalette(false));
  calibrationToggle();
  TEST_ASSERT_FALSE(calibrationShown());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_one_swatch_row_per_role);
  RUN_TEST(test_one_line_per_face);
  RUN_TEST(test_console_letters);
  return UNITY_END();
}
