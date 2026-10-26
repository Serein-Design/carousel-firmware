// SPDX-License-Identifier: AGPL-3.0-only
// The panel's text: widths as the design measured them, French accents
// composed over their base letter, an unknown character shown as a box, no
// pixel written off the canvas, and the three alignments.
//
//   pio test -e native -f test_type
#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"

static const uint16_t INK = 0xFFFF;

// A 64x32 canvas that remembers every pixel and counts writes off its edges
class Canvas : public Adafruit_GFX {
 public:
  uint16_t px[32][64];
  unsigned outside = 0, writes = 0;
  Canvas() : Adafruit_GFX(64, 32) { clear(); }
  void clear() { memset(px, 0, sizeof(px)); outside = writes = 0; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    writes++;
    if (x < 0 || y < 0 || x >= 64 || y >= 32) { outside++; return; }
    px[y][x] = color;
  }
  int lit(int x0, int y0, int x1, int y1) const {   // inclusive box, clipped
    int n = 0;
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < 32; y++)
      for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < 64; x++) n += px[y][x] != 0;
    return n;
  }
  int leftmost() const {
    for (int x = 0; x < 64; x++) if (lit(x, 0, x, 31)) return x;
    return -1;
  }
  int rightmost() const {
    for (int x = 63; x >= 0; x--) if (lit(x, 0, x, 31)) return x;
    return -1;
  }
};
static Canvas canvas;

void setUp() { canvas.clear(); }
void tearDown() {}

