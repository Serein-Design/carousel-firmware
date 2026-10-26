// SPDX-License-Identifier: AGPL-3.0-only
// The frame push without the panel: only the pixels that differ from what the
// panel shows are written, an unchanged frame writes none, and a pixel drawn
// on the rotated off-screen frame lands where the panel's own rotation put it
// when screens drew straight into the panel.
//
//   pio test -e native -f test_frame_logic
#include <unity.h>

#include <cstring>

#include "../../src/frame_logic.cpp"

static const int16_t PW = 64, PH = 32;   // the panel, unrotated

struct Write { int16_t x, y; uint16_t color; };
static Write    writes[PW * PH + 1];
static unsigned writeCount = 0;

static void record(int16_t x, int16_t y, uint16_t color) {
  if (writeCount < sizeof(writes) / sizeof(writes[0])) writes[writeCount] = {x, y, color};
  writeCount++;
}

static uint16_t frame[PW * PH], shown[PW * PH];

void setUp() {
  memset(frame, 0, sizeof(frame));
  memset(shown, 0, sizeof(shown));
  writeCount = 0;
}
void tearDown() {}

static void test_unchanged_frame_writes_nothing() {
  for (int i = 0; i < PW * PH; i++) frame[i] = shown[i] = (uint16_t)(i * 37);
  TEST_ASSERT_EQUAL_UINT16(0, framePush(frame, shown, PW, PH, record));
  TEST_ASSERT_EQUAL_UINT(0, writeCount);
}

static void test_black_frame_on_black_panel_writes_nothing() {
  TEST_ASSERT_EQUAL_UINT16(0, framePush(frame, shown, PW, PH, record));
  TEST_ASSERT_EQUAL_UINT(0, writeCount);
}

static void test_only_changed_pixels_are_written_at_their_place() {
  frame[0]               = 0xF800;   // (0, 0)
  frame[5 * PW + 7]      = 0x07E0;   // (7, 5)
  frame[PW * PH - 1]     = 0x001F;   // (63, 31)
  TEST_ASSERT_EQUAL_UINT16(3, framePush(frame, shown, PW, PH, record));
  TEST_ASSERT_EQUAL_UINT(3, writeCount);
  TEST_ASSERT_EQUAL_INT16(0, writes[0].x);  TEST_ASSERT_EQUAL_INT16(0, writes[0].y);
  TEST_ASSERT_EQUAL_HEX16(0xF800, writes[0].color);
  TEST_ASSERT_EQUAL_INT16(7, writes[1].x);  TEST_ASSERT_EQUAL_INT16(5, writes[1].y);
  TEST_ASSERT_EQUAL_HEX16(0x07E0, writes[1].color);
  TEST_ASSERT_EQUAL_INT16(63, writes[2].x); TEST_ASSERT_EQUAL_INT16(31, writes[2].y);
  TEST_ASSERT_EQUAL_HEX16(0x001F, writes[2].color);
}

static void test_shadow_follows_the_frame() {
  for (int i = 0; i < PW * PH; i++) frame[i] = (uint16_t)(i * 13 + 1);
  framePush(frame, shown, PW, PH, record);
  TEST_ASSERT_EQUAL_MEMORY(frame, shown, sizeof(frame));
  // The same frame again, as every steady screen sends it: nothing written
  writeCount = 0;
  TEST_ASSERT_EQUAL_UINT16(0, framePush(frame, shown, PW, PH, record));
  TEST_ASSERT_EQUAL_UINT(0, writeCount);
}

static void test_a_whole_new_frame_writes_every_pixel() {
  for (int i = 0; i < PW * PH; i++) frame[i] = 0xFFFF;
  TEST_ASSERT_EQUAL_UINT16(PW * PH, framePush(frame, shown, PW, PH, record));
  TEST_ASSERT_EQUAL_UINT(PW * PH, writeCount);
}

// Where Adafruit's GFXcanvas16::drawPixel stores logical (x, y) for a rotation
// (Adafruit_GFX.cpp, GFXcanvas16::drawPixel), on a 64 x 32 canvas.
static int canvasIndex(uint8_t rotation, int16_t x, int16_t y) {
  int16_t t;
  switch (rotation) {
    case 1: t = x; x = PW - 1 - y; y = t; break;
    case 2: x = PW - 1 - x; y = PH - 1 - y; break;
  }
  return x + y * PW;
}

// Where MatrixPanel_I2S_DMA::transform sent logical (x, y) when screens drew
// into the panel itself, rotated (ESP32-HUB75-MatrixPanel-I2S-DMA.h, transform,
// for one pixel; _width and _height are the rotated dimensions).
static void panelPoint(uint8_t rotation, int16_t x, int16_t y, int16_t *px, int16_t *py) {
  const int16_t width = rotation & 1 ? PH : PW, height = rotation & 1 ? PW : PH;
  switch (rotation) {
    case 1: *px = height - 1 - y; *py = x; return;
    case 2: *px = width - 1 - x;  *py = height - 1 - y; return;
    default: *px = x; *py = y; return;
  }
}

// The gallery's two orientations: landscape upside down (2), portrait (1)
static void checkRotation(uint8_t rotation) {
  const int16_t width = rotation & 1 ? PH : PW, height = rotation & 1 ? PW : PH;
  for (int16_t y = 0; y < height; y++) {
    for (int16_t x = 0; x < width; x++) {
      setUp();
      frame[canvasIndex(rotation, x, y)] = 0xFFFF;
      TEST_ASSERT_EQUAL_UINT16(1, framePush(frame, shown, PW, PH, record));
      int16_t px, py;
      panelPoint(rotation, x, y, &px, &py);
      TEST_ASSERT_EQUAL_INT16(px, writes[0].x);
      TEST_ASSERT_EQUAL_INT16(py, writes[0].y);
    }
  }
}

static void test_portrait_lands_where_the_panel_rotation_put_it()  { checkRotation(1); }
static void test_landscape_lands_where_the_panel_rotation_put_it() { checkRotation(2); }

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_unchanged_frame_writes_nothing);
  RUN_TEST(test_black_frame_on_black_panel_writes_nothing);
  RUN_TEST(test_only_changed_pixels_are_written_at_their_place);
  RUN_TEST(test_shadow_follows_the_frame);
  RUN_TEST(test_a_whole_new_frame_writes_every_pixel);
  RUN_TEST(test_portrait_lands_where_the_panel_rotation_put_it);
  RUN_TEST(test_landscape_lands_where_the_panel_rotation_put_it);
  return UNITY_END();
}
