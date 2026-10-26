// SPDX-License-Identifier: AGPL-3.0-only
// The GIF decoder against our fixtures and against broken files: logical
// screens, frame delays, transparency, disposal, interlace, every truncation,
// seeded byte mutations, frame rectangles, code sizes, the LZW table at its
// limits, blocks and reader errors. Uploaded GIFs are untrusted, so every run
// is under AddressSanitizer: each file is a heap copy of its exact size and
// each decoder its own heap block, so a read or write past either fails the
// test.
//
//   pio test -e native -f test_gif
#include <unity.h>

#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include <Preferences.h>
#include "../../src/i18n.cpp"
#include "../../src/content_logic.cpp"
#include "../../src/gif_decoder.cpp"
#include "fixtures.h"

bool panelLocked() { return false; }   // settings.cpp's, never locked here

void setUp() {}
void tearDown() {}

typedef std::vector<uint8_t> Bytes;

static const uint16_t BLACK = 0x0000, RED = 0xF800, GREEN = 0x07E0, BLUE = 0x001F,
                      WHITE = 0xFFFF;

// A file in a heap block of its exact size; can fail its k-th read (-1), end
// early, or refuse to seek
struct MemReader : GifReader {
  uint8_t *data;
  int32_t size, pos = 0;
  int reads = 0, failAt = 0;   // failAt: 1-based read that returns -1, 0 = never
  bool seekFails = false;
  MemReader(const uint8_t *d, size_t n) : data(new uint8_t[n ? n : 1]), size((int32_t)n) {
    if (n) memcpy(data, d, n);
  }
  explicit MemReader(const Bytes &b) : MemReader(b.data(), b.size()) {}
  ~MemReader() override { delete[] data; }
  int32_t read(uint8_t *buf, int32_t len) override {
    if (++reads == failAt) return -1;
    int32_t n = size - pos < len ? size - pos : len;
    if (n < 0) n = 0;
    if (n) memcpy(buf, data + pos, n);
    pos += n;
    return n;
  }
  bool seek(int32_t p) override {
    if (seekFails || p < 0 || p > size) return false;
    pos = p;
    return true;
  }
};

static GifDecoder *make() {
  GifDecoder *d = new (std::nothrow) GifDecoder;
  TEST_ASSERT_NOT_NULL(d);
  return d;
}

static uint16_t px(const GifDecoder *d, int x, int y) { return d->canvas()[y * d->width() + x]; }

static Bytes canvasOf(const GifDecoder *d) {
  const uint8_t *c = reinterpret_cast<const uint8_t *>(d->canvas());
  return Bytes(c, c + 2 * d->width() * d->height());
}

static bool allBlack(const GifDecoder *d) {
  for (int i = 0; i < d->width() * d->height(); i++)
    if (d->canvas()[i]) return false;
  return true;
}

static size_t find(const uint8_t *data, size_t n, const uint8_t *pat, size_t m) {
  for (size_t i = 0; i + m <= n; i++)
    if (memcmp(data + i, pat, m) == 0) return i;
  return n;
}

static Bytes animated() { return Bytes(GIF_ANIMATED, GIF_ANIMATED + sizeof GIF_ANIMATED); }

static const uint8_t DESC1[] = {0x2c, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x20, 0x00, 0x00};
static const uint8_t DESC2[] = {0x2c, 0x08, 0x00, 0x00, 0x00, 0x38, 0x00, 0x20, 0x00, 0x87};
static const uint8_t GCE[] = {0x21, 0xf9, 0x04};
static const size_t PALETTE_END = 13 + 768;   // GIF_ANIMATED: header and global palette

static size_t at(const Bytes &b, const uint8_t *pat, size_t m) {
  const size_t i = find(b.data(), b.size(), pat, m);
  TEST_ASSERT_TRUE(i < b.size());
  return i;
}

