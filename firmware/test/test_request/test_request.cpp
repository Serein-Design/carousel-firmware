// SPDX-License-Identifier: AGPL-3.0-only
// A request shows itself: what each kind shows for each Panel setting, what
// crosses to loop(), the band's scroll for long and accented text, the
// chip's icon per source, and that the overlay touches only its own pixels.
//
//   pio test -e native -f test_request
#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"
#include "../../src/i18n.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/palette.cpp"
#include "../../src/request.cpp"

Settings settings;
const Palette &palette() { return paletteFor(false); }

// The gallery's queue, as requestShown() posts to it
static std::vector<RequestOrder> posted;
static bool queueFull = false;
bool galleryRequest(const RequestOrder &o) {
  if (queueFull) return false;
  posted.push_back(o);
  return true;
}

// A canvas that remembers every pixel written and counts writes off its edges
class Canvas : public Adafruit_GFX {
 public:
  uint16_t px[64][64];
  bool     hit[64][64];
  unsigned outside = 0;
  Canvas(int16_t w, int16_t h) : Adafruit_GFX(w, h) { clear(); }
  void clear() { memset(px, 0, sizeof(px)); memset(hit, 0, sizeof(hit)); outside = 0; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) { outside++; return; }
    px[y][x] = color;
    hit[y][x] = true;
  }
  int touched(int x0, int y0, int x1, int y1) const {   // inclusive box
    int n = 0;
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) n += hit[y][x];
    return n;
  }
  int lit(int x0, int y0, int x1, int y1, uint16_t c) const {
    int n = 0;
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) n += hit[y][x] && px[y][x] == c;
    return n;
  }
};

static const Palette &C = paletteFor(false);
static const uint32_t T0 = 100000;

static RequestOrder order(RequestKind kind, RequestSource from, const char *text = "",
                          uint32_t seconds = 0) {
  RequestOrder o = {};
  o.kind = kind;
  o.from = from;
  o.plan = requestPlan(kind, REQUEST_SHOW);
  o.color = REQUEST_DEFAULT_COLOR;
  o.seconds = seconds;
  strncpy(o.text, text, sizeof(o.text) - 1);
  return o;
}

void setUp() {
  posted.clear();
  queueFull = false;
  settings = Settings();
  requestOverlayClear();
}
void tearDown() {}

// --- Kind x setting -> what is shown --------------------------------------------

static void expectPlan(RequestKind k, uint8_t show, bool band, bool chip, bool back, bool stage) {
  const RequestPlan p = requestPlan(k, show);
  TEST_ASSERT_EQUAL(band, p.band);
  TEST_ASSERT_EQUAL(chip, p.chip);
  TEST_ASSERT_EQUAL(back, p.backAfterPlay);
  TEST_ASSERT_EQUAL(stage, p.onStage);
}

static void test_show_is_the_kinds_own_way() {
  expectPlan(REQUEST_MESSAGE, REQUEST_SHOW, true, false, false, false);
  expectPlan(REQUEST_IMAGE,   REQUEST_SHOW, false, false, true, false);
  expectPlan(REQUEST_COMMAND, REQUEST_SHOW, false, true, false, false);
  expectPlan(REQUEST_AGENT,   REQUEST_SHOW, false, false, false, true);
}

static void test_discreet_is_the_chip_only() {
  for (uint8_t k = 0; k < REQUEST_KIND_COUNT; k++)
    expectPlan((RequestKind)k, REQUEST_DISCREET, false, true, false, false);
}

static void test_off_shows_nothing() {
  for (uint8_t k = 0; k < REQUEST_KIND_COUNT; k++)
    expectPlan((RequestKind)k, REQUEST_OFF, false, false, false, false);
}

static void test_out_of_range_setting_counts_as_show() {
  expectPlan(REQUEST_MESSAGE, 7, true, false, false, false);
  expectPlan(REQUEST_AGENT, REQUEST_SHOW_COUNT, false, false, false, true);
}

