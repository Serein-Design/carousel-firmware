// SPDX-License-Identifier: AGPL-3.0-only
// The panel's text: the three Atelier faces (tools/fonts/atelier.mjs writes
// their glyphs to fonts_data.cpp), measured and drawn from UTF-8. French
// accented letters are composed from the base letter and a mark; a character
// no face has is drawn as a box in the alert colour, so it shows on the panel
// instead of vanishing. Hardware-free: host-tested in test_type.
#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

enum class Face : uint8_t { LABEL, BODY, PIERRE };   // the order of FONT_FACES
enum class Align : uint8_t { LEFT, CENTER, RIGHT };

// One glyph or mark: rows packed 1 bit per pixel, MSB first, from offset in
// the face's bits; top is its first row from the cap top (marks: from the
// mark's row).
struct FontGlyph {
  uint16_t cp;   // code point; 0 for a mark
  uint16_t offset;
  uint8_t  w, h;
  int8_t   top;
};

struct FontFace {
  const uint8_t   *bits;
  const FontGlyph *glyphs;     // sorted by code point
  uint8_t          count;
  const FontGlyph *marks;      // acute, grave, circ, diaer, cedil; nullptr: no accents
  const FontGlyph *capMarks;   // the same over capitals, cedilla aside; nullptr: marks
  uint8_t          cap, pitch, space, track;
  uint8_t          tabular;    // digit advance; 0: proportional
  int8_t           markRow, cedilRow;
};

extern const FontFace FONT_FACES[3];

// The alert red (#C8323C), the same in both palettes: the box drawn for a
// character no face has
static const uint16_t TYPE_MISSING_COLOR = 0xC987;

// Whether the face draws this code point, a composed or ligature one included;
// false: drawText shows the alert box
bool hasGlyph(uint32_t cp, Face f);

// The width drawText lays out: advances and tracking, the last track left out
uint16_t textWidth(const char *utf8, Face f);

// y is the cap top; with CENTER x is the middle, with RIGHT the column just
// after the text. Pixels off the canvas are skipped: nothing wraps.
void drawText(Adafruit_GFX &g, const char *utf8, int16_t x, int16_t y, Face f, uint16_t color,
              Align a = Align::LEFT);