// Plays a file to its end: up to 16 frames, the canvas after each
struct Run {
  bool opened = false;
  GifStatus last = GIF_ERROR;
  GifError error = GE_NONE;
  int frames = 0;
  std::vector<uint32_t> delays;
  std::vector<Bytes> canvases;
  Bytes final;
};

static Run play(const Bytes &file) {
  Run run;
  MemReader r(file);
  GifDecoder *d = make();
  run.opened = d->open(r);
  run.last = GIF_ERROR;
  if (run.opened)
    for (int f = 0; f < 16; f++) {
      uint32_t ms = 0;
      run.last = d->next(&ms);
      if (run.last != GIF_FRAME) break;
      run.delays.push_back(ms);
      run.canvases.push_back(canvasOf(d));
    }
  run.error = d->error();
  run.frames = d->frames();
  run.final = canvasOf(d);
  delete d;
  return run;
}

// --- Fixtures ------------------------------------------------------------------

static void test_animated_plays() {
  MemReader r(GIF_ANIMATED, sizeof GIF_ANIMATED);
  GifDecoder *d = make();
  TEST_ASSERT_TRUE(d->open(r));
  TEST_ASSERT_EQUAL(64, d->width());
  TEST_ASSERT_EQUAL(32, d->height());
  uint32_t ms = 1;
  TEST_ASSERT_EQUAL(GIF_FRAME, d->next(&ms));
  TEST_ASSERT_EQUAL_UINT32(0, ms);
  TEST_ASSERT_EQUAL_UINT32(100, gifFrameDelay((int)ms));
  TEST_ASSERT_EQUAL_HEX16(GREEN, px(d, 8, 0));
  TEST_ASSERT_EQUAL_HEX16(BLACK, px(d, 0, 0));   // index 0 transparent over the black canvas
  const Bytes first = canvasOf(d);
  TEST_ASSERT_EQUAL(GIF_FRAME, d->next(&ms));
  TEST_ASSERT_EQUAL_UINT32(50, ms);
  TEST_ASSERT_EQUAL_HEX16(BLUE, px(d, 8, 0));     // frame 2, local palette
  TEST_ASSERT_EQUAL_HEX16(BLACK, px(d, 7, 31));   // frame 2 starts at x = 8
  TEST_ASSERT_EQUAL(GIF_END, d->next(&ms));
  TEST_ASSERT_EQUAL(GIF_END, d->next(&ms));       // sticky
  TEST_ASSERT_EQUAL(2, d->frames());
  TEST_ASSERT_TRUE(d->rewind());
  TEST_ASSERT_EQUAL(0, d->frames());
  TEST_ASSERT_TRUE(allBlack(d));
  TEST_ASSERT_EQUAL(GIF_FRAME, d->next(&ms));
  TEST_ASSERT_EQUAL_MEMORY(first.data(), canvasOf(d).data(), first.size());
  delete d;
}

static void test_portrait_still() {
  TEST_ASSERT_EQUAL_HEX8(0x40, GIF_PORTRAIT[25 + 9]);   // interlaced: keeps that path tested
  MemReader r(GIF_PORTRAIT, sizeof GIF_PORTRAIT);
  GifDecoder *d = make();
  TEST_ASSERT_TRUE(d->open(r));
  TEST_ASSERT_EQUAL(32, d->width());
  TEST_ASSERT_EQUAL(64, d->height());
  uint32_t ms;
  TEST_ASSERT_EQUAL(GIF_FRAME, d->next(&ms));
  TEST_ASSERT_EQUAL(GIF_END, d->next(&ms));
  TEST_ASSERT_EQUAL(1, d->frames());
  const uint16_t colour[] = {BLACK, RED, GREEN, BLUE};
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 32; x++) TEST_ASSERT_EQUAL_HEX16(colour[1 + (x / 4 + y / 4) % 3], px(d, x, y));
  delete d;
}