static void test_shown_posts_the_plan_of_the_setting() {
  settings.requestShow[REQUEST_COMMAND] = REQUEST_DISCREET;
  requestShown(REQUEST_COMMAND, SOURCE_HOME_ASSISTANT, "WEATHER");
  TEST_ASSERT_EQUAL(1, (int)posted.size());
  TEST_ASSERT_EQUAL(REQUEST_COMMAND, posted[0].kind);
  TEST_ASSERT_EQUAL(SOURCE_HOME_ASSISTANT, posted[0].from);
  TEST_ASSERT_TRUE(posted[0].plan.chip);
  TEST_ASSERT_EQUAL_STRING("WEATHER", posted[0].text);
}

static void test_off_posts_nothing() {
  for (uint8_t k = 0; k < REQUEST_KIND_COUNT; k++) settings.requestShow[k] = REQUEST_OFF;
  requestShown(REQUEST_MESSAGE, SOURCE_API, "Dinner is ready");
  requestShown(REQUEST_AGENT, SOURCE_AGENT, "carousel");
  TEST_ASSERT_EQUAL(0, (int)posted.size());
}

// A full queue is logged by kind and source: a message's text (a code, a
// name) never reaches a serial log that may be shared for support
static void test_a_full_queue_logs_the_kind_never_the_text() {
  queueFull = true;
  Serial.last[0] = '\0';
  requestShown(REQUEST_MESSAGE, SOURCE_HOME_ASSISTANT, "door code 4821");
  TEST_ASSERT_EQUAL(0, (int)posted.size());
  TEST_ASSERT_NOT_NULL(strstr(Serial.last, "[REQUEST]"));
  TEST_ASSERT_NOT_NULL(strstr(Serial.last, "kind 0 from 2"));
  TEST_ASSERT_NULL(strstr(Serial.last, "4821"));
}

static void test_message_keeps_its_accents_colour_and_seconds() {
  requestShown(REQUEST_MESSAGE, SOURCE_API, "À table, c’est prêt !", 0x07E0, 30);
  TEST_ASSERT_EQUAL(1, (int)posted.size());
  TEST_ASSERT_EQUAL_STRING("À table, c’est prêt !", posted[0].text);
  TEST_ASSERT_EQUAL_HEX16(0x07E0, posted[0].color);
  TEST_ASSERT_EQUAL_UINT32(30, posted[0].seconds);
}

static void test_long_text_is_cut_on_a_character_boundary() {
  // 60 two-byte letters, 95 bytes of room: 47 whole letters, never half of one
  std::string s;
  for (int i = 0; i < 60; i++) s += "é";
  requestShown(REQUEST_MESSAGE, SOURCE_API, s.c_str());
  const char *t = posted[0].text;
  TEST_ASSERT_EQUAL(94, (int)strlen(t));
  TEST_ASSERT_EQUAL(0xA9, (uint8_t)t[93]);   // the last é whole
}

static void test_line_breaks_become_spaces() {
  requestShown(REQUEST_MESSAGE, SOURCE_API, "Door\nopen\t!");
  TEST_ASSERT_EQUAL_STRING("Door open !", posted[0].text);
}

// --- The band's scroll ----------------------------------------------------------

static void test_pass_is_text_width_plus_the_span_it_crosses() {
  const char *msg = "Dinner is ready";
  TEST_ASSERT_EQUAL_UINT32(textWidth(msg, Face::BODY) + 52, bandPassPx(msg, 64));
  TEST_ASSERT_EQUAL_UINT32(textWidth(msg, Face::BODY) + 20, bandPassPx(msg, 32));
  TEST_ASSERT_EQUAL_UINT32(bandPassPx(msg, 64) * 1000 / BAND_PX_PER_S, bandPassMs(msg, 64));
}

static void test_accented_text_is_measured_as_drawn() {
  // Accents ride over their letter: the same width as the plain text, and
  // the narrow no-break space is measured, not dropped
  TEST_ASSERT_EQUAL_UINT32(bandPassPx("prete", 64), bandPassPx("prête", 64));
  TEST_ASSERT_TRUE(bandPassPx("prêt !", 64) > bandPassPx("prêt!", 64));
}

