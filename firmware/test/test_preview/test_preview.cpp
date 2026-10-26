// SPDX-License-Identifier: AGPL-3.0-only
// The Stage preview: the frame the panel shows, as a 24-bit BMP the browser
// draws without a script, upright in either orientation, black while the
// sign-in code is up; and the gate every read-only browser file goes through.
//
//   pio test -e native -f test_preview
#include <unity.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/preview_logic.cpp"

void setUp() {}
void tearDown() {}

static uint32_t le32(const uint8_t *p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

// The unrotated frame, as the panel's copy holds it: row after row of 64
static std::vector<uint16_t> frame(uint16_t fill = 0) {
  return std::vector<uint16_t>(PREVIEW_W * PREVIEW_H, fill);
}

// The BMP's pixel at (x, y) counted from the top-left of the upright image,
// as B, G, R
static const uint8_t *pixel(const std::vector<uint8_t> &bmp, int x, int y) {
  const int w = (int)le32(&bmp[18]), h = (int)le32(&bmp[22]);
  return &bmp[54 + ((h - 1 - y) * w + x) * 3];
}

static void test_bmp_header_landscape_and_portrait() {
  TEST_ASSERT_EQUAL(6198, PREVIEW_BMP_SIZE);
  const std::vector<uint16_t> f = frame();
  for (uint8_t rotation = 0; rotation < 4; rotation++) {
    std::vector<uint8_t> bmp(PREVIEW_BMP_SIZE, 0xAA);
    previewBmp(f.data(), rotation, false, bmp.data());
    TEST_ASSERT_EQUAL('B', bmp[0]);
    TEST_ASSERT_EQUAL('M', bmp[1]);
    TEST_ASSERT_EQUAL(6198, le32(&bmp[2]));
    TEST_ASSERT_EQUAL(0, le32(&bmp[6]));
    TEST_ASSERT_EQUAL(54, le32(&bmp[10]));     // pixels right after the two headers
    TEST_ASSERT_EQUAL(40, le32(&bmp[14]));     // BITMAPINFOHEADER
    const bool upright = rotation & 1;
    TEST_ASSERT_EQUAL(upright ? 32 : 64, le32(&bmp[18]));
    TEST_ASSERT_EQUAL(upright ? 64 : 32, le32(&bmp[22]));   // positive: bottom-up
    TEST_ASSERT_EQUAL(1, le16(&bmp[26]));
    TEST_ASSERT_EQUAL(24, le16(&bmp[28]));
    TEST_ASSERT_EQUAL(0, le32(&bmp[30]));      // no compression
    TEST_ASSERT_EQUAL(6144, le32(&bmp[34]));
  }
}

static void test_colours_expand_565() {
  struct { uint16_t c; uint8_t b, g, r; } cases[] = {
    {0xF800, 0, 0, 255}, {0x07E0, 0, 255, 0}, {0x001F, 255, 0, 0}, {0xFFFF, 255, 255, 255},
    {0x0000, 0, 0, 0}, {0x8410, 132, 130, 132},   // half grey: 16, 32, 16 widened
  };
  for (const auto &c : cases) {
    std::vector<uint16_t> f = frame(c.c);
    std::vector<uint8_t> bmp(PREVIEW_BMP_SIZE);
    previewBmp(f.data(), 0, false, bmp.data());
    const uint8_t *p = pixel(bmp, 10, 5);
    TEST_ASSERT_EQUAL_HEX8(c.b, p[0]);
    TEST_ASSERT_EQUAL_HEX8(c.g, p[1]);
    TEST_ASSERT_EQUAL_HEX8(c.r, p[2]);
  }
}

// Adafruit GFX's GFXcanvas16::drawPixel: where a pixel drawn at (x, y) of
// the rotated canvas lands in its unrotated buffer
static void canvasDraw(std::vector<uint16_t> &buf, uint8_t rotation, int x, int y, uint16_t c) {
  const int W = PREVIEW_W, H = PREVIEW_H;
  int t;
  switch (rotation) {
    case 1: t = x; x = W - 1 - y; y = t; break;
    case 2: x = W - 1 - x; y = H - 1 - y; break;
    case 3: t = x; x = y; y = H - 1 - t; break;
  }
  buf[x + y * W] = c;
}

// Each pixel a colour of its own place, drawn through the canvas: read back
// upright, every one is where it was drawn
static void test_rotation_matches_the_canvas() {
  for (uint8_t rotation = 0; rotation < 4; rotation++) {
    const int w = rotation & 1 ? PREVIEW_H : PREVIEW_W, h = rotation & 1 ? PREVIEW_W : PREVIEW_H;
    std::vector<uint16_t> f = frame();
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) canvasDraw(f, rotation, x, y, (uint16_t)((x << 11) | (y << 5)));
    std::vector<uint8_t> bmp(PREVIEW_BMP_SIZE);
    previewBmp(f.data(), rotation, false, bmp.data());
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        const uint8_t *p = pixel(bmp, x, y);
        const uint16_t c = (uint16_t)((x << 11) | (y << 5));
        const uint8_t r5 = (c >> 11) & 31, g6 = (c >> 5) & 63;
        char msg[48];
        snprintf(msg, sizeof(msg), "rotation %u at %d,%d", rotation, x, y);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((r5 << 3) | (r5 >> 2), p[2], msg);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((g6 << 2) | (g6 >> 4), p[1], msg);
      }
  }
}

