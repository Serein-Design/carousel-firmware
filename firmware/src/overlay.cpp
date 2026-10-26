// SPDX-License-Identifier: AGPL-3.0-only
#include "overlay.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ease.h"
#include "grid.h"
#include "palette.h"
#include "type.h"

void Feedback::show(FeedbackKind kind, uint32_t now, int8_t dir) {
  if (on && now - eventAt >= FEEDBACK_HOLD_MS + FEEDBACK_FALL_MS) on = false;
  if (!on || kind != kind_) {
    on = rising = true;
    riseAt = now;
  }
  kind_ = kind;
  dir_ = dir;
  eventAt = now;
}

float Feedback::shown(uint32_t now) {
  if (!on) return 0;
  if (rising) {
    const uint32_t r = now - riseAt;
    if (r < FEEDBACK_RISE_MS) return easeOut((float)r / FEEDBACK_RISE_MS);
    rising = false;   // cleared once over, so the difference never comes round
  }
  const uint32_t e = now - eventAt;
  if (e < FEEDBACK_HOLD_MS) return 1;
  if (e < FEEDBACK_HOLD_MS + FEEDBACK_FALL_MS) return 1 - (float)(e - FEEDBACK_HOLD_MS) / FEEDBACK_FALL_MS;
  on = false;
  return 0;
}

ScreenPlace knobPlace(const ScreenList &l, uint8_t screen, bool portrait) {
  ScreenPlace p = {0, 0};
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    if (!screenOn(l, l.order[i]) || !screenFits(l.order[i], portrait)) continue;
    p.count++;
    if (l.order[i] == screen) p.pos = p.count;
  }
  return p;
}

// --- drawing helpers ---------------------------------------------------------

static const int16_t ROW = 7;   // a 3x5 label line and its 2-row gap

// A band of `rows` label lines: its height, a blank row above and below
static int16_t bandHeight(int rows) { return ROW * rows + 1; }

// Clipped to the canvas: a band on its way up is partly below it
static void fill(Adafruit_GFX &g, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > g.width()) w = g.width() - x;
  if (y + h > g.height()) h = g.height() - y;
  if (w > 0 && h > 0) g.fillRect(x, y, w, h, c);
}

// The colour faded toward black, for what fades instead of sliding
static uint16_t faded(uint16_t c, float k) {
  const unsigned s = (unsigned)(k * 256);
  return (uint16_t)((((c >> 11) * s >> 8) << 11) | ((((c >> 5) & 63) * s >> 8) << 5) | ((c & 31) * s >> 8));
}

// The sun of the brightness feedback, 5x5
static void drawSun(Adafruit_GFX &g, int16_t x, int16_t y, uint16_t c) {
  static const uint8_t SUN[5] = {0b10101, 0b01110, 0b11011, 0b01110, 0b10101};
  for (int16_t r = 0; r < 5; r++)
    for (int16_t b = 0; b < 5; b++)
      if (SUN[r] & (0x10 >> b) && x + b >= 0 && y + r >= 0 && x + b < g.width() && y + r < g.height())
        g.drawPixel(x + b, y + r, c);
}

// A bottom band risen by k: the band's top row, its fill and its line
static int16_t bottomBand(Adafruit_GFX &g, int16_t h, float k) {
  const int16_t y = g.height() - (int16_t)lroundf(h * k);
  fill(g, 0, y, g.width(), h, palette().off);
  fill(g, 0, y, g.width(), 1, palette().dim);
  return y;
}

// A name in at most two lines of maxW: whole, or split at its last space
// that leaves both halves fitting, or else cut short to fit. Names are
// panel-ASCII or UTF-8; a cut never splits a character.
struct NameLines {
  char line[2][32];
  int  count;
};
static void cutToFit(char *s, int16_t maxW) {
  size_t n = strlen(s);
  while (n && textWidth(s, Face::LABEL) > maxW) {
    do n--; while (n && ((uint8_t)s[n] & 0xC0) == 0x80);
    s[n] = 0;
  }
}
static NameLines fitName(const char *name, int16_t maxW, bool twoLines) {
  NameLines l = {};
  snprintf(l.line[0], sizeof(l.line[0]), "%s", name ? name : "");
  l.count = 1;
  if (textWidth(l.line[0], Face::LABEL) <= maxW) return l;
  char *space = twoLines ? strrchr(l.line[0], ' ') : nullptr;
  if (space) {
    snprintf(l.line[1], sizeof(l.line[1]), "%s", space + 1);
    *space = 0;
    if (textWidth(l.line[0], Face::LABEL) <= maxW && textWidth(l.line[1], Face::LABEL) <= maxW) {
      l.count = 2;
      return l;
    }
    *space = ' ';
    l.line[1][0] = 0;
  }
  cutToFit(l.line[0], maxW);
  return l;
}