static void test_long_text_scrolls_twice_by_default() {
  const char *msg = "The washing machine is done, the dryer starts in ten minutes";
  const uint32_t pass = bandPassMs(msg, 64);
  TEST_ASSERT_TRUE(textWidth(msg, Face::BODY) > 200);
  TEST_ASSERT_EQUAL(2, bandPasses(msg, 64, 0));
  TEST_ASSERT_EQUAL(2, bandPasses(msg, 64, 10));
  TEST_ASSERT_EQUAL_UINT32(2 * BAND_SLIDE_MS + 2 * pass, bandDurationMs(msg, 64, 10));
}

static void test_seconds_longer_than_two_passes_add_passes() {
  const char *msg = "Door open";
  const uint32_t pass = bandPassMs(msg, 64);
  const uint8_t n = bandPasses(msg, 64, 30);
  TEST_ASSERT_TRUE(n > 2);
  TEST_ASSERT_TRUE(n * pass >= 30000);
  TEST_ASSERT_TRUE((n - 1) * pass < 30000);
}

static void test_band_slides_in_holds_and_slides_out() {
  const uint32_t d = 10000;
  TEST_ASSERT_EQUAL(-BAND_H, bandTop(0, d));
  TEST_ASSERT_TRUE(bandTop(100, d) > -BAND_H && bandTop(100, d) < 0);
  TEST_ASSERT_EQUAL(0, bandTop(BAND_SLIDE_MS, d));
  TEST_ASSERT_EQUAL(0, bandTop(d - BAND_SLIDE_MS, d));
  TEST_ASSERT_TRUE(bandTop(d - 100, d) < 0);
  TEST_ASSERT_EQUAL(-BAND_H, bandTop(d, d));
}

static void test_text_starts_at_the_right_edge_each_pass() {
  const char *msg = "Dinner is ready";
  const uint32_t pass = bandPassMs(msg, 64);
  TEST_ASSERT_EQUAL(62, bandTextX(msg, 64, 0));
  TEST_ASSERT_EQUAL(62, bandTextX(msg, 64, BAND_SLIDE_MS));
  TEST_ASSERT_EQUAL(62 - 24, bandTextX(msg, 64, BAND_SLIDE_MS + 1000));
  TEST_ASSERT_EQUAL(62, bandTextX(msg, 64, BAND_SLIDE_MS + pass));
  TEST_ASSERT_EQUAL(30, bandTextX(msg, 32, BAND_SLIDE_MS));
}

// --- The chip's icon ------------------------------------------------------------

static void test_every_source_has_its_own_icon() {
  for (uint8_t a = 0; a < SOURCE_COUNT; a++) {
    const SourceIcon &i = sourceIcon((RequestSource)a);
    TEST_ASSERT_TRUE(i.w >= 3 && i.w <= 5);
    for (uint8_t b = a + 1; b < SOURCE_COUNT; b++)
      TEST_ASSERT_TRUE(memcmp(&i, &sourceIcon((RequestSource)b), sizeof(SourceIcon)) != 0);
  }
}

// The icon's pixels, drawn in the accent at the chip's place
static void expectIcon(const Canvas &g, RequestSource s) {
  const SourceIcon &i = sourceIcon(s);
  const int x0 = g.width() - CHIP_SIZE + 2 + (5 - i.w) / 2;
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < i.w; c++) {
      const bool on = i.rows[r] >> (4 - c) & 1;
      TEST_ASSERT_EQUAL(on ? C.accent : C.off, g.px[2 + r][x0 + c]);
    }
}

