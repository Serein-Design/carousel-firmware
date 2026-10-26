// SPDX-License-Identifier: AGPL-3.0-only
// What the panel draws over the screen when the brightness changes or the
// knob turns: the owner's style (feedback_style.h) on the system grid,
// rising, holding and falling on one curve. gallery.cpp draws it on top of
// the frame each tick, after the screen; nothing underneath is redrawn.
// Hardware-free: host-tested in test_overlay.
#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

#include "feedback_style.h"
#include "screen_list.h"

// The design's feedback durations
static const uint32_t FEEDBACK_RISE_MS = 120;
static const uint32_t FEEDBACK_HOLD_MS = 1500;   // from the last event, the rise included
static const uint32_t FEEDBACK_FALL_MS = 250;

enum class FeedbackKind : uint8_t { NONE, BRIGHTNESS, KNOB };

// The one feedback on show. A new event of the same kind holds it again
// without a second rise; another kind rises afresh in its place. Times are
// compared as uint32_t differences, so the wrap of millis() changes nothing.
class Feedback {
 public:
  void show(FeedbackKind kind, uint32_t now, int8_t dir = 0);   // dir: the knob's, +1 next, -1 previous
  float shown(uint32_t now);   // 0 hidden .. 1 fully up
  FeedbackKind kind() const { return kind_; }
  int8_t dir() const { return dir_; }
  uint32_t since(uint32_t now) const { return now - eventAt; }   // since the last event

 private:
  FeedbackKind kind_ = FeedbackKind::NONE;
  int8_t   dir_    = 0;
  bool     on      = false;
  bool     rising  = false;
  uint32_t riseAt  = 0;
  uint32_t eventAt = 0;
};

// A screen's place among the screens that are on and fit the orientation, in
// the running order: the ones the knob reaches, the count Stage shows
// (stagePlace). pos from 1; 0 when the screen is off or does not fit.
struct ScreenPlace {
  int16_t pos, count;
};
ScreenPlace knobPlace(const ScreenList &l, uint8_t screen, bool portrait);

// What a knob feedback names: the screen, the ones a turn would bring
// (nullptr: none), its place, the turn's direction and the time since it
struct KnobView {
  const char *name, *prev, *next;
  ScreenPlace place;
  int8_t      dir;
  uint32_t    since;
};

// k: the curve, 0 draws nothing. The canvas's size gives the orientation.
void drawBrightnessFeedback(Adafruit_GFX &g, uint8_t style, uint8_t level, uint8_t levels, float k);
void drawKnobFeedback(Adafruit_GFX &g, uint8_t style, const KnobView &v, float k);
