// SPDX-License-Identifier: AGPL-3.0-only
#include "request.h"

#include <string.h>

#include "gallery.h"
#include "grid.h"
#include "settings.h"
#include "type.h"

RequestPlan requestPlan(RequestKind kind, uint8_t show) {
  RequestPlan p = {};
  if (show >= REQUEST_SHOW_COUNT) show = REQUEST_SHOW;
  if (show == REQUEST_OFF) return p;
  if (show == REQUEST_DISCREET) {
    p.chip = true;
    return p;
  }
  switch (kind) {
    case REQUEST_MESSAGE: p.band = true; break;
    case REQUEST_IMAGE:   p.backAfterPlay = true; break;
    case REQUEST_COMMAND: p.chip = true; break;
    case REQUEST_AGENT:   p.onStage = true; break;
    default: break;
  }
  return p;
}

namespace {

bool anything(const RequestPlan &p) { return p.band || p.chip || p.backAfterPlay || p.onStage; }

// UTF-8 copied a whole character at a time, so a cut never leaves half of
// one; line breaks and tabs become spaces, the band has one line
void copyText(const char *in, char *out, size_t size) {
  size_t n = 0;
  const uint8_t *p = (const uint8_t *)(in ? in : "");
  while (*p) {
    const size_t len = *p >= 0xF0 ? 4 : *p >= 0xE0 ? 3 : *p >= 0xC0 ? 2 : 1;
    if (n + len > size - 1) break;
    size_t have = 1;
    while (have < len && p[have]) have++;
    if (have < len) break;                   // cut short by the end: dropped
    for (size_t i = 0; i < len; i++) {
      const uint8_t c = *p++;
      out[n++] = c == '\n' || c == '\r' || c == '\t' ? ' ' : (char)c;
    }
  }
  out[n] = '\0';
}

}  // namespace

void requestShown(RequestKind kind, RequestSource from, const char *text, uint16_t color,
                  uint32_t seconds) {
  if (kind >= REQUEST_KIND_COUNT) return;
  RequestOrder o = {};
  o.plan = requestPlan(kind, settings.requestShow[kind]);
  if (!anything(o.plan)) return;   // Off: the change alone
  o.kind    = kind;
  o.from    = from < SOURCE_COUNT ? from : SOURCE_API;
  o.color   = color;
  o.seconds = seconds;
  copyText(text, o.text, sizeof(o.text));
  // By kind and source only: the text can be a message (a door code) or a name
  if (!galleryRequest(o))
    Serial.printf("[REQUEST] not shown, the queue is full: kind %u from %u\n", (unsigned)o.kind,
                  (unsigned)o.from);
}

// --- Icons ------------------------------------------------------------------------

namespace {
// Drawn for the panel, never taken from a brand
const SourceIcon ICONS[SOURCE_COUNT] = {
  {3, {0x1C, 0x14, 0x14, 0x1C, 0x08}},   // web: the settings page, on a phone
  {5, {0x0A, 0x0A, 0x11, 0x0A, 0x0A}},   // API: two braces
  {5, {0x04, 0x0E, 0x1F, 0x0A, 0x0E}},   // Home Assistant: a house
  {5, {0x10, 0x08, 0x04, 0x08, 0x17}},   // agent: a prompt and its cursor
};
}  // namespace

const SourceIcon &sourceIcon(RequestSource from) {
  return ICONS[from < SOURCE_COUNT ? from : SOURCE_API];
}

// --- The band's timing ------------------------------------------------------------

// From the right margin to the icon's columns
static int16_t bandSpan(int16_t width) { return width - GRID_MARGIN - BAND_TEXT_X; }

uint32_t bandPassPx(const char *text, int16_t width) {
  return textWidth(text, Face::BODY) + (uint32_t)bandSpan(width);
}

uint32_t bandPassMs(const char *text, int16_t width) {
  return bandPassPx(text, width) * 1000 / BAND_PX_PER_S;
}

uint16_t bandPasses(const char *text, int16_t width, uint32_t seconds) {
  const uint32_t pass = bandPassMs(text, width);
  const uint32_t n = (seconds * 1000 + pass - 1) / pass;
  return n < 2 ? 2 : (uint16_t)n;
}

uint32_t bandDurationMs(const char *text, int16_t width, uint32_t seconds) {
  return 2 * BAND_SLIDE_MS + bandPasses(text, width, seconds) * bandPassMs(text, width);
}