static void test_interlace_rows() {
  for (int h = 1; h <= 64; h++) {
    std::vector<int> seen(h, 0);
    for (int r = 0; r < h; r++) {
      const int row = gifInterlacedRow(r, h);
      TEST_ASSERT_TRUE(row >= 0 && row < h);
      seen[row]++;
    }
    for (int i = 0; i < h; i++) TEST_ASSERT_EQUAL(1, seen[i]);
  }
}

static void test_wrong_logical_screen() {
  MemReader wide(GIF_WIDE, sizeof GIF_WIDE);
  GifDecoder *d = make();
  TEST_ASSERT_FALSE(d->open(wide));
  TEST_ASSERT_EQUAL(GE_SIZE, d->error());
  uint32_t ms;
  TEST_ASSERT_EQUAL(GIF_ERROR, d->next(&ms));
  const uint16_t sizes[][2] = {{64, 64}, {32, 32}, {0, 0}, {65535, 65535}};
  for (const auto &s : sizes) {
    Bytes b = animated();
    b[6] = s[0] & 0xFF; b[7] = s[0] >> 8; b[8] = s[1] & 0xFF; b[9] = s[1] >> 8;
    MemReader r(b);
    TEST_ASSERT_FALSE(d->open(r));
    TEST_ASSERT_EQUAL(GE_SIZE, d->error());
  }
  for (uint8_t v : {(uint8_t)'7', (uint8_t)'9'}) {
    Bytes b = animated();
    b[4] = v; b[5] = 'b';
    MemReader r(b);
    TEST_ASSERT_FALSE(d->open(r));
    TEST_ASSERT_EQUAL(GE_SIGNATURE, d->error());
  }
  delete d;
}

// --- Broken files ----------------------------------------------------------------

static void test_truncations() {
  const Run whole = play(animated());
  TEST_ASSERT_EQUAL(GIF_END, whole.last);
  TEST_ASSERT_EQUAL(2, (int)whole.canvases.size());
  const Bytes &afterFirst = whole.canvases[0];
  const size_t frame2 = at(animated(), DESC2, sizeof DESC2) - 8;   // its graphic control
  for (size_t n = 0; n <= sizeof GIF_ANIMATED; n++) {
    const Run run = play(Bytes(GIF_ANIMATED, GIF_ANIMATED + n));
    TEST_ASSERT_EQUAL_MESSAGE(n == sizeof GIF_ANIMATED, run.last == GIF_END, "END only when whole");
    if (n == sizeof GIF_ANIMATED) continue;
    TEST_ASSERT_EQUAL(GIF_ERROR, run.last);
    TEST_ASSERT_EQUAL(GE_TRUNCATED, run.error);
    if (n >= frame2 && n < sizeof GIF_ANIMATED - 1) {   // the cut is inside frame 2
      TEST_ASSERT_EQUAL(1, run.frames);
      TEST_ASSERT_EQUAL_MEMORY(afterFirst.data(), run.final.data(), afterFirst.size());
    }
    if (n < frame2) {
      TEST_ASSERT_EQUAL(0, run.frames);
      for (uint8_t c : run.final) TEST_ASSERT_EQUAL_HEX8(0, c);
    }
  }
  for (size_t n = 0; n <= sizeof GIF_PORTRAIT; n++) {
    const Run run = play(Bytes(GIF_PORTRAIT, GIF_PORTRAIT + n));
    TEST_ASSERT_EQUAL(n == sizeof GIF_PORTRAIT, run.last == GIF_END);
    if (n < sizeof GIF_PORTRAIT) TEST_ASSERT_EQUAL(GE_TRUNCATED, run.error);
  }
}

// Our own generator, so a failure replays by its iteration on any OS
static uint32_t rngState = 42;
static uint32_t xorshift32() {
  uint32_t x = rngState;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  return rngState = x;
}

// Offsets of the data bytes of the image sub-blocks that start at `from`
static void dataBytes(const Bytes &b, size_t from, std::vector<size_t> &out) {
  size_t p = from;
  while (p < b.size() && b[p]) {
    for (size_t i = 1; i <= b[p]; i++) out.push_back(p + i);
    p += b[p] + 1;
  }
}