static void test_chip_draws_the_source_icon_top_right() {
  for (int16_t w : {64, 32}) {
    for (uint8_t s = 0; s < SOURCE_COUNT; s++) {
      Canvas g(w, w == 64 ? 32 : 64);
      requestOverlayClear();
      RequestOrder o = order(REQUEST_COMMAND, (RequestSource)s);
      requestOverlayStart(o, o.plan, T0);
      TEST_ASSERT_TRUE(requestOverlayDraw(g, C, T0 + CHIP_PULSE_MS + 100));
      expectIcon(g, (RequestSource)s);
      TEST_ASSERT_EQUAL(CHIP_SIZE * CHIP_SIZE, g.touched(w - CHIP_SIZE, 0, w - 1, CHIP_SIZE - 1));
      TEST_ASSERT_EQUAL(CHIP_SIZE * CHIP_SIZE, g.touched(0, 0, w - 1, g.height() - 1));
      TEST_ASSERT_EQUAL(0u, g.outside);
    }
  }
}

// The pulse is a brighter fill of the chip itself, nothing outside it: the
// 1 px outline is the knob's mode marker
static void test_chip_pulses_in_its_square_then_goes_after_1500_ms() {
  Canvas g(64, 32);
  RequestOrder o = order(REQUEST_COMMAND, SOURCE_WEB);
  requestOverlayStart(o, o.plan, T0);
  requestOverlayDraw(g, C, T0 + 100);
  const int ground = CHIP_SIZE * CHIP_SIZE - g.lit(64 - CHIP_SIZE, 0, 63, CHIP_SIZE - 1, C.accent);
  TEST_ASSERT_EQUAL(CHIP_SIZE * CHIP_SIZE, g.touched(0, 0, 63, 31));
  TEST_ASSERT_EQUAL(ground, g.lit(64 - CHIP_SIZE, 0, 63, CHIP_SIZE - 1, C.dim));
  g.clear();
  requestOverlayDraw(g, C, T0 + CHIP_PULSE_MS);
  TEST_ASSERT_EQUAL(ground, g.lit(64 - CHIP_SIZE, 0, 63, CHIP_SIZE - 1, C.off));
  g.clear();
  TEST_ASSERT_TRUE(requestOverlayDraw(g, C, T0 + CHIP_MS - 1));
  g.clear();
  TEST_ASSERT_FALSE(requestOverlayDraw(g, C, T0 + CHIP_MS));
  TEST_ASSERT_EQUAL(0, g.touched(0, 0, 63, 31));
}

// --- The band on the frame ------------------------------------------------------

static void test_band_draws_only_its_rows_with_icon_and_text() {
  for (int16_t w : {64, 32}) {
    Canvas g(w, w == 64 ? 32 : 64);
    requestOverlayClear();
    RequestOrder o = order(REQUEST_MESSAGE, SOURCE_HOME_ASSISTANT, "Front door open");
    requestOverlayStart(o, o.plan, T0);
    TEST_ASSERT_TRUE(requestOverlayDraw(g, C, T0 + BAND_SLIDE_MS + 1500));
    TEST_ASSERT_EQUAL(0, g.touched(0, BAND_H, w - 1, g.height() - 1));   // the screen stays
    TEST_ASSERT_EQUAL(0u, g.outside);
    TEST_ASSERT_EQUAL(w, g.lit(0, BAND_H - 1, w - 1, BAND_H - 1, C.dim));   // its rule
    TEST_ASSERT_TRUE(g.lit(0, 0, BAND_TEXT_X - 2, BAND_H - 2, C.accent) > 0);   // the icon
    TEST_ASSERT_TRUE(g.lit(BAND_TEXT_X, 0, w - 1, BAND_H - 2, C.text) > 0);    // the text
    TEST_ASSERT_EQUAL(0, g.lit(0, 0, BAND_TEXT_X - 1, BAND_H - 2, C.text));    // never under the icon
  }
}

static void test_band_text_in_the_senders_colour() {
  Canvas g(64, 32);
  RequestOrder o = order(REQUEST_MESSAGE, SOURCE_API, "Door open");
  o.color = 0x07E0;
  requestOverlayStart(o, o.plan, T0);
  requestOverlayDraw(g, C, T0 + BAND_SLIDE_MS + 1500);
  TEST_ASSERT_TRUE(g.lit(BAND_TEXT_X, 0, 63, BAND_H - 2, 0x07E0) > 0);
}