int16_t bandTop(uint32_t t, uint32_t duration) {
  if (t >= duration) return -BAND_H;
  const uint32_t left = duration - t;
  const uint32_t in = t < left ? t : left;   // into the slide in, or before the end of the slide out
  if (in >= BAND_SLIDE_MS) return 0;
  return (int16_t)(-BAND_H + BAND_H * (int32_t)in / (int32_t)BAND_SLIDE_MS);
}

int16_t bandTextX(const char *text, int16_t width, uint32_t t) {
  const uint32_t e = t > BAND_SLIDE_MS ? t - BAND_SLIDE_MS : 0;
  // Within the pass in ms first: every pass starts again at the right margin
  const uint32_t off = e % bandPassMs(text, width) * BAND_PX_PER_S / 1000;
  return (int16_t)(width - GRID_MARGIN - (int32_t)off);
}

// --- The overlay --------------------------------------------------------------------

namespace {

struct Band {
  bool          on      = false;
  uint32_t      at      = 0;
  RequestSource from    = SOURCE_API;
  uint16_t      color   = REQUEST_DEFAULT_COLOR;
  uint32_t      seconds = 0;
  char          text[sizeof(RequestOrder::text)] = "";
};
struct Chip {
  bool          on   = false;
  uint32_t      at   = 0;
  RequestSource from = SOURCE_API;
};
Band band;
Chip chip;

// Only pixels on the canvas: the band slides in from above it
void dot(Adafruit_GFX &g, int16_t x, int16_t y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < g.width() && y < g.height()) g.drawPixel(x, y, c);
}
void fill(Adafruit_GFX &g, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  for (int16_t j = y; j < y + h; j++)
    for (int16_t i = x; i < x + w; i++) dot(g, i, j, c);
}
// Centred in the 5 columns from x
void icon(Adafruit_GFX &g, RequestSource from, int16_t x, int16_t y, uint16_t c) {
  const SourceIcon &s = sourceIcon(from);
  x += (5 - s.w) / 2;
  for (int r = 0; r < 5; r++)
    for (int k = 0; k < s.w; k++)
      if (s.rows[r] >> (4 - k) & 1) dot(g, x + k, y + r, c);
}

bool drawBand(Adafruit_GFX &g, const Palette &c, uint32_t now) {
  if (!band.on) return false;
  const int16_t w = g.width();
  const uint32_t t = now - band.at, d = bandDurationMs(band.text, w, band.seconds);
  if (t >= d) {
    band.on = false;
    return false;
  }
  const int16_t y = bandTop(t, d);
  fill(g, 0, y, w, BAND_H - 1, c.off);
  fill(g, 0, y + BAND_H - 1, w, 1, c.dim);
  const uint16_t ink = band.color == REQUEST_DEFAULT_COLOR ? c.text : band.color;
  drawText(g, band.text, bandTextX(band.text, w, t), y + 2, Face::BODY, ink);
  // The icon's columns: the text slides under them, out of sight
  fill(g, 0, y, BAND_TEXT_X, BAND_H - 1, c.off);
  icon(g, band.from, 2, y + 4, c.accent);
  return true;
}

bool drawChip(Adafruit_GFX &g, const Palette &c, uint32_t now) {
  if (!chip.on) return false;
  const uint32_t t = now - chip.at;
  if (t >= CHIP_MS) {
    chip.on = false;
    return false;
  }
  // It arrives as a brief brighter fill, inside its own square: a 1 px
  // outline is the knob's mode marker, and nothing outside the chip stays
  // behind on the screen when it goes
  const int16_t x = g.width() - CHIP_SIZE;
  fill(g, x, 0, CHIP_SIZE, CHIP_SIZE, t < CHIP_PULSE_MS ? c.dim : c.off);
  icon(g, chip.from, x + 2, 2, c.accent);
  return true;
}

}  // namespace

void requestOverlayStart(const RequestOrder &o, const RequestPlan &p, uint32_t now) {
  if (p.band) {
    band.on      = true;
    band.at      = now;
    band.from    = o.from;
    band.color   = o.color;
    band.seconds = o.seconds;
    memcpy(band.text, o.text, sizeof(band.text));
    band.text[sizeof(band.text) - 1] = '\0';
  }
  if (p.chip) {
    chip.on   = true;
    chip.at   = now;
    chip.from = o.from;
  }
}

bool requestOverlayDraw(Adafruit_GFX &g, const Palette &c, uint32_t now) {
  const bool drawn = drawBand(g, c, now);
  return drawChip(g, c, now) || drawn;   // the chip over the band's right end
}

void requestOverlayClear() {
  band.on = false;
  chip.on = false;
}