struct Tally { int opened = 0, failed = 0, ended = 0; };

static void playMutant(const Bytes &file, Tally &t) {
  MemReader r(file);
  GifDecoder *d = make();
  if (!d->open(r)) {
    t.failed++;
    delete d;
    return;
  }
  t.opened++;
  bool failed = false, ended = false;
  for (int loop = 0; loop < 2; loop++) {
    for (int f = 0; f < 16; f++) {
      uint32_t ms;
      const GifStatus s = d->next(&ms);
      TEST_ASSERT_TRUE(d->frames() <= f + 1);
      if (s == GIF_ERROR) failed = true;
      if (s == GIF_END) ended = true;
      if (s != GIF_FRAME) break;
    }
    if (loop == 0 && !d->rewind()) break;
  }
  t.failed += failed;
  t.ended += ended;
  delete d;
}

static void mutate(Bytes &b, const std::vector<size_t> *where) {
  const int flips = 1 + xorshift32() % 8;
  for (int k = 0; k < flips; k++) {
    const size_t i = where ? (*where)[xorshift32() % where->size()] : xorshift32() % b.size();
    b[i] ^= (uint8_t)(1 + xorshift32() % 255);
  }
}

static void test_mutations() {
  rngState = 42;
  const Bytes base = animated();
  std::vector<size_t> lzw;
  dataBytes(base, at(base, DESC1, sizeof DESC1) + 11, lzw);
  dataBytes(base, at(base, DESC2, sizeof DESC2) + 10 + 768 + 1, lzw);
  TEST_ASSERT_TRUE(lzw.size() > 500);
  const Bytes portrait(GIF_PORTRAIT, GIF_PORTRAIT + sizeof GIF_PORTRAIT);
  Tally anywhere, inLzw, still;
  for (int i = 0; i < 2000; i++) { Bytes b = base; mutate(b, nullptr); playMutant(b, anywhere); }
  for (int i = 0; i < 5000; i++) { Bytes b = base; mutate(b, &lzw); playMutant(b, inLzw); }
  for (int i = 0; i < 2000; i++) { Bytes b = portrait; mutate(b, nullptr); playMutant(b, still); }
  char msg[160];
  snprintf(msg, sizeof msg, "opened/failed/ended: anywhere %d/%d/%d, lzw %d/%d/%d, portrait %d/%d/%d",
           anywhere.opened, anywhere.failed, anywhere.ended, inLzw.opened, inLzw.failed,
           inLzw.ended, still.opened, still.failed, still.ended);
  TEST_MESSAGE(msg);
  for (const Tally *t : {&anywhere, &inLzw, &still}) {
    TEST_ASSERT_TRUE(t->opened > 0);
    TEST_ASSERT_TRUE(t->failed > 0);
    TEST_ASSERT_TRUE(t->ended > 0);
  }
}

static void test_frame_rects() {
  const Run whole = play(animated());
  const Bytes &afterFirst = whole.canvases[0];
  // descriptor field offset (x 1, y 3, w 5, h 7) and value
  const uint16_t patches[][2] = {{1, 16}, {3, 1}, {5, 57}, {7, 33}, {1, 0xFFFF},
                                 {5, 0xFFFF}, {1, 0x8000}, {3, 0x8000}, {5, 0}, {7, 0}};
  for (const auto &p : patches) {
    Bytes b = animated();
    const size_t i = at(b, DESC2, sizeof DESC2) + p[0];
    b[i] = p[1] & 0xFF; b[i + 1] = p[1] >> 8;
    MemReader r(b);
    GifDecoder *d = make();
    TEST_ASSERT_TRUE(d->open(r));
    uint32_t ms;
    TEST_ASSERT_EQUAL(GIF_FRAME, d->next(&ms));
    TEST_ASSERT_EQUAL(GIF_ERROR, d->next(&ms));
    TEST_ASSERT_EQUAL(GE_FRAME_RECT, d->error());
    TEST_ASSERT_EQUAL_MEMORY(afterFirst.data(), canvasOf(d).data(), afterFirst.size());
    delete d;
  }
}

