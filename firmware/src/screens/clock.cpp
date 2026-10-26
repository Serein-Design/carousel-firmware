// SPDX-License-Identifier: AGPL-3.0-only
// CLOCK in the Atelier faces and the palette's roles (DATE is with the other
// information screens, in info.cpp), and the network dot.
#include "display.h"
#include "screens/row_clip.h"
#include "screens/screens.h"
#include "clock_logic.h"
#include "gallery.h"
#include "grid.h"
#include "i18n.h"
#include "network.h"
#include "palette.h"
#include "settings.h"
#include "type.h"
#include "weather.h"
#include "weather_logic.h"

// ===========================================================================
// The network dot
// ===========================================================================

// Status dot in the top-right corner: green synced, orange connected, red offline
void drawNetworkStatus(uint32_t f) {
  uint16_t c;
  switch (networkState) {
    case NET_SYNCED:    c = display->color565(0, 160, 40);  break;
    case NET_CONNECTED: c = display->color565(210, 120, 0); break;
    default:            c = ((f / 20) & 1) ? display->color565(200, 0, 0)
                                           : display->color565(30, 0, 0);
  }
  display->fillRect(W() - 3, 1, 2, 2, c);
}

// ===========================================================================
// CLOCK: Pierre (C1), Cadran (C2) or Alternance (C3), as settings.clockStyle
// says, with the readouts settings.clockShow asks for (clock_logic.h). Only
// Pierre has a portrait layout: the others draw it there.
// ===========================================================================

// The seconds as a line that fills each minute: a ghost track, the accent
// over the seconds gone (the board's 42 px under the landscape digits)
static void drawSecondsLine(int16_t x, int16_t y, int16_t w, int sec, const Palette &p) {
  display->drawFastHLine(x, y, w, p.ghost);
  display->drawFastHLine(x, y, (int16_t)lroundf(sec / 59.0f * (w - 1)) + 1, p.accent);
}

struct ClockNow {
  struct tm t;
  int  week;           // ISO 8601
  bool outside;        // the weather is known
  ClockReadout read[READ_COUNT];
  uint8_t reads;
};

// The outside temperature in the owner's unit
static float outsideTemp() { return tempIn(weather.temp, settings.tempUnit); }

static void readClock(ClockNow &c, const struct tm &t) {
  c.t = t;
  char w[4];
  strftime(w, sizeof(w), "%V", &t);
  c.week = atoi(w);
  c.outside = weather.valid;
  c.reads = clockReadouts(settings.clockShow, c.outside, c.read);
}

// One readout as a line: "MON 19 OCT", "WEEK 43", "OUTSIDE 14°"
static void readoutLine(char *out, size_t size, const ClockNow &c, ClockReadout r) {
  if (r == READ_DATE) clockDateLine(out, size, tr(), c.t.tm_wday, c.t.tm_mday, c.t.tm_mon);
  else if (r == READ_WEEK) clockWeekLine(out, size, tr(), c.week);
  else clockOutsideLine(out, size, tr(), outsideTemp());
}

// Pierre's week and outside temperature on one line: side by side when both
// show (the test measures the widest pair), else the one centred
static void drawExtras(int16_t y, const ClockNow &c, const Palette &p) {
  const bool week = settings.clockShow & CLOCK_SHOW_WEEK;
  const bool outside = (settings.clockShow & CLOCK_SHOW_OUTSIDE) && c.outside;
  char buf[32];
  if (week && outside) {
    clockWeekLine(buf, sizeof(buf), tr(), c.week);
    drawText(*display, buf, GRID_MARGIN, y, Face::LABEL, p.label);
    clockDegrees(buf, sizeof(buf), outsideTemp());
    drawText(*display, buf, W() - GRID_MARGIN, y, Face::LABEL, p.label, Align::RIGHT);
  } else if (week || outside) {
    readoutLine(buf, sizeof(buf), c, week ? READ_WEEK : READ_OUTSIDE);
    drawText(*display, buf, W() / 2, y, Face::LABEL, p.label, Align::CENTER);
  }
}

// The time centred on the line; in 12 h, the AM/PM beside the figures' top
static void drawTime(const struct tm &t, int16_t y, const Palette &p) {
  char buf[8];
  clockTime(buf, sizeof(buf), t.tm_hour, t.tm_min, settings.clock12);
  const char *ampm = settings.clock12 ? clockAmPm(tr(), t.tm_hour) : nullptr;
  const TimePlace l = placeTime(textWidth(buf, Face::PIERRE), ampm, W() / 2, false);
  drawText(*display, buf, l.timeX, y, Face::PIERRE, p.text);
  if (ampm) drawText(*display, ampm, l.ampmX, y, Face::LABEL, p.label);
}

