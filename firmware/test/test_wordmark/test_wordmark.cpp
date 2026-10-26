// SPDX-License-Identifier: AGPL-3.0-only
// The boot's "Carousel" wordmark: both bitmaps fit their panel, the landscape
// one is a single bit (lit or off, no grey), the portrait one is anti-aliased
// (several grey levels, not one), the canvas picks one line in landscape and
// two in portrait, the word is centred on x with its top at y, and each pixel
// takes its level times the light of the text role.
//
//   pio test -e native -f test_wordmark
#include <unity.h>

#include <cstring>
#include <set>

#include "../../src/boot_logic.cpp"
#include "../../src/palette.cpp"
#include "../../src/wordmark.cpp"
#include "../../src/wordmark_data.cpp"

// gallery.cpp's palette() picks by the hour and the brightness: the day one here
const Palette &palette() { return paletteFor(false); }

void setUp() {}
void tearDown() {}

// A canvas of any size that remembers every pixel and counts writes off its edges
class Canvas : public Adafruit_GFX {
 public:
  uint16_t px[64][64];
  unsigned outside = 0;
  Canvas(int16_t w, int16_t h) : Adafruit_GFX(w, h) { memset(px, 0, sizeof(px)); }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) { outside++; return; }
    px[y][x] = color;
  }
  // The lit pixels' bounding box, inclusive; false when nothing is lit
  bool box(int &x0, int &y0, int &x1, int &y1) const {
    x0 = y0 = 64;
    x1 = y1 = -1;
    for (int y = 0; y < height(); y++)
      for (int x = 0; x < width(); x++)
        if (px[y][x]) {
          if (x < x0) x0 = x;
          if (x > x1) x1 = x;
          if (y < y0) y0 = y;
          if (y > y1) y1 = y;
        }
    return x1 >= 0;
  }
};

// The landscape word holds on one line of the 64 px panel with a margin, at
// the cap height the owner chose (9 px, the l rising no higher, the round
// letters dipping one under the baseline); the portrait word on the 32 px
// side with two pixels each side
static void test_the_bitmaps_fit() {
  TEST_ASSERT_LESS_OR_EQUAL(60, WORDMARK_WIDE.w);
  TEST_ASSERT_GREATER_OR_EQUAL(40, WORDMARK_WIDE.w);
  TEST_ASSERT_GREATER_OR_EQUAL(9, WORDMARK_WIDE.h);
  TEST_ASSERT_LESS_OR_EQUAL(13, WORDMARK_WIDE.h);
  TEST_ASSERT_LESS_OR_EQUAL(28, WORDMARK_TALL.w);
  TEST_ASSERT_LESS_OR_EQUAL(24, WORDMARK_TALL.h);
}

// Every edge row and column of each bitmap holds ink: the generator trimmed it
static void assertTrimmed(const Wordmark &m) {
  bool top = false, bottom = false, left = false, right = false;
  for (int x = 0; x < m.w; x++) {
    top |= wordmarkLevel(m, x, 0) > 0;
    bottom |= wordmarkLevel(m, x, m.h - 1) > 0;
  }
  for (int y = 0; y < m.h; y++) {
    left |= wordmarkLevel(m, 0, y) > 0;
    right |= wordmarkLevel(m, m.w - 1, y) > 0;
  }
  TEST_ASSERT_TRUE(top && bottom && left && right);
}

static void test_the_bitmaps_are_trimmed() {
  assertTrimmed(WORDMARK_WIDE);
  assertTrimmed(WORDMARK_TALL);
}

// The landscape word is one bit: only off and full, no grey edge
static void test_the_landscape_word_is_one_bit() {
  const Wordmark &m = WORDMARK_WIDE;
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      const uint8_t v = wordmarkLevel(m, x, y);
      TEST_ASSERT_TRUE(v == 0 || v == WORDMARK_LEVELS - 1);
    }
}

// The portrait word is anti-aliased: the edges use several grey levels
// between off and full, and the strokes reach full light
static void test_the_portrait_word_is_anti_aliased() {
  const Wordmark &m = WORDMARK_TALL;
  std::set<uint8_t> used;
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      const uint8_t v = wordmarkLevel(m, x, y);
      TEST_ASSERT_LESS_THAN(WORDMARK_LEVELS, v);
      if (v) used.insert(v);
    }
  TEST_ASSERT_GREATER_OR_EQUAL(4, used.size());
  TEST_ASSERT_TRUE(used.count(WORDMARK_LEVELS - 1));
}