static void test_band_ends_after_its_duration() {
  Canvas g(64, 32);
  RequestOrder o = order(REQUEST_MESSAGE, SOURCE_API, "Door open");
  requestOverlayStart(o, o.plan, T0);
  const uint32_t d = bandDurationMs("Door open", 64, 0);
  TEST_ASSERT_TRUE(requestOverlayDraw(g, C, T0 + d - 1));
  g.clear();
  TEST_ASSERT_FALSE(requestOverlayDraw(g, C, T0 + d));
  TEST_ASSERT_EQUAL(0, g.touched(0, 0, 63, 31));
}

static void test_off_and_takeover_plans_draw_nothing() {
  Canvas g(64, 32);
  RequestOrder o = order(REQUEST_IMAGE, SOURCE_API);   // full screen: the screen is the answer
  requestOverlayStart(o, o.plan, T0);
  TEST_ASSERT_FALSE(requestOverlayDraw(g, C, T0 + 10));
  o = order(REQUEST_MESSAGE, SOURCE_API, "hi");
  o.plan = requestPlan(REQUEST_MESSAGE, REQUEST_OFF);
  requestOverlayStart(o, o.plan, T0);
  TEST_ASSERT_FALSE(requestOverlayDraw(g, C, T0 + 500));
  TEST_ASSERT_EQUAL(0, g.touched(0, 0, 63, 31));
}

static void test_a_new_chip_restarts_and_keeps_the_band() {
  Canvas g(64, 32);
  RequestOrder m = order(REQUEST_MESSAGE, SOURCE_API, "Door open");
  requestOverlayStart(m, m.plan, T0);
  RequestOrder c = order(REQUEST_COMMAND, SOURCE_WEB);
  requestOverlayStart(c, c.plan, T0 + 1000);
  requestOverlayDraw(g, C, T0 + 1000 + CHIP_PULSE_MS + 10);
  expectIcon(g, SOURCE_WEB);
  TEST_ASSERT_EQUAL(64, g.lit(0, BAND_H - 1, 63, BAND_H - 1, C.dim));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_show_is_the_kinds_own_way);
  RUN_TEST(test_discreet_is_the_chip_only);
  RUN_TEST(test_off_shows_nothing);
  RUN_TEST(test_out_of_range_setting_counts_as_show);
  RUN_TEST(test_shown_posts_the_plan_of_the_setting);
  RUN_TEST(test_off_posts_nothing);
  RUN_TEST(test_a_full_queue_logs_the_kind_never_the_text);
  RUN_TEST(test_message_keeps_its_accents_colour_and_seconds);
  RUN_TEST(test_long_text_is_cut_on_a_character_boundary);
  RUN_TEST(test_line_breaks_become_spaces);
  RUN_TEST(test_pass_is_text_width_plus_the_span_it_crosses);
  RUN_TEST(test_accented_text_is_measured_as_drawn);
  RUN_TEST(test_long_text_scrolls_twice_by_default);
  RUN_TEST(test_seconds_longer_than_two_passes_add_passes);
  RUN_TEST(test_band_slides_in_holds_and_slides_out);
  RUN_TEST(test_text_starts_at_the_right_edge_each_pass);
  RUN_TEST(test_every_source_has_its_own_icon);
  RUN_TEST(test_chip_draws_the_source_icon_top_right);
  RUN_TEST(test_chip_pulses_in_its_square_then_goes_after_1500_ms);
  RUN_TEST(test_band_draws_only_its_rows_with_icon_and_text);
  RUN_TEST(test_band_text_in_the_senders_colour);
  RUN_TEST(test_band_ends_after_its_duration);
  RUN_TEST(test_off_and_takeover_plans_draw_nothing);
  RUN_TEST(test_a_new_chip_restarts_and_keeps_the_band);
  return UNITY_END();
}
