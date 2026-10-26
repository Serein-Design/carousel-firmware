// SPDX-License-Identifier: AGPL-3.0-only
// The status strip on the bottom row of the system-grid screens: a ghost line
// filling from the left over the rotation's interval (nothing when the
// rotation is off or held), and a 2-pixel mark at the right end for the
// agents, orange when one needs the owner, dim while one works, nothing
// otherwise, left out on CODE, which shows the sessions itself. Full-art
// screens get neither. Both orientations.
//
//   pio test -e native -f test_status_strip
#include <unity.h>

#include <cstring>

#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"
#include "../../src/i18n.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/palette.cpp"
#include "../../src/code_view_logic.cpp"
#include "../../src/status_strip.cpp"

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
  int changed() const {
    int n = 0;
    for (int y = 0; y < height(); y++)
      for (int x = 0; x < width(); x++) n += px[y][x] != SCREEN;
    return n;
  }
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
static const Palette &P = paletteFor(false);

static uint8_t idx(const char *name) {
  const int i = screenFind(name);
  TEST_ASSERT_TRUE_MESSAGE(i >= 0, name);
  return (uint8_t)i;
}

static const char *const GRID[] = {"CODE",   "CLOCK", "WEATHER", "DATE",
                                   "SYSTEM", "MONEY", "SOCIAL",  "TEXT"};

static bool inGrid(uint8_t screen) {
  for (const char *n : GRID)
    if (idx(n) == screen) return true;
  return false;
}

// --- which screens --------------------------------------------------------

static void test_only_the_grid_screens_have_it() {
  for (const char *n : GRID) TEST_ASSERT_TRUE_MESSAGE(screenHasStrip(idx(n)), n);
  for (uint8_t i = 0; i < SCREEN_COUNT; i++)
    TEST_ASSERT_EQUAL_MESSAGE(inGrid(i), screenHasStrip(i), screenNameEn(i));
}

static void test_nothing_drawn_on_art_screens() {
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    if (inGrid(i)) continue;
    for (const auto &s : SIZES) {
      Canvas c(s[0], s[1]);
      drawStatusStrip(c, P, i, 0.5f, AgentMark::NEEDS_OWNER);
      TEST_ASSERT_EQUAL_MESSAGE(0, c.changed(), screenNameEn(i));
    }
  }
}

// --- the line -------------------------------------------------------------

static void test_line_fills_over_the_interval() {
  for (const auto &s : SIZES) {
    const int16_t w = s[0], bottom = s[1] - 1, len = w - 5;   // 3 px apart from the mark
    Canvas zero(w, s[1]);
    drawStatusStrip(zero, P, idx("CLOCK"), 0, AgentMark::NONE);
    TEST_ASSERT_EQUAL(0, zero.changed());

    Canvas half(w, s[1]);
    drawStatusStrip(half, P, idx("CLOCK"), 0.5f, AgentMark::NONE);
    TEST_ASSERT_EQUAL(0, half.changedAbove());
    TEST_ASSERT_EQUAL(0, half.outside);
    TEST_ASSERT_EQUAL((int)lroundf(len / 2.0f), half.count(P.ghost, bottom));
    TEST_ASSERT_EQUAL_HEX16(P.ghost, half.px[bottom][0]);   // from the left
    TEST_ASSERT_EQUAL(w - (int)lroundf(len / 2.0f), half.count(SCREEN, bottom));

    Canvas end(w, s[1]);
    drawStatusStrip(end, P, idx("CLOCK"), 1, AgentMark::NONE);
    TEST_ASSERT_EQUAL(len, end.count(P.ghost, bottom));
    TEST_ASSERT_EQUAL(0, end.changedAbove());
    // The mark's place stays apart even at the end
    for (int16_t x = len; x < w; x++) TEST_ASSERT_EQUAL_HEX16(SCREEN, end.px[bottom][x]);
  }
}

static void test_no_line_when_rotation_off_or_held() {
  for (const auto &s : SIZES) {
    Canvas c(s[0], s[1]);
    drawStatusStrip(c, P, idx("DATE"), -1, AgentMark::NONE);
    TEST_ASSERT_EQUAL(0, c.changed());
  }
}

// --- the agents' mark -----------------------------------------------------

static void test_mark_colour_per_state() {
  const struct {
    AgentMark mark;
    uint16_t  colour;
  } cases[] = {{AgentMark::NEEDS_OWNER, P.accent}, {AgentMark::WORKING, P.dim}};
  for (const auto &k : cases)
    for (const auto &s : SIZES) {
      const int16_t w = s[0], bottom = s[1] - 1;
      Canvas c(w, s[1]);
      drawStatusStrip(c, P, idx("WEATHER"), -1, k.mark);
      TEST_ASSERT_EQUAL(2, c.changed());
      TEST_ASSERT_EQUAL_HEX16(k.colour, c.px[bottom][w - 2]);
      TEST_ASSERT_EQUAL_HEX16(k.colour, c.px[bottom][w - 1]);
    }
  Canvas none(64, 32);
  drawStatusStrip(none, P, idx("WEATHER"), -1, AgentMark::NONE);
  TEST_ASSERT_EQUAL(0, none.changed());
}

// CODE shows the sessions itself: its strip has the line, never the mark
static void test_no_mark_on_code() {
  Canvas c(64, 32);
  drawStatusStrip(c, P, idx("CODE"), 1, AgentMark::NEEDS_OWNER);
  TEST_ASSERT_EQUAL(59, c.count(P.ghost, 31));
  TEST_ASSERT_EQUAL(0, c.count(P.accent, 31));
  TEST_ASSERT_EQUAL(59, c.changed());
}

static void test_mark_from_the_sessions() {
  TEST_ASSERT_TRUE(agentMark(nullptr, 0) == AgentMark::NONE);   // no session
  const AgentState quiet[] = {AG_IDLE, AG_DONE, AG_ERROR};
  TEST_ASSERT_TRUE(agentMark(quiet, 3) == AgentMark::NONE);     // none at work
  const AgentState thinking[] = {AG_IDLE, AG_THINKING};
  TEST_ASSERT_TRUE(agentMark(thinking, 2) == AgentMark::WORKING);
  const AgentState tool[] = {AG_TOOL};
  TEST_ASSERT_TRUE(agentMark(tool, 1) == AgentMark::WORKING);
  const AgentState failed[] = {AG_FAILED};   // a tool failed: it carries on
  TEST_ASSERT_TRUE(agentMark(failed, 1) == AgentMark::WORKING);
  const AgentState needs[] = {AG_TOOL, AG_ATTENTION, AG_IDLE};
  TEST_ASSERT_TRUE(agentMark(needs, 3) == AgentMark::NEEDS_OWNER);   // the owner first
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_only_the_grid_screens_have_it);
  RUN_TEST(test_nothing_drawn_on_art_screens);
  RUN_TEST(test_line_fills_over_the_interval);
  RUN_TEST(test_no_line_when_rotation_off_or_held);
  RUN_TEST(test_mark_colour_per_state);
  RUN_TEST(test_no_mark_on_code);
  RUN_TEST(test_mark_from_the_sessions);
  return UNITY_END();
}