static void test_the_canvas_picks_the_word() {
  Canvas wide(64, 32), tall(32, 64);
  TEST_ASSERT_EQUAL_PTR(&WORDMARK_WIDE, &bootWordFor(wide));
  TEST_ASSERT_EQUAL_PTR(&WORDMARK_TALL, &bootWordFor(tall));
}

// Centred on x within half a pixel, its top row at y, nothing off the canvas
static void assertCentred(Canvas &c, int16_t x, int16_t y) {
  const Wordmark &m = bootWordFor(c);
  drawBootWord(c, x, y, 255);
  int x0, y0, x1, y1;
  TEST_ASSERT_TRUE(c.box(x0, y0, x1, y1));
  TEST_ASSERT_EQUAL(0, c.outside);
  TEST_ASSERT_EQUAL(m.w, x1 - x0 + 1);
  TEST_ASSERT_EQUAL(m.h, y1 - y0 + 1);
  TEST_ASSERT_EQUAL(y, y0);
  TEST_ASSERT_INT_WITHIN(1, 2 * x, x0 + x1 + 1);
}

static void test_the_word_is_centred_at_x_with_its_top_at_y() {
  Canvas wide(64, 32), tall(32, 64), off(64, 32);
  assertCentred(wide, 32, 11);
  assertCentred(tall, 16, 20);
  assertCentred(off, 30, 4);
}

// The pixel at full level takes the text role at the light given; a half-lit
// one (the portrait word's) less; light 0 writes nothing
static void test_each_pixel_takes_its_level_times_the_light() {
  const Wordmark &m = WORDMARK_WIDE;
  const int16_t left = 32 - m.w / 2;
  int fx = -1, fy = -1;
  for (int y = 0; y < m.h && fx < 0; y++)
    for (int x = 0; x < m.w && fx < 0; x++)
      if (wordmarkLevel(m, x, y) == WORDMARK_LEVELS - 1) fx = x, fy = y;
  TEST_ASSERT_TRUE(fx >= 0);
  const uint16_t text = paletteFor(false).text;

  Canvas full(64, 32);
  drawBootWord(full, 32, 0, 255);
  TEST_ASSERT_EQUAL_HEX16(text, full.px[fy][left + fx]);

  Canvas dim(64, 32);
  drawBootWord(dim, 32, 0, 128);
  TEST_ASSERT_EQUAL_HEX16(bootFaded(text, 128), dim.px[fy][left + fx]);

  Canvas dark(64, 32);
  drawBootWord(dark, 32, 0, 0);
  int x0, y0, x1, y1;
  TEST_ASSERT_FALSE(dark.box(x0, y0, x1, y1));

  const Wordmark &t = WORDMARK_TALL;
  int hx = -1, hy = -1;
  for (int y = 0; y < t.h && hx < 0; y++)
    for (int x = 0; x < t.w && hx < 0; x++) {
      const uint8_t v = wordmarkLevel(t, x, y);
      if (v && v < WORDMARK_LEVELS / 2) hx = x, hy = y;
    }
  TEST_ASSERT_TRUE(hx >= 0);
  Canvas tall(32, 64);
  drawBootWord(tall, 16, 0, 255);
  TEST_ASSERT_TRUE(tall.px[hy][16 - t.w / 2 + hx] != 0);
  TEST_ASSERT_TRUE(tall.px[hy][16 - t.w / 2 + hx] < text);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_the_bitmaps_fit);
  RUN_TEST(test_the_bitmaps_are_trimmed);
  RUN_TEST(test_the_landscape_word_is_one_bit);
  RUN_TEST(test_the_portrait_word_is_anti_aliased);
  RUN_TEST(test_the_canvas_picks_the_word);
  RUN_TEST(test_the_word_is_centred_at_x_with_its_top_at_y);
  RUN_TEST(test_each_pixel_takes_its_level_times_the_light);
  return UNITY_END();
}