// The last row of the file is the top of the picture
static void test_rows_bottom_up() {
  std::vector<uint16_t> f = frame();
  for (int x = 0; x < PREVIEW_W; x++) f[x] = 0xF800;                              // top: red
  for (int x = 0; x < PREVIEW_W; x++) f[(PREVIEW_H - 1) * PREVIEW_W + x] = 0x001F;  // bottom: blue
  std::vector<uint8_t> bmp(PREVIEW_BMP_SIZE);
  previewBmp(f.data(), 0, false, bmp.data());
  const uint8_t *firstRow = &bmp[54];
  const uint8_t *lastRow = &bmp[54 + (PREVIEW_H - 1) * PREVIEW_W * 3];
  TEST_ASSERT_EQUAL_HEX8(255, firstRow[0]);   // blue in B
  TEST_ASSERT_EQUAL_HEX8(0, firstRow[2]);
  TEST_ASSERT_EQUAL_HEX8(0, lastRow[0]);
  TEST_ASSERT_EQUAL_HEX8(255, lastRow[2]);    // red in R
}

// The code on the panel must never reach a page: every pixel black, and the
// frame not even read (a null frame is fine)
static void test_black_while_the_code_shows() {
  std::vector<uint16_t> f = frame(0xFFFF);
  for (uint8_t rotation : {0, 1}) {
    std::vector<uint8_t> bmp(PREVIEW_BMP_SIZE, 0xAA);
    previewBmp(f.data(), rotation, true, bmp.data());
    TEST_ASSERT_EQUAL(6198, le32(&bmp[2]));
    for (size_t i = 54; i < PREVIEW_BMP_SIZE; i++) TEST_ASSERT_EQUAL_HEX8(0, bmp[i]);
    std::vector<uint8_t> none(PREVIEW_BMP_SIZE, 0xAA);
    previewBmp(nullptr, rotation, true, none.data());
    TEST_ASSERT_EQUAL_MEMORY(bmp.data(), none.data(), PREVIEW_BMP_SIZE);
  }
}

// 403 before 401 before 503: a foreign page learns nothing about the
// session, and a visitor without one nothing about the panel's state
static void test_read_gate() {
  TEST_ASSERT_EQUAL(READ_OK, readGate(true, false, true, false, true));
  TEST_ASSERT_EQUAL(READ_FORBIDDEN, readGate(false, false, true, false, true));
  TEST_ASSERT_EQUAL(READ_FORBIDDEN, readGate(true, true, true, false, true));
  TEST_ASSERT_EQUAL(READ_UNAUTHORIZED, readGate(true, false, false, false, true));
  TEST_ASSERT_EQUAL(READ_UNAVAILABLE, readGate(true, false, true, true, true));
  TEST_ASSERT_EQUAL(READ_UNAVAILABLE, readGate(true, false, true, false, false));
  // Combined: the first refusal in that order wins
  for (int bits = 0; bits < 32; bits++) {
    const bool hostOk = bits & 1, cross = bits & 2, cookie = bits & 4, locked = bits & 8,
               available = bits & 16;
    const ReadVerdict v = readGate(hostOk, cross, cookie, locked, available);
    if (!hostOk || cross)            TEST_ASSERT_EQUAL(READ_FORBIDDEN, v);
    else if (!cookie)                TEST_ASSERT_EQUAL(READ_UNAUTHORIZED, v);
    else if (locked || !available)   TEST_ASSERT_EQUAL(READ_UNAVAILABLE, v);
    else                             TEST_ASSERT_EQUAL(READ_OK, v);
  }
}

// An image file for a thumbnail goes through the same gate, with the lock
// and the storage area it needs: the visitor is judged before either, so a
// locked panel or a panel without storage tells nothing to a stranger
static void test_file_gate() {
  // Signed in: locked or no storage area, 503 either way
  TEST_ASSERT_EQUAL(READ_OK, readGate(true, false, true, false, true));
  TEST_ASSERT_EQUAL(READ_UNAVAILABLE, readGate(true, false, true, true, true));
  TEST_ASSERT_EQUAL(READ_UNAVAILABLE, readGate(true, false, true, false, false));
  TEST_ASSERT_EQUAL(READ_UNAVAILABLE, readGate(true, false, true, true, false));
  // Signed out: 401 whatever the lock or the storage, never the sign-in page
  for (int bits = 0; bits < 4; bits++)
    TEST_ASSERT_EQUAL(READ_UNAUTHORIZED, readGate(true, false, false, bits & 1, bits & 2));
  // Another site or an unknown host, with a session: 403 before anything else
  for (int bits = 0; bits < 4; bits++) {
    TEST_ASSERT_EQUAL(READ_FORBIDDEN, readGate(true, true, true, bits & 1, bits & 2));
    TEST_ASSERT_EQUAL(READ_FORBIDDEN, readGate(false, false, true, bits & 1, bits & 2));
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_bmp_header_landscape_and_portrait);
  RUN_TEST(test_colours_expand_565);
  RUN_TEST(test_rotation_matches_the_canvas);
  RUN_TEST(test_rows_bottom_up);
  RUN_TEST(test_black_while_the_code_shows);
  RUN_TEST(test_read_gate);
  RUN_TEST(test_file_gate);
  return UNITY_END();
}