static int16_t lineWidth(const Adafruit_GFX &g) { return g.width() - 2 * GRID_MARGIN; }
static bool portraitOf(const Adafruit_GFX &g) { return g.height() > g.width(); }

// --- brightness --------------------------------------------------------------

// F2: a bottom band, the sun, one step per level, "4/6". Portrait: the
// steps on a second row.
static void brightnessDots(Adafruit_GFX &g, uint8_t level, uint8_t levels, float k) {
  const Palette &p = palette();
  const bool tall = portraitOf(g);
  const int16_t y = bottomBand(g, bandHeight(tall ? 2 : 1), k);
  drawSun(g, GRID_MARGIN, y + 2, p.text);
  char n[8];
  snprintf(n, sizeof(n), "%u/%u", level + 1, levels);
  drawText(g, n, g.width() - GRID_MARGIN, y + 2, Face::LABEL, p.accent, Align::RIGHT);
  for (uint8_t j = 0; j < levels; j++) {
    const uint16_t c = j <= level ? p.text : p.dim;
    if (tall) fill(g, GRID_MARGIN + j * 5, y + 2 + ROW, 3, 3, c);
    else      fill(g, 10 + j * 7, y + 3, 5, 3, c);
  }
}

// F1: a 5 px gauge slides in on the right edge, one segment per level from
// the bottom, unlabelled
static void brightnessGauge(Adafruit_GFX &g, uint8_t level, uint8_t levels, float k) {
  const Palette &p = palette();
  const int16_t x = g.width() - (int16_t)lroundf(5 * k);
  fill(g, x, 0, 5, g.height(), p.off);
  const int16_t pitch = (g.height() - 2) / levels;
  for (uint8_t j = 0; j < levels; j++)
    fill(g, x + 1, g.height() - (j + 1) * pitch, 3, pitch - 1, j <= level ? p.text : p.dim);
}

// F3: the level in Pierre with the sun, centred on a black card, fading.
// The design dims the screen to 25 % instead; a screen that does not redraw
// every pixel each frame would keep the dimming, so the card stands in.
static void brightnessNumber(Adafruit_GFX &g, uint8_t level, uint8_t levels, float k) {
  const Palette &p = palette();
  char digit[4], total[6];
  snprintf(digit, sizeof(digit), "%u", level + 1);
  snprintf(total, sizeof(total), "/%u", levels);
  const int16_t dw = textWidth(digit, Face::PIERRE), tw = textWidth(total, Face::LABEL);
  const int16_t width = 5 + 2 + dw + 2 + tw;
  const int16_t x = (g.width() - width) / 2, top = (g.height() - 13) / 2;
  fill(g, x - 2, top - 2, width + 4, 17, p.off);
  drawSun(g, x, top + 4, faded(p.text, k));
  drawText(g, digit, x + 7, top, Face::PIERRE, faded(p.text, k));
  drawText(g, total, x + 7 + dw + 2, top + 8, Face::LABEL, faded(p.label, k));
}

void drawBrightnessFeedback(Adafruit_GFX &g, uint8_t style, uint8_t level, uint8_t levels, float k) {
  if (k <= 0 || !levels) return;
  if (level >= levels) level = levels - 1;
  switch (style) {
    case BRIGHTNESS_DOTS:   brightnessDots(g, level, levels, k); break;
    case BRIGHTNESS_GAUGE:  brightnessGauge(g, level, levels, k); break;
    case BRIGHTNESS_NUMBER: brightnessNumber(g, level, levels, k); break;
    default: break;   // BRIGHTNESS_NONE
  }
}

// --- knob --------------------------------------------------------------------