static void test_frame_larger_than_canvas() {
  Bytes b(GIF_WIDE, GIF_WIDE + sizeof GIF_WIDE);
  b[6] = 64; b[7] = 0; b[8] = 32; b[9] = 0;
  MemReader r(b);
  GifDecoder *d = make();
  TEST_ASSERT_TRUE(d->open(r));
  uint32_t ms;
  TEST_ASSERT_EQUAL(GIF_ERROR, d->next(&ms));
  TEST_ASSERT_EQUAL(GE_FRAME_RECT, d->error());
  TEST_ASSERT_TRUE(allBlack(d));
  delete d;
}

static void test_code_sizes() {
  for (uint8_t v : {0, 1, 9, 12, 255}) {
    Bytes b = animated();
    b[at(b, DESC1, sizeof DESC1) + 10] = v;
    MemReader r(b);
    GifDecoder *d = make();
    TEST_ASSERT_TRUE(d->open(r));
    uint32_t ms;
    TEST_ASSERT_EQUAL(GIF_ERROR, d->next(&ms));
    TEST_ASSERT_EQUAL(GE_CODE_SIZE, d->error());
    TEST_ASSERT_TRUE(allBlack(d));
    delete d;
  }
  GifLzw *lzw = new (std::nothrow) GifLzw;
  TEST_ASSERT_NOT_NULL(lzw);
  for (int m = -1; m <= 13; m++) TEST_ASSERT_EQUAL(m >= 2 && m <= 8, lzw->begin(m));
  delete lzw;
}

// --- The code table --------------------------------------------------------------

static void test_lzw_table_overflow() {
  const uint32_t SIZE = 64 * 1024, CAP = SIZE - 1024, FROM = CAP - 3000;
  uint8_t *out = new uint8_t[SIZE];
  memset(out, 0xAA, SIZE);
  GifLzw *lzw = new (std::nothrow) GifLzw;
  TEST_ASSERT_NOT_NULL(lzw);
  TEST_ASSERT_TRUE(lzw->begin(2));
  uint32_t n = 0;
  TEST_ASSERT_EQUAL(LZW_OK, lzw->push(4, out, &n, CAP));   // clear
  TEST_ASSERT_EQUAL_UINT32(0, n);
  TEST_ASSERT_EQUAL(LZW_OK, lzw->push(1, out, &n, CAP));
  TEST_ASSERT_EQUAL_UINT32(1, n);
  // Each push defines the code it sends: its string is the last one plus one
  // byte, all 1s, up to 4 091 bytes for code 4 095 (the stack holds 4 096)
  while (lzw->size() < LZW_MAX_CODES) {
    const int code = lzw->size();
    const uint32_t len = code - 4;
    n = FROM;
    TEST_ASSERT_EQUAL(LZW_OK, lzw->push(code, out, &n, CAP));
    TEST_ASSERT_EQUAL_UINT32(FROM + len < CAP ? FROM + len : CAP, n);   // cut at cap
    TEST_ASSERT_EQUAL(code + 1, lzw->size());
  }
  for (uint32_t i = FROM; i < CAP; i++) TEST_ASSERT_EQUAL_HEX8(1, out[i]);
  for (uint32_t i = CAP; i < SIZE; i++) TEST_ASSERT_EQUAL_HEX8(0xAA, out[i]);
  TEST_ASSERT_EQUAL(12, lzw->width());
  for (int i = 0; i < 2000; i++) {
    n = 0;
    TEST_ASSERT_EQUAL(LZW_OK, lzw->push(4095, out, &n, CAP));
    TEST_ASSERT_EQUAL_UINT32(4091, n);
    TEST_ASSERT_EQUAL(LZW_MAX_CODES, lzw->size());   // nothing added past the table
  }
  for (uint32_t i = CAP; i < SIZE; i++) TEST_ASSERT_EQUAL_HEX8(0xAA, out[i]);
  n = 0;
  TEST_ASSERT_EQUAL(LZW_BAD, lzw->push(4096, out, &n, CAP));
  TEST_ASSERT_EQUAL(LZW_BAD, lzw->push(65535, out, &n, CAP));
  TEST_ASSERT_EQUAL(LZW_OK, lzw->push(4, out, &n, CAP));
  TEST_ASSERT_EQUAL(6, lzw->size());
  TEST_ASSERT_EQUAL(3, lzw->width());
  delete lzw;
  delete[] out;
}

