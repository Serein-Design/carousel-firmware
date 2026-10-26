// SPDX-License-Identifier: AGPL-3.0-only
// Which screen is on show and when the rotation moves on, without the
// hardware: one place decides the screen, the hold after a touch and when
// the screen on show is stored. Checked on the PC by
// test/test_gallery_logic.
#pragma once

#include <stdint.h>

#include "screen_list.h"

static const uint32_t HOLD_MS       = 30000;   // automatic changes wait this long after a touch or a request
static const uint32_t SAVE_DELAY_MS = 3000;    // the screen on show is stored once the knob settled

// The panel's brightness moves instead of jumping: over this long, one step
// at most every BRIGHTNESS_STEP_MS (each step rebuilds the panel's tables).
static const uint32_t BRIGHTNESS_RAMP_MS = 250;
static const uint32_t BRIGHTNESS_STEP_MS = 25;
// The night comes in over the first, and goes out over the last, of its hours
static const int NIGHT_RAMP_SEC = 180;

// The level elapsedMs into a ramp from one level to another lasting durationMs
uint8_t rampValue(uint8_t from, uint8_t to, uint32_t elapsedMs, uint32_t durationMs);

// The level the night asks for secOfDay (seconds since midnight): day outside
// the hours [start, end) (they may cross midnight, equal means none), night
// within, the day-to-night ramp over the first NIGHT_RAMP_SEC and back over the
// last. A touch waking the panel is gallery.cpp's, not here.
uint8_t nightLevel(int secOfDay, int startHour, int endHour, uint8_t day, uint8_t night);

// gallery.cpp feeds it the knob, the tilt, requests and saves, and draws what
// it says. Every time is compared as a difference of uint32_t, so the 49.7-day
// wrap of millis() changes nothing; the hold is cleared once over, so a touch
// that old cannot block again when the difference comes round.
class ScreenCursor {
public:
  // stored: firmware index, -1 = never stored -> CLOCK if on, else the list's
  // first that fits. The count starts now.
  void begin(const ScreenList &l, uint16_t rotateSec, bool portrait, int stored, uint32_t now);
  uint8_t current() const { return cur; }
  const ScreenList &list() const { return lst; }
  uint16_t rotateSec() const { return rotate; }
  bool isOn(uint8_t screen) const { return screen < SCREEN_COUNT && screenOn(lst, screen); }
  // The screen a turn of one step brings (+1 next, -1 previous); -1: none other
  int neighbour(int step) const { return nextFitting(cur, step, 0); }

  // Each returns true when the screen is shown afresh: the caller restarts
  // its animation.
  // On and fitting screens, list order, wrapping; hold; save due.
  // Any turn shows the screen afresh, even when no other screen can be reached.
  bool turn(int32_t detents, uint32_t now);
  // Hold only: the button's meaning (brightness, sessions, night wake) stays gallery.cpp's
  void press(uint32_t now);
  // False: off or other orientation, nothing changes. Else hold; save due.
  bool request(uint8_t screen, uint32_t now);
  // An agent's request (the hooks, unauthenticated): as request, hold, but no
  // save, so a hook never rewrites the stored start screen or brightness
  bool bringOn(uint8_t screen, uint32_t now);
  // Keep the screen if on and fits, else the list's first that does; count
  // again; no hold, no save. True whenever the orientation changed.
  bool tilt(bool portrait, uint32_t now);
  // A stored save, list and interval at once; count again; no save.
  // True when the screen on show was turned off and the next one took its place.
  // An invalid list (screenListValid) or an interval not offered changes nothing.
  bool apply(const ScreenList &l, uint16_t rotateSec, uint32_t now);
  void brightnessChanged(uint32_t now);   // save due, no hold

  // Once per frame. covered: an overlay hides the screen (no change; the
  // first uncovered frame counts again). cannotRotate: bit i, screen i is
  // skipped by the rotation (the knob still reaches it). True when the
  // rotation changed screen.
  bool tick(uint32_t now, bool covered, uint32_t cannotRotate);
  bool takeSave(uint32_t now);            // true once, SAVE_DELAY_MS after the last user cause
  // Seconds, rounded up, before the next automatic change; -1 when rotation
  // is off or no other screen can rotate in this orientation
  int32_t nextInSeconds(uint32_t now, uint32_t cannotRotate) const;
  // How far the screen on show is into the interval, 0 to 1, for the status
  // strip's line; negative (nothing to draw) when the rotation is off, held
  // after a touch or a request, covered, or has no other screen to bring
  float rotationFill(uint32_t now, uint32_t cannotRotate) const;

private:
  bool fits(uint8_t screen) const { return isOn(screen) && screenFits(screen, portrait); }
  uint8_t firstFitting() const;
  int nextFitting(uint8_t from, int step, uint32_t skip) const;   // -1: none but from
  void byHand(uint32_t now);                                      // hold, save due

  ScreenList lst;
  uint16_t rotate   = 0;
  bool     portrait = false;
  uint8_t  cur      = 0;
  uint32_t shownAt  = 0;      // the count's start
  bool     held     = false;
  uint32_t heldAt   = 0;
  bool     saveDue  = false;
  uint32_t touchedAt = 0;     // last user cause, for the save
  bool     covered  = false;  // the last tick was covered
};