static void drawStone(const ClockNow &c, const Palette &p) {
  const StoneLayout l = stoneLayout(settings.clockShow, c.outside);
  char buf[32];
  if (l.dateOnLabel) {
    readoutLine(buf, sizeof(buf), c, READ_DATE);
    drawText(*display, buf, W() / 2, GRID_LABEL_TOP, Face::LABEL, p.label, Align::CENTER);
  }
  if (l.extrasOnLabel) drawExtras(GRID_LABEL_TOP, c, p);
  drawTime(c.t, GRID_MAIN_TOP, p);
  if (l.extrasOnFoot) drawExtras(GRID_FOOT_TOP, c, p);
  if (l.secondsRow >= 0) drawSecondsLine(11, l.secondsRow, 42, c.t.tm_sec, p);
}

// Portrait (the board's portrait rules): the weekday on the label line, hours
// over minutes with the seconds line between, then one line per readout, a
// hidden one leaving its row to the next. In 12 h the column of hours over
// minutes moves left to leave the AM/PM room beside the hours.
static void drawStoneTall(const ClockNow &c, const Palette &p) {
  const int16_t mid = W() / 2;
  const uint8_t show = settings.clockShow;
  char buf[24];
  if (show & CLOCK_SHOW_DATE)
    drawText(*display, tr().clkDays[c.t.tm_wday % 7], mid, GRID_PORTRAIT_LABEL_TOP, Face::LABEL,
             p.label, Align::CENTER);
  const char *ampm = settings.clock12 ? clockAmPm(tr(), c.t.tm_hour) : nullptr;
  const uint16_t column = textWidth("00", Face::PIERRE);   // tabular: any two figures
  const TimePlace l = placeTime(column, ampm, mid, true);
  const int16_t x = l.timeX + (column >> 1);
  clockHours(buf, sizeof(buf), c.t.tm_hour, settings.clock12);
  drawText(*display, buf, x, 10, Face::PIERRE, p.text, Align::CENTER);
  if (ampm) drawText(*display, ampm, l.ampmX, 10, Face::LABEL, p.label);
  if (show & CLOCK_SHOW_SECONDS) drawSecondsLine(l.timeX, 26, column, c.t.tm_sec, p);
  snprintf(buf, sizeof(buf), "%02d", c.t.tm_min);
  drawText(*display, buf, x, 30, Face::PIERRE, p.text, Align::CENTER);

  int16_t y = 46;
  for (uint8_t i = 0; i < c.reads; i++, y += 6) {
    if (c.read[i] == READ_DATE) clockDayMonth(buf, sizeof(buf), tr(), c.t.tm_mday, c.t.tm_mon);
    else if (c.read[i] == READ_WEEK) clockWeekLine(buf, sizeof(buf), tr(), c.week, true);
    else clockDegrees(buf, sizeof(buf), outsideTemp());
    drawText(*display, buf, mid, y, Face::LABEL, p.label, Align::CENTER);
  }
}

// The dial's column, centred on x: a label, a number in Pierre, a label
static const int16_t DIAL_COLUMN_X = 47;
static const int16_t DIAL_COLUMN_W = 30;   // from the dial's rim to the margin

static void drawDialColumn(const ClockNow &c, ClockReadout r, int16_t dy, Adafruit_GFX &g,
                           const Palette &p) {
  const char *top = tr().clkOutside;
  const char *bottom = nullptr;
  char big[12];
  if (r == READ_DATE) {
    top = tr().clkDays[c.t.tm_wday % 7];
    bottom = tr().clkMonths[c.t.tm_mon % 12];
    snprintf(big, sizeof(big), "%d", c.t.tm_mday);
  } else if (r == READ_WEEK) {
    top = tr().clkWeek;
    snprintf(big, sizeof(big), "%d", c.week);
  } else {
    clockDegrees(big, sizeof(big), outsideTemp());
    // "-12°" and "122°" are wider than the column in Pierre: the number
    // alone, under its label
    if (textWidth(big, Face::PIERRE) > DIAL_COLUMN_W)
      snprintf(big, sizeof(big), "%ld", lroundf(outsideTemp()));
  }
  drawText(g, top, DIAL_COLUMN_X, 3 + dy, Face::LABEL, p.label, Align::CENTER);
  drawText(g, big, DIAL_COLUMN_X, 10 + dy, Face::PIERRE, p.text, Align::CENTER);
  if (bottom) drawText(g, bottom, DIAL_COLUMN_X, 25 + dy, Face::LABEL, p.label, Align::CENTER);
}