static void test_lzw_codes() {
  GifLzw *lzw = new (std::nothrow) GifLzw;
  TEST_ASSERT_NOT_NULL(lzw);
  uint8_t out[16];
  uint32_t n = 0;
  TEST_ASSERT_TRUE(lzw->begin(2));
  TEST_ASSERT_EQUAL(3, lzw->width());
  TEST_ASSERT_EQUAL(6, lzw->size());
  for (uint16_t c : {4, 1, 6, 2}) TEST_ASSERT_EQUAL(LZW_OK, lzw->push(c, out, &n, sizeof out));
  TEST_ASSERT_EQUAL(LZW_END, lzw->push(5, out, &n, sizeof out));
  const uint8_t want[] = {1, 1, 1, 2};
  TEST_ASSERT_EQUAL_UINT32(4, n);
  TEST_ASSERT_EQUAL_MEMORY(want, out, 4);
  TEST_ASSERT_EQUAL(8, lzw->size());
  TEST_ASSERT_EQUAL(4, lzw->width());               // widened at 1 << 3

  // a code above the next free one
  TEST_ASSERT_TRUE(lzw->begin(2));
  n = 0;
  TEST_ASSERT_EQUAL(LZW_OK, lzw->push(4, out, &n, sizeof out));
  TEST_ASSERT_EQUAL(LZW_OK, lzw->push(3, out, &n, sizeof out));
  TEST_ASSERT_EQUAL(LZW_BAD, lzw->push(7, out, &n, sizeof out));

  // the first code after a clear that is not a literal
  TEST_ASSERT_TRUE(lzw->begin(2));
  n = 0;
  TEST_ASSERT_EQUAL(LZW_OK, lzw->push(4, out, &n, sizeof out));
  TEST_ASSERT_EQUAL(LZW_BAD, lzw->push(6, out, &n, sizeof out));
  TEST_ASSERT_TRUE(lzw->begin(2));
  TEST_ASSERT_EQUAL(LZW_BAD, lzw->push(6, out, &n, sizeof out));   // no clear sent at all

  // a clear in the middle restarts the table
  TEST_ASSERT_TRUE(lzw->begin(2));
  n = 0;
  for (uint16_t c : {4, 1, 6, 2}) TEST_ASSERT_EQUAL(LZW_OK, lzw->push(c, out, &n, sizeof out));
  TEST_ASSERT_EQUAL(8, lzw->size());
  TEST_ASSERT_EQUAL(LZW_OK, lzw->push(4, out, &n, sizeof out));
  TEST_ASSERT_EQUAL(6, lzw->size());
  TEST_ASSERT_EQUAL(3, lzw->width());
  TEST_ASSERT_EQUAL(LZW_BAD, lzw->push(7, out, &n, sizeof out));   // 7 was defined before
  TEST_ASSERT_TRUE(lzw->begin(2));
  n = 0;
  for (uint16_t c : {4, 1, 6, 4, 3, 6}) TEST_ASSERT_EQUAL(LZW_OK, lzw->push(c, out, &n, sizeof out));
  const uint8_t again[] = {1, 1, 1, 3, 3, 3};          // 6 now means 3 3, not 1 1
  TEST_ASSERT_EQUAL_UINT32(6, n);
  TEST_ASSERT_EQUAL_MEMORY(again, out, 6);
  delete lzw;
}

// --- Composition -----------------------------------------------------------------

