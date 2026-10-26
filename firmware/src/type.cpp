// SPDX-License-Identifier: AGPL-3.0-only
#include "type.h"

namespace {

enum Mark : uint8_t { ACUTE, GRAVE, CIRC, DIAER, CEDIL };

// The French accented letters and the base and mark they are drawn from
struct Composed { uint16_t cp; char base; Mark mark; };
const Composed COMPOSED[] = {
  {0x00C0, 'A', GRAVE}, {0x00C2, 'A', CIRC},  {0x00C4, 'A', DIAER}, {0x00C7, 'C', CEDIL},
  {0x00C8, 'E', GRAVE}, {0x00C9, 'E', ACUTE}, {0x00CA, 'E', CIRC},  {0x00CB, 'E', DIAER},
  {0x00CE, 'I', CIRC},  {0x00CF, 'I', DIAER}, {0x00D4, 'O', CIRC},  {0x00D6, 'O', DIAER},
  {0x00D9, 'U', GRAVE}, {0x00DB, 'U', CIRC},  {0x00DC, 'U', DIAER}, {0x00E0, 'a', GRAVE},
  {0x00E2, 'a', CIRC},  {0x00E4, 'a', DIAER}, {0x00E7, 'c', CEDIL}, {0x00E8, 'e', GRAVE},
  {0x00E9, 'e', ACUTE}, {0x00EA, 'e', CIRC},  {0x00EB, 'e', DIAER}, {0x00EE, 'i', CIRC},
  {0x00EF, 'i', DIAER}, {0x00F4, 'o', CIRC},  {0x00F6, 'o', DIAER}, {0x00F9, 'u', GRAVE},
  {0x00FB, 'u', CIRC},  {0x00FC, 'u', DIAER}, {0x00FF, 'y', DIAER}, {0x0178, 'Y', DIAER},
};

const uint16_t NBSP = 0x00A0, NNBSP = 0x202F, DOTLESS_I = 0x0131;
const uint16_t OE_UPPER = 0x0152, OE_LOWER = 0x0153;
const uint16_t BAD = 0xFFFD;

// Next code point; a malformed sequence gives BAD and moves one byte on
uint32_t nextCodePoint(const char *&p) {
  const uint8_t c = (uint8_t)*p++;
  if (c < 0x80) return c;
  int more = c >= 0xF0 && c < 0xF8 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 && c < 0xE0 ? 1 : -1;
  if (more < 0 || c >= 0xF8) return BAD;
  uint32_t cp = c & (0x3F >> more);
  for (int i = 0; i < more; i++) {
    const uint8_t n = (uint8_t)p[i];
    if ((n & 0xC0) != 0x80) return BAD;
    cp = cp << 6 | (n & 0x3F);
  }
  p += more;
  return cp;
}

const FontGlyph *find(const FontFace &f, uint32_t cp) {
  int lo = 0, hi = f.count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (f.glyphs[mid].cp == cp) return &f.glyphs[mid];
    if (f.glyphs[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}

// A lowercase letter the face lacks (the label face has capitals only) is
// drawn as its capital
const FontGlyph *findLetter(const FontFace &f, uint32_t cp) {
  const FontGlyph *g = find(f, cp);
  if (g) return g;
  if (cp >= 'a' && cp <= 'z') return find(f, cp - 'a' + 'A');
  if (cp == DOTLESS_I) return find(f, 'I');
  return nullptr;
}

// What one code point draws: up to two glyphs side by side (a ligature), or
// a glyph and a mark centred over or under it; width 0 with no glyph: unknown.
struct Shape {
  const FontGlyph *g = nullptr, *second = nullptr, *mark = nullptr;
  uint8_t w = 0;
  int8_t  gx = 0, sx = 0, mx = 0, my = 0;
};

Shape shapeOf(const FontFace &f, uint32_t cp) {
  Shape s;
  if ((s.g = find(f, cp))) {
    s.w = s.g->w;
    if (f.tabular && cp >= '0' && cp <= '9') {   // every digit as wide: the clock stays still
      s.gx = (f.tabular - s.g->w) >> 1;
      s.w  = f.tabular;
    }
    return s;
  }
  if (cp == OE_UPPER || cp == OE_LOWER) {
    // O and E sharing the stroke between them
    const bool upper = cp == OE_UPPER;
    s.g = findLetter(f, upper ? 'O' : 'o');
    s.second = findLetter(f, upper ? 'E' : 'e');
    if (!s.g || !s.second) return Shape();
    s.sx = s.g->w - 1;
    s.w  = s.sx + s.second->w;
    return s;
  }
  for (const Composed &c : COMPOSED) {
    if (c.cp != cp) continue;
    if (!f.marks) return Shape();
    const bool upper = c.base >= 'A' && c.base <= 'Z';
    // A lowercase i loses its dot under a mark
    s.g = c.base == 'i' && find(f, DOTLESS_I) ? find(f, DOTLESS_I) : findLetter(f, c.base);
    if (!s.g) return Shape();
    s.mark = c.mark != CEDIL && upper && f.capMarks ? &f.capMarks[c.mark] : &f.marks[c.mark];
    s.w  = s.g->w > s.mark->w ? s.g->w : s.mark->w;
    s.gx = (s.w - s.g->w) >> 1;
    s.mx = (s.w - s.mark->w) >> 1;
    s.my = c.mark == CEDIL ? f.cedilRow : f.markRow;
    return s;
  }
  if ((s.g = findLetter(f, cp))) s.w = s.g->w;
  return s;
}

void drawGlyph(Adafruit_GFX &out, const FontFace &f, const FontGlyph &g, int x, int y,
               uint16_t color) {
  unsigned bit = 0;
  for (int r = 0; r < g.h; r++) {
    const int py = y + g.top + r;
    for (int c = 0; c < g.w; c++, bit++) {
      const int px = x + c;
      if (px < 0 || py < 0 || px >= out.width() || py >= out.height()) continue;
      if (f.bits[g.offset + bit / 8] & (0x80 >> (bit % 8))) out.drawPixel(px, py, color);
    }
  }
}

void drawBox(Adafruit_GFX &out, int x, int y, int w, int h) {
  for (int r = 0; r < h; r++)
    for (int c = 0; c < w; c++) {
      if (r != 0 && r != h - 1 && c != 0 && c != w - 1) continue;   // an outline
      const int px = x + c, py = y + r;
      if (px < 0 || py < 0 || px >= out.width() || py >= out.height()) continue;
      out.drawPixel(px, py, TYPE_MISSING_COLOR);
    }
}

const uint8_t MISSING_W = 3;

// Lays the text out from x; draws it when out is given. Returns the width.
int run(const char *utf8, const FontFace &f, Adafruit_GFX *out, int x, int y, uint16_t color) {
  int cx = 0;
  bool any = false;
  for (const char *p = utf8 ? utf8 : ""; *p;) {
    const uint32_t cp = nextCodePoint(p);
    any = true;
    if (cp == ' ' || cp == NBSP) { cx += f.space + f.track; continue; }
    if (cp == NNBSP) { cx += 1 + f.track; continue; }
    const Shape s = shapeOf(f, cp);
    if (!s.g) {
      if (out) drawBox(*out, x + cx, y, MISSING_W, f.cap);
      cx += MISSING_W + f.track;
      continue;
    }
    if (out) {
      drawGlyph(*out, f, *s.g, x + cx + s.gx, y, color);
      if (s.second) drawGlyph(*out, f, *s.second, x + cx + s.sx, y, color);
      if (s.mark) drawGlyph(*out, f, *s.mark, x + cx + s.mx, y + s.my, color);
    }
    cx += s.w + f.track;
  }
  return any && cx > f.track ? cx - f.track : 0;
}

}  // namespace

bool hasGlyph(uint32_t cp, Face f) {
  return cp == ' ' || cp == NBSP || cp == NNBSP || shapeOf(FONT_FACES[(int)f], cp).g != nullptr;
}

uint16_t textWidth(const char *utf8, Face f) {
  return (uint16_t)run(utf8, FONT_FACES[(int)f], nullptr, 0, 0, 0);
}

void drawText(Adafruit_GFX &g, const char *utf8, int16_t x, int16_t y, Face f, uint16_t color,
              Align a) {
  const FontFace &face = FONT_FACES[(int)f];
  const int w = run(utf8, face, nullptr, 0, 0, 0);
  int left = x;
  if (a == Align::CENTER) left = x - (w >> 1);   // the odd pixel to the right, as the design rounds
  if (a == Align::RIGHT)  left = x - w;
  run(utf8, face, &g, left, y, color);
}