// Expected values: the design's own measure() (the board's led.js), run once
// in node on these strings and copied here. A difference means the firmware
// draws other glyphs or spacing than the board showed.
static void test_widths_match_the_design() {
  TEST_ASSERT_EQUAL_UINT16(60, textWidth("Vous attend", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(42, textWidth("LUN. 19 OCT.", Face::LABEL));
  TEST_ASSERT_EQUAL_UINT16(42, textWidth("14:32", Face::PIERRE));
  TEST_ASSERT_EQUAL_UINT16(43, textWidth("Réfléchit", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(39, textWidth("ÉCLAIRCIES", Face::LABEL));
  TEST_ASSERT_EQUAL_UINT16(36, textWidth("Terminé", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(30, textWidth("AU REPOS", Face::LABEL));
  TEST_ASSERT_EQUAL_UINT16(50, textWidth("Garçon île", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(22, textWidth("9° 16°", Face::LABEL));
  TEST_ASSERT_EQUAL_UINT16(27, textWidth("14 °", Face::PIERRE));
  TEST_ASSERT_EQUAL_UINT16(0, textWidth("", Face::BODY));
}

static void test_witness_strings_fit_the_panel() {
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(60, textWidth("Vous attend", Face::BODY));
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(60, textWidth("LUN. 19 OCT.", Face::LABEL));
}

static void test_spaces() {
  // No-break space: one space; narrow no-break space: 1 px, both plus the track
  TEST_ASSERT_EQUAL_UINT16(textWidth("a b", Face::BODY), textWidth("a b", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(textWidth("ab", Face::BODY) + 2, textWidth("a b", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(textWidth("AB", Face::LABEL) + 2, textWidth("A B", Face::LABEL));
}

// Every French accented letter: as wide as its base letter, and lit above it
// (below for the cedilla), where the base alone is dark.
struct Accent { const char *accented, *base; bool below; };
static const Accent ACCENTS[] = {
  {"é", "e", false}, {"è", "e", false}, {"ê", "e", false}, {"ë", "e", false},
  {"à", "a", false}, {"â", "a", false}, {"î", "i", false}, {"ï", "i", false},
  {"ô", "o", false}, {"û", "u", false}, {"ù", "u", false}, {"ü", "u", false},
  {"ç", "c", true},  {"É", "E", false}, {"È", "E", false}, {"Ê", "E", false},
  {"À", "A", false}, {"Ç", "C", true},  {"Ô", "O", false},
};

static void test_accents_compose_over_the_base() {
  for (Face f : {Face::BODY, Face::LABEL}) {
    const FontFace &face = FONT_FACES[(int)f];
    for (const Accent &a : ACCENTS) {
      // i takes a mark as dotless i (body), as wide as the mark, as the design measures it
      const bool dotless = f == Face::BODY && !strcmp(a.base, "i");
      const char *base = dotless ? "\u0131" : a.base;
      if (!dotless) TEST_ASSERT_EQUAL_UINT16_MESSAGE(textWidth(base, f), textWidth(a.accented, f), a.accented);
      // Rows the mark owns: the row above the cap (and the two below it for a
      // body lowercase letter, whose top is row 2), or the two under the baseline
      const int y = 10;
      const bool lower = f == Face::BODY && a.base[0] >= 'a' && a.base[0] <= 'z';
      const int top = a.below ? y + face.cedilRow : y - 1;
      const int bottom = a.below ? top + 1 : (lower ? y + 1 : y - 1);
      drawText(canvas, base, 4, y, f, INK);
      const int baseOnly = canvas.lit(0, top, 63, bottom);
      canvas.clear();
      drawText(canvas, a.accented, 4, y, f, INK);
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, baseOnly, a.accented);
      TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, canvas.lit(0, top, 63, bottom), a.accented);
      canvas.clear();
    }
  }
  TEST_ASSERT_EQUAL_UINT16(3, textWidth("î", Face::BODY));
}

static void test_lowercase_i_with_a_mark_has_no_dot() {
  // Body i: the dot is row 0, the stem rows 2-6. î: no pixel on row 1 (the
  // gap under the dot), the circumflex on rows -1..0, the stem below.
  drawText(canvas, "î", 4, 10, Face::BODY, INK);
  TEST_ASSERT_EQUAL_INT(0, canvas.lit(0, 11, 63, 11));
  TEST_ASSERT_EQUAL_INT(5, canvas.lit(0, 12, 63, 16));
}

static void test_oe_ligature() {
  // Œ and œ: O and E (o and e) sharing their middle stroke
  TEST_ASSERT_EQUAL_UINT16(9, textWidth("Œ", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(9, textWidth("œ", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(5, textWidth("Œ", Face::LABEL));
  drawText(canvas, "œ", 0, 0, Face::BODY, INK);
  TEST_ASSERT_GREATER_THAN_INT(0, canvas.lit(0, 0, 3, 31));
  TEST_ASSERT_GREATER_THAN_INT(0, canvas.lit(5, 0, 8, 31));
}

static void test_unknown_code_point_is_an_alert_box() {
  // U+2603, a snowman: no glyph in any face
  TEST_ASSERT_EQUAL_UINT16(3, textWidth("☃", Face::BODY));
  TEST_ASSERT_EQUAL_UINT16(textWidth("a", Face::BODY) + 1 + 3, textWidth("a☃", Face::BODY));
  drawText(canvas, "☃", 2, 3, Face::BODY, INK);
  TEST_ASSERT_EQUAL_UINT16(TYPE_MISSING_COLOR, canvas.px[3][2]);
  TEST_ASSERT_EQUAL_UINT16(TYPE_MISSING_COLOR, canvas.px[3 + 6][4]);
  TEST_ASSERT_EQUAL_UINT16(0, canvas.px[5][3]);   // an outline: the middle stays dark
  TEST_ASSERT_EQUAL_INT(0, canvas.lit(5, 0, 63, 31));
  // A broken UTF-8 byte is unknown too, and the rest still reads
  TEST_ASSERT_EQUAL_UINT16(3 + 1 + textWidth("A", Face::LABEL), textWidth("\xC3" "A", Face::LABEL));
  // Pierre has digits only: a letter is a box as tall as its digits
  canvas.clear();
  drawText(canvas, "h", 0, 0, Face::PIERRE, INK);
  TEST_ASSERT_EQUAL_UINT16(TYPE_MISSING_COLOR, canvas.px[12][0]);
}

static void test_clipped_at_both_edges() {
  drawText(canvas, "Vous", -3, 0, Face::BODY, INK);
  TEST_ASSERT_EQUAL_UINT(0, canvas.outside);
  TEST_ASSERT_GREATER_THAN_UINT(0, canvas.writes);
  TEST_ASSERT_EQUAL_INT(0, canvas.leftmost());   // the V's right part still shows
  canvas.clear();
  drawText(canvas, "Vous", 62, 0, Face::BODY, INK);
  TEST_ASSERT_EQUAL_UINT(0, canvas.outside);
  TEST_ASSERT_EQUAL_INT(62, canvas.leftmost());
  TEST_ASSERT_EQUAL_INT(0, canvas.lit(0, 0, 61, 31));   // nothing wraps to the left
  canvas.clear();
  drawText(canvas, "ÉÇ", 0, 0, Face::BODY, INK);   // the mark above row 0
  drawText(canvas, "Ç", 0, 27, Face::BODY, INK);   // the cedilla under row 31
  TEST_ASSERT_EQUAL_UINT(0, canvas.outside);
}

static void test_alignment() {
  // Centre: x is the middle, the odd pixel to the right as the design rounds
  const uint16_t w = textWidth("LUN. 19 OCT.", Face::LABEL);   // 42
  drawText(canvas, "LUN. 19 OCT.", 32, 2, Face::LABEL, INK, Align::CENTER);
  TEST_ASSERT_EQUAL_INT(32 - w / 2, canvas.leftmost());
  TEST_ASSERT_EQUAL_INT(32 - w / 2 + w - 1, canvas.rightmost());
  canvas.clear();
  drawText(canvas, "PARIS", 32, 2, Face::LABEL, INK, Align::CENTER);   // 19 wide
  TEST_ASSERT_EQUAL_INT(23, canvas.leftmost());
  // Right: x is the first column after the text
  canvas.clear();
  drawText(canvas, "9° 16°", 61, 2, Face::LABEL, INK, Align::RIGHT);
  TEST_ASSERT_EQUAL_INT(60, canvas.rightmost());
  TEST_ASSERT_EQUAL_INT(61 - 22, canvas.leftmost());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_widths_match_the_design);
  RUN_TEST(test_witness_strings_fit_the_panel);
  RUN_TEST(test_spaces);
  RUN_TEST(test_accents_compose_over_the_base);
  RUN_TEST(test_lowercase_i_with_a_mark_has_no_dot);
  RUN_TEST(test_oe_ligature);
  RUN_TEST(test_unknown_code_point_is_an_alert_box);
  RUN_TEST(test_clipped_at_both_edges);
  RUN_TEST(test_alignment);
  return UNITY_END();
}