static void drawHand(int16_t cx, int16_t cy, float frac, float len, int16_t ox, int16_t oy,
                     uint16_t color) {
  const float a = frac * 2 * PI - PI / 2;
  display->drawLine(cx + ox, cy + oy, cx + ox + lroundf(cosf(a) * len),
                    cy + oy + lroundf(sinf(a) * len), color);
}

// Cadran: the dial on the left, its readouts in the column on the right,
// taking turns as Alternance does when there are several; no readout
// centres the dial
static void drawDial(const ClockNow &c, const Palette &p) {
  const int16_t cx = c.reads ? 15 : W() / 2 - 1, cy = 15, r = 14;
  for (int a = 0; a < 360; a += 3) {
    const float rad = a * PI / 180;
    display->drawPixel(cx + lroundf(cosf(rad) * r), cy + lroundf(sinf(rad) * r), p.ghost);
  }
  for (int i = 0; i < 12; i++) {
    const float a = i / 12.0f * 2 * PI - PI / 2;
    display->drawPixel(cx + lroundf(cosf(a) * r), cy + lroundf(sinf(a) * r),
                       i % 3 ? p.dim : p.text);
  }
  const struct tm &t = c.t;
  const float hour = ((t.tm_hour % 12) + t.tm_min / 60.0f) / 12;
  drawHand(cx, cy, hour, 7, 0, 0, p.text);   // the hour hand, 2 px thick
  drawHand(cx, cy, hour, 6, 1, 0, p.text);
  drawHand(cx, cy, hour, 6, 0, 1, p.text);
  drawHand(cx, cy, (t.tm_min + t.tm_sec / 60.0f) / 60, 11, 0, 0, p.text);
  if (settings.clockShow & CLOCK_SHOW_SECONDS) {
    const float a = t.tm_sec / 60.0f * 2 * PI - PI / 2;
    display->drawPixel(cx + lroundf(cosf(a) * (r - 2)), cy + lroundf(sinf(a) * (r - 2)), p.accent);
  }
  display->drawPixel(cx, cy, p.accent);

  if (!c.reads) return;
  RowClip clip(0, GRID_STRIP_ROW - 1);
  const uint8_t travel = GRID_STRIP_ROW - 3;   // the column's height: one leaves as the next comes
  const CycleStep s = clockCycle(millis(), c.reads, travel);
  if (s.shift) drawDialColumn(c, c.read[s.before], s.shift - travel, clip, p);
  drawDialColumn(c, c.read[s.now], s.shift, clip, p);
}

// Alternance: the digits on top, the seconds line under them, one foot slot
// taking turns between the readouts
static void drawCycle(const ClockNow &c, const Palette &p) {
  drawTime(c.t, 3, p);
  if (settings.clockShow & CLOCK_SHOW_SECONDS) drawSecondsLine(11, 19, 42, c.t.tm_sec, p);
  if (!c.reads) return;
  RowClip clip(20, GRID_STRIP_ROW - 1);
  const uint8_t travel = 9;
  const CycleStep s = clockCycle(millis(), c.reads, travel);
  char buf[32];
  if (s.shift) {
    readoutLine(buf, sizeof(buf), c, c.read[s.before]);
    drawText(clip, buf, W() / 2, 23 + s.shift - travel, Face::LABEL, p.label, Align::CENTER);
  }
  readoutLine(buf, sizeof(buf), c, c.read[s.now]);
  drawText(clip, buf, W() / 2, 23 + s.shift, Face::LABEL, p.label, Align::CENTER);
}

// No time yet: dim dashes in the digits' place and no word, as the time now
// comes within seconds of the Wi-Fi (network.cpp)
static void drawWaiting(const Palette &p) {
  if (!portrait) {
    const int16_t y = settings.clockStyle == CLOCK_CYCLE ? 3 : GRID_MAIN_TOP;   // drawCycle's row
    drawText(*display, "--:--", W() / 2, y, Face::PIERRE, p.dim, Align::CENTER);
  } else {
    drawText(*display, "--", W() / 2, 10, Face::PIERRE, p.dim, Align::CENTER);
    drawText(*display, "--", W() / 2, 30, Face::PIERRE, p.dim, Align::CENTER);
  }
}

void screenClock(uint32_t f, bool init) {
  const Palette &p = palette();
  display->clearScreen();
  struct tm t;
  if (!timeValid || !getLocalTime(&t, 0)) {
    drawWaiting(p);
    return;
  }
  ClockNow c;
  readClock(c, t);
  if (portrait) drawStoneTall(c, p);
  else if (settings.clockStyle == CLOCK_DIAL) drawDial(c, p);
  else if (settings.clockStyle == CLOCK_CYCLE) drawCycle(c, p);
  else drawStone(c, p);
}