static void test_disposal_and_transparency() {
  struct Case { uint8_t flags; uint16_t after1, after2; };
  const Case cases[] = {{0x05, BLACK, BLACK},    // keep, index 0 transparent
                        {0x04, WHITE, WHITE},    // keep, opaque
                        {0x08, WHITE, BLACK},    // disposal 2: frame 1's rectangle cleared
                        {0x0C, WHITE, WHITE}};   // disposal 3 kept as 1
  for (const Case &c : cases) {
    Bytes b = animated();
    b[13] = b[14] = b[15] = 0xFF;                // global entry 0 white
    b[at(b, GCE, sizeof GCE) + 3] = c.flags;
    MemReader r(b);
    GifDecoder *d = make();
    TEST_ASSERT_TRUE(d->open(r));
    uint32_t ms;
    TEST_ASSERT_EQUAL(GIF_FRAME, d->next(&ms));
    TEST_ASSERT_EQUAL_HEX16(c.after1, px(d, 0, 0));
    TEST_ASSERT_EQUAL(GIF_FRAME, d->next(&ms));
    TEST_ASSERT_EQUAL_HEX16(c.after2, px(d, 0, 0));
    TEST_ASSERT_EQUAL(GIF_END, d->next(&ms));
    delete d;
  }
}

static void test_blocks() {
  const Run original = play(animated());
  const uint8_t comment[] = {0x21, 0xfe, 0x05, 'h', 'e', 'l', 'l', 'o', 0x00};
  const uint8_t app[] = {0x21, 0xff, 0x0b, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', '1', '2', '3',
                         0x03, 0x01, 0x02, 0x03, 0x00};
  const uint8_t text[] = {0x21, 0x01, 0x0c, 0, 0, 0, 0, 8, 0, 8, 0, 4, 4, 1, 0,
                          0x04, 'T', 'E', 'X', 'T', 0x00};
  Bytes withBlocks = animated();
  withBlocks.insert(withBlocks.begin() + PALETTE_END, text, text + sizeof text);
  withBlocks.insert(withBlocks.begin() + PALETTE_END, app, app + sizeof app);
  withBlocks.insert(withBlocks.begin() + PALETTE_END, comment, comment + sizeof comment);
  const Run skipped = play(withBlocks);
  TEST_ASSERT_EQUAL(GIF_END, skipped.last);
  TEST_ASSERT_TRUE(original.delays == skipped.delays);
  TEST_ASSERT_TRUE(original.canvases == skipped.canvases);

  Bytes unknown = animated();
  unknown.insert(unknown.begin() + PALETTE_END, 0x99);
  Run run = play(unknown);
  TEST_ASSERT_TRUE(run.opened);
  TEST_ASSERT_EQUAL(GIF_ERROR, run.last);
  TEST_ASSERT_EQUAL(GE_BLOCK, run.error);

  Bytes shortControl = animated();
  shortControl[at(shortControl, GCE, sizeof GCE) + 2] = 3;
  run = play(shortControl);
  TEST_ASSERT_EQUAL(GIF_ERROR, run.last);
  TEST_ASSERT_EQUAL(GE_BLOCK, run.error);

  Bytes noPalette = animated();
  noPalette[10] &= 0x7F;
  noPalette.erase(noPalette.begin() + 13, noPalette.begin() + PALETTE_END);
  run = play(noPalette);
  TEST_ASSERT_TRUE(run.opened);
  TEST_ASSERT_EQUAL(GIF_ERROR, run.last);
  TEST_ASSERT_EQUAL(GE_PALETTE, run.error);
  TEST_ASSERT_EQUAL(0, run.frames);
}

static void test_no_frame() {
  Bytes b(GIF_ANIMATED, GIF_ANIMATED + PALETTE_END);
  b.push_back(0x3b);
  MemReader r(b);
  GifDecoder *d = make();
  TEST_ASSERT_TRUE(d->open(r));
  uint32_t ms;
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_EQUAL(GIF_ERROR, d->next(&ms));
    TEST_ASSERT_EQUAL(GE_NO_FRAME, d->error());
  }
  TEST_ASSERT_TRUE(d->rewind());
  TEST_ASSERT_EQUAL(GIF_ERROR, d->next(&ms));
  delete d;
}

