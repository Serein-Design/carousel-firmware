// SPDX-License-Identifier: AGPL-3.0-only
#include "gallery_logic.h"

// The first screen of the list that is on and fits. screenListFill and the
// form's checks leave at least one per orientation; order[0] only guards a
// list that would break that promise.
uint8_t ScreenCursor::firstFitting() const {
  for (uint8_t p = 0; p < SCREEN_COUNT; p++)
    if (fits(lst.order[p])) return lst.order[p];
  return lst.order[0];
}

// The next screen in list order from `from`'s place, one way or the other,
// wrapping, that is on, fits and is not in skip; -1 when none but `from`.
int ScreenCursor::nextFitting(uint8_t from, int step, uint32_t skip) const {
  const int place = screenPlace(lst, from);
  for (int n = 1; n < SCREEN_COUNT; n++) {
    const uint8_t i = lst.order[((place + step * n) % SCREEN_COUNT + SCREEN_COUNT) % SCREEN_COUNT];
    if (fits(i) && !((skip >> i) & 1u)) return i;
  }
  return -1;
}

void ScreenCursor::byHand(uint32_t now) {
  held = saveDue = true;
  heldAt = touchedAt = now;
}

void ScreenCursor::begin(const ScreenList &l, uint16_t rotateSec, bool isPortrait, int stored,
                         uint32_t now) {
  *this = ScreenCursor();
  lst = l;
  rotate = rotateSec;
  portrait = isPortrait;
  // First boot: the clock, useful before anything is set up
  int start = stored;
  if (start < 0)
    for (uint8_t i = 0; i < SCREEN_COUNT; i++)
      if (SCREEN_INFO[i].name == &Strings::scrClock) start = i;
  cur = start >= 0 && start < SCREEN_COUNT && fits((uint8_t)start) ? (uint8_t)start : firstFitting();
  shownAt = now;
}

bool ScreenCursor::turn(int32_t detents, uint32_t now) {
  if (!detents) return false;
  const int step = detents > 0 ? 1 : -1;
  for (int32_t n = detents > 0 ? detents : -detents; n > 0; n--) {
    const int i = nextFitting(cur, step, 0);
    if (i < 0) break;
    cur = (uint8_t)i;
  }
  shownAt = now;
  byHand(now);
  return true;
}

void ScreenCursor::press(uint32_t now) {
  held = true;
  heldAt = now;
}

bool ScreenCursor::request(uint8_t screen, uint32_t now) {
  if (screen >= SCREEN_COUNT || !fits(screen)) return false;
  cur = screen;
  shownAt = now;
  byHand(now);
  return true;
}

bool ScreenCursor::bringOn(uint8_t screen, uint32_t now) {
  if (screen >= SCREEN_COUNT || !fits(screen)) return false;
  cur = screen;
  shownAt = now;
  press(now);
  return true;
}

bool ScreenCursor::tilt(bool isPortrait, uint32_t now) {
  if (isPortrait == portrait) return false;
  portrait = isPortrait;
  if (!fits(cur)) cur = firstFitting();
  shownAt = now;
  return true;
}

bool ScreenCursor::apply(const ScreenList &l, uint16_t rotateSec, uint32_t now) {
  // Checked again here, as request() is: the list came from another task
  if (!screenListValid(l) || !rotateOffered(rotateSec)) return false;
  const uint8_t before = cur;
  lst = l;
  rotate = rotateSec;
  shownAt = now;
  if (!fits(cur)) {
    // Next after its place in the new list; it is off, so never itself
    const int i = nextFitting(cur, 1, 0);
    cur = i >= 0 ? (uint8_t)i : firstFitting();
  }
  return cur != before;
}

void ScreenCursor::brightnessChanged(uint32_t now) {
  saveDue = true;
  touchedAt = now;
}

bool ScreenCursor::tick(uint32_t now, bool isCovered, uint32_t cannotRotate) {
  // Cleared once over, so a 49.7-day-old touch cannot come round as recent
  if (held && now - heldAt >= HOLD_MS) held = false;
  if (isCovered) {
    covered = true;
    return false;
  }
  if (covered) {
    covered = false;
    shownAt = now;
    return false;
  }
  if (!rotate || held || now - shownAt < rotate * 1000UL) return false;
  shownAt = now;
  const int i = nextFitting(cur, 1, cannotRotate);
  // Alone in the rotation: it keeps running, its animation is not restarted
  if (i < 0) return false;
  cur = (uint8_t)i;
  return true;
}

bool ScreenCursor::takeSave(uint32_t now) {
  if (!saveDue || now - touchedAt < SAVE_DELAY_MS) return false;
  saveDue = false;
  return true;
}

int32_t ScreenCursor::nextInSeconds(uint32_t now, uint32_t cannotRotate) const {
  if (!rotate || nextFitting(cur, 1, cannotRotate) < 0) return -1;
  const uint32_t interval = rotate * 1000UL;
  if (covered) return rotate;   // the count starts again when the screen shows
  const uint32_t counted = now - shownAt;
  uint32_t left = counted < interval ? interval - counted : 0;
  if (held && now - heldAt < HOLD_MS && HOLD_MS - (now - heldAt) > left)
    left = HOLD_MS - (now - heldAt);
  return (int32_t)((left + 999) / 1000);
}

float ScreenCursor::rotationFill(uint32_t now, uint32_t cannotRotate) const {
  if (!rotate || covered || nextFitting(cur, 1, cannotRotate) < 0) return -1;
  if (held && now - heldAt < HOLD_MS) return -1;
  const uint32_t interval = rotate * 1000UL, counted = now - shownAt;
  return counted >= interval ? 1 : (float)counted / interval;
}

uint8_t rampValue(uint8_t from, uint8_t to, uint32_t elapsedMs, uint32_t durationMs) {
  if (elapsedMs >= durationMs) return to;
  return (uint8_t)((int32_t)from + ((int32_t)to - from) * (int32_t)elapsedMs / (int32_t)durationMs);
}

uint8_t nightLevel(int secOfDay, int startHour, int endHour, uint8_t day, uint8_t night) {
  if (startHour == endHour) return day;
  const int DAY_SEC = 24 * 3600;
  const int length  = ((endHour - startHour + 24) % 24) * 3600;
  const int in      = ((secOfDay - startHour * 3600) % DAY_SEC + DAY_SEC) % DAY_SEC;
  if (in >= length) return day;
  if (in < NIGHT_RAMP_SEC) return rampValue(day, night, in, NIGHT_RAMP_SEC);
  const int left = length - in;
  if (left <= NIGHT_RAMP_SEC) return rampValue(night, day, NIGHT_RAMP_SEC - left, NIGHT_RAMP_SEC);
  return night;
}