// N1: a bottom band, the name and "4/12" on one row; on two (the name may
// take two lines) when they do not fit side by side, always in portrait
static void knobBand(Adafruit_GFX &g, const KnobView &v, float k) {
  const Palette &p = palette();
  char count[16];   // two int16_t and the slash
  snprintf(count, sizeof(count), "%d/%d", v.place.pos, v.place.count);
  const int16_t lw = lineWidth(g), cw = textWidth(count, Face::LABEL);
  const NameLines n = fitName(v.name, lw, true);
  const bool oneRow = n.count == 1 && textWidth(n.line[0], Face::LABEL) + 4 + cw <= lw;
  const int rows = oneRow ? 1 : n.count + 1;
  const int16_t y = bottomBand(g, bandHeight(rows), k);
  for (int i = 0; i < n.count; i++)
    drawText(g, n.line[i], GRID_MARGIN, y + 2 + i * ROW, Face::LABEL, p.text);
  drawText(g, count, g.width() - GRID_MARGIN, y + 2 + (rows - 1) * ROW, Face::LABEL, p.label, Align::RIGHT);
}

// N2: the band with the screens before and after. Landscape: one row cut
// by the edges, sliding in from the side turned toward with the push
// (240 ms). Portrait, where nothing slides sideways: stacked, centred.
static void knobNeighbours(Adafruit_GFX &g, const KnobView &v, float k) {
  const Palette &p = palette();
  const int16_t cx = g.width() / 2;
  if (portraitOf(g)) {
    const int16_t lw = lineWidth(g);
    const NameLines cur = fitName(v.name, lw, true);
    const NameLines prev = fitName(v.prev, lw, false), next = fitName(v.next, lw, false);
    const int16_t y = bottomBand(g, bandHeight(cur.count + 2), k);
    drawText(g, prev.line[0], cx, y + 2, Face::LABEL, p.label, Align::CENTER);
    for (int i = 0; i < cur.count; i++)
      drawText(g, cur.line[i], cx, y + 2 + (1 + i) * ROW, Face::LABEL, p.text, Align::CENTER);
    drawText(g, next.line[0], cx, y + 2 + (1 + cur.count) * ROW, Face::LABEL, p.label, Align::CENTER);
    return;
  }
  const int16_t y = bottomBand(g, bandHeight(1), k);
  const float push = easeOut(v.since / 240.0f);
  const int16_t slide = (int16_t)lroundf((1 - push) * 28) * (v.dir < 0 ? -1 : 1);
  const int16_t w = textWidth(v.name, Face::LABEL), left = cx - w / 2 + slide;
  if (v.prev) drawText(g, v.prev, left - 6, y + 2, Face::LABEL, p.label, Align::RIGHT);
  drawText(g, v.name, left, y + 2, Face::LABEL, p.text);
  if (v.next) drawText(g, v.next, left + w + 6, y + 2, Face::LABEL, p.label);
}

// N3: no band, one dot per screen the knob reaches along the bottom, the current
// one in the accent, fading; the name on the label line for its first
// 800 ms. Dots that do not fit one row wrap to rows above, 2 rows apart.
static void knobDots(Adafruit_GFX &g, const KnobView &v, float k) {
  const Palette &p = palette();
  const int16_t lw = lineWidth(g), n = v.place.count;
  if (n <= 0) return;
  int16_t pitch = lw / n;
  if (pitch > 5) pitch = 5;
  if (pitch < 2) pitch = 2;
  const int16_t perRow = (lw - 1) / pitch + 1;
  const uint16_t on = faded(p.accent, k), off = faded(p.dim, k);
  for (int16_t i = 0; i < n; i++) {
    const int16_t row = i / perRow, col = i % perRow;
    const int16_t inRow = n - row * perRow < perRow ? n - row * perRow : perRow;
    const int16_t x0 = (g.width() - ((inRow - 1) * pitch + 1)) / 2;
    g.drawPixel(x0 + col * pitch, g.height() - 1 - 2 * row, i + 1 == v.place.pos ? on : off);
  }
  if (v.since >= 800) return;
  const NameLines name = fitName(v.name, lw, true);
  fill(g, 0, 0, g.width(), bandHeight(name.count), p.off);
  for (int i = 0; i < name.count; i++)
    drawText(g, name.line[i], g.width() / 2, GRID_LABEL_TOP + i * ROW, Face::LABEL, p.text, Align::CENTER);
}

void drawKnobFeedback(Adafruit_GFX &g, uint8_t style, const KnobView &v, float k) {
  if (k <= 0) return;
  switch (style) {
    case KNOB_BAND:       knobBand(g, v, k); break;
    case KNOB_NEIGHBOURS: knobNeighbours(g, v, k); break;
    case KNOB_DOTS:       knobDots(g, v, k); break;
    default: break;   // KNOB_NONE
  }
}
