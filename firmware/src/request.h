// SPDX-License-Identifier: AGPL-3.0-only
// A request shows itself (direction §3, Requests): every route that changes
// the panel from outside calls requestShown(), and what the owner picked for
// that kind on Panel (settings.requestShow) decides what the panel adds to the
// change: a top band that scrolls a message twice, a corner chip with the
// source's icon, the image full screen for its play then back, or CODE on
// stage. Sources are told apart by icon, never by colour or a brand logo.
//
// requestShown() runs on any task: it posts to the gallery's queue, so the
// order lands after the screen change the same route posted just before it.
// The overlay is loop()'s, drawn on top of the frame after the screen each
// tick; it never asks the screen underneath to draw again. Hardware-free:
// host-tested in test_request.
#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

#include "feedback_style.h"
#include "palette.h"

// What a request adds to its change; all false: nothing (Off)
struct RequestPlan {
  bool band;            // the message in a top band, scrolled twice
  bool chip;            // the corner chip with the source's icon, 1.5 s
  bool backAfterPlay;   // IMAGE for one play of the image, then the screen before
  bool onStage;         // CODE comes on stage (a chip when it cannot)
};
// The kind's own way for Show, the chip for Discreet, nothing for Off. A
// stored value out of range counts as the default, Show.
RequestPlan requestPlan(RequestKind kind, uint8_t show);

// The sender's colour when it is not the API's white default
static const uint16_t REQUEST_DEFAULT_COLOR = 0xFFFF;

// What crosses from the route's task to loop(), through the gallery's queue
struct RequestOrder {
  RequestKind   kind;
  RequestSource from;
  RequestPlan   plan;
  uint16_t      color;     // the message's, REQUEST_DEFAULT_COLOR: the palette's text
  uint32_t      seconds;   // the message's least time on show; 0: two passes
  char          text[96];  // UTF-8, cut on a character boundary
};

// The one entry point. color and seconds are a message's (/api/notify, MQTT
// notify); text is the message, else what the request names (a log line).
void requestShown(RequestKind kind, RequestSource from, const char *text,
                  uint16_t color = REQUEST_DEFAULT_COLOR, uint32_t seconds = 0);

// The source's icon: 5 rows, w columns, bit 4 the leftmost
struct SourceIcon {
  uint8_t w;
  uint8_t rows[5];
};
const SourceIcon &sourceIcon(RequestSource from);

// The band: slides in from above in 200 ms, scrolls the text at 24 px/s
// from the right edge until it has left on the left, twice or for the
// message's seconds if longer, then slides away in 200 ms
static const uint32_t BAND_SLIDE_MS = 200;
static const uint32_t BAND_PX_PER_S = 24;
static const int16_t  BAND_H        = 13;   // rows 0-12, its rule on row 12
static const int16_t  BAND_TEXT_X   = 10;   // left of it, the source's icon
// Pixels in one pass across a canvas this wide: the text's width plus the span it crosses
uint32_t bandPassPx(const char *text, int16_t width);
uint32_t bandPassMs(const char *text, int16_t width);
uint16_t bandPasses(const char *text, int16_t width, uint32_t seconds);   // 2 at least
uint32_t bandDurationMs(const char *text, int16_t width, uint32_t seconds);
// The band's top row t ms in: -BAND_H before the slide, 0 while it holds
int16_t  bandTop(uint32_t t, uint32_t duration);
// The text's left column t ms in: width - 2 at the start of every pass
int16_t  bandTextX(const char *text, int16_t width, uint32_t t);

// The chip: top right, a 9x9 square, filled brighter for its first 300 ms
static const uint32_t CHIP_MS       = 1500;
static const uint32_t CHIP_PULSE_MS = 300;
static const int16_t  CHIP_SIZE     = 9;

// loop() only: the band and the chip a taken order starts, and their drawing
// on top of the frame. Draw returns false when nothing was drawn.
void requestOverlayStart(const RequestOrder &o, const RequestPlan &p, uint32_t now);
bool requestOverlayDraw(Adafruit_GFX &g, const Palette &c, uint32_t now);
void requestOverlayClear();