// --- The reader ------------------------------------------------------------------

static void test_reader_errors() {
  const uint8_t *files[] = {GIF_ANIMATED, GIF_PORTRAIT};
  const size_t sizes[] = {sizeof GIF_ANIMATED, sizeof GIF_PORTRAIT};
  for (int f = 0; f < 2; f++) {
    int count;
    {
      MemReader r(files[f], sizes[f]);
      GifDecoder *d = make();
      TEST_ASSERT_TRUE(d->open(r));
      uint32_t ms;
      while (d->next(&ms) == GIF_FRAME) {}
      TEST_ASSERT_EQUAL(GE_NONE, d->error());
      count = r.reads;
      delete d;
    }
    TEST_ASSERT_TRUE(count > 4);
    for (int k = 1; k <= count; k++) {
      MemReader r(files[f], sizes[f]);
      r.failAt = k;
      GifDecoder *d = make();
      uint32_t ms;
      if (d->open(r))
        while (d->next(&ms) == GIF_FRAME) {}
      TEST_ASSERT_EQUAL(GE_READ, d->error());
      TEST_ASSERT_EQUAL(GIF_ERROR, d->next(&ms));
      delete d;
    }
  }
  // a file that ends early
  MemReader early(GIF_ANIMATED, sizeof GIF_ANIMATED);
  early.size = 1500;
  GifDecoder *d = make();
  TEST_ASSERT_TRUE(d->open(early));
  uint32_t ms;
  TEST_ASSERT_EQUAL(GIF_FRAME, d->next(&ms));
  TEST_ASSERT_EQUAL(GIF_ERROR, d->next(&ms));
  TEST_ASSERT_EQUAL(GE_TRUNCATED, d->error());
  // a seek that fails: no new loop
  MemReader noSeek(GIF_ANIMATED, sizeof GIF_ANIMATED);
  TEST_ASSERT_TRUE(d->open(noSeek));
  TEST_ASSERT_EQUAL(GIF_FRAME, d->next(&ms));
  noSeek.seekFails = true;
  TEST_ASSERT_FALSE(d->rewind());
  TEST_ASSERT_EQUAL(GIF_ERROR, d->next(&ms));
  TEST_ASSERT_EQUAL(GE_READ, d->error());
  // never opened
  GifDecoder *fresh = make();
  TEST_ASSERT_FALSE(fresh->rewind());
  TEST_ASSERT_EQUAL(GIF_ERROR, fresh->next(&ms));
  delete fresh;
  delete d;
}

static void test_working_set() {
  char msg[64];
  snprintf(msg, sizeof msg, "sizeof(GifDecoder) = %u bytes", (unsigned)sizeof(GifDecoder));
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE(sizeof(GifDecoder) <= 24576);
  TEST_ASSERT_EQUAL_STRING("lzw", gifErrorName(GE_LZW));
  TEST_ASSERT_NOT_NULL(gifErrorName((GifError)200));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_animated_plays);
  RUN_TEST(test_portrait_still);
  RUN_TEST(test_interlace_rows);
  RUN_TEST(test_wrong_logical_screen);
  RUN_TEST(test_truncations);
  RUN_TEST(test_mutations);
  RUN_TEST(test_frame_rects);
  RUN_TEST(test_frame_larger_than_canvas);
  RUN_TEST(test_code_sizes);
  RUN_TEST(test_lzw_table_overflow);
  RUN_TEST(test_lzw_codes);
  RUN_TEST(test_disposal_and_transparency);
  RUN_TEST(test_blocks);
  RUN_TEST(test_no_frame);
  RUN_TEST(test_reader_errors);
  RUN_TEST(test_working_set);
  return UNITY_END();
}
