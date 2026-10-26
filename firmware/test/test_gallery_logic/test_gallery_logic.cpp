// SPDX-License-Identifier: AGPL-3.0-only
// The screen on show and the rotation clock without the hardware: the knob,
// the tilt, requests and saves follow the owner's list; the rotation waits
// for its interval and for the hold after a touch, stops under an overlay,
// never stores anything, and keeps time across the wrap of millis(); the
// screens a turn would bring, for the knob's feedback.
//
//   pio test -e native -f test_gallery_logic
#include <unity.h>

#include <string>

#include "../../src/i18n.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/gallery_logic.cpp"

void setUp() {}
void tearDown() {}

static uint8_t idx(const char *name) {
  const int i = screenFind(name);
  TEST_ASSERT_TRUE_MESSAGE(i >= 0, name);
  return (uint8_t)i;
}

// The named screens first, in that order, on unless marked '-'; every other
// screen after them, off: only the named ones take part.
static ScreenList only(const char *names) {
  ScreenList l = screenListParse(names);
  l.on = 0;
  std::string s = names;
  size_t start = 0;
  while (start <= s.size()) {
    size_t end = s.find(',', start);
    if (end == std::string::npos) end = s.size();
    const std::string n = s.substr(start, end - start);
    if (!n.empty() && n[0] != '-') l.on |= 1u << idx(n.c_str());
    start = end + 1;
  }
  return l;
}

static uint32_t bit(const char *name) { return 1u << idx(name); }

// Ticks every 100 ms from now until the rotation changes screen or limit ms
// passed; returns the ms elapsed, or -1. now moves on.
static long ticksUntilChange(ScreenCursor &c, uint32_t &now, uint32_t limit,
                             uint32_t cannotRotate = 0) {
  for (uint32_t t = 0; t <= limit; t += 100, now += 100)
    if (c.tick(now, false, cannotRotate)) return (long)t;
  return -1;
}

// Changes at "at" ms after now, to within one tick, and not before
static void assertChangeAt(ScreenCursor &c, uint32_t &now, uint32_t at, uint32_t cannotRotate = 0) {
  const long t = ticksUntilChange(c, now, at + 1000, cannotRotate);
  TEST_ASSERT_TRUE_MESSAGE(t >= 0, "no change");
  TEST_ASSERT_GREATER_OR_EQUAL((long)at, t);
  TEST_ASSERT_LESS_OR_EQUAL((long)at + 100, t);
}

// --- Start ------------------------------------------------------------------------

static void test_start_screen() {
  ScreenCursor c;
  c.begin(screenListDefault(), 0, false, idx("FIRE"), 0);
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());

  c.begin(only("DATE,FIRE,-STARS"), 0, false, idx("STARS"), 0);   // stored, now off
  TEST_ASSERT_EQUAL(idx("DATE"), c.current());

  c.begin(only("CODE,CAT,DATE"), 0, true, idx("CODE"), 0);        // stored for landscape
  TEST_ASSERT_EQUAL(idx("CAT"), c.current());

  c.begin(screenListDefault(), 0, false, -1, 0);                    // never stored
  TEST_ASSERT_EQUAL(idx("CLOCK"), c.current());

  c.begin(only("DATE,-CLOCK,FIRE"), 0, false, -1, 0);              // never stored, CLOCK off
  TEST_ASSERT_EQUAL(idx("DATE"), c.current());
}

// --- Knob ---------------------------------------------------------------------------

static void test_turn_follows_the_list_both_ways() {
  ScreenCursor c;
  c.begin(only("STARS,DATE,FIRE"), 0, false, idx("DATE"), 0);
  TEST_ASSERT_TRUE(c.turn(1, 10));
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());
  c.turn(1, 20);
  TEST_ASSERT_EQUAL(idx("STARS"), c.current());   // wraps past the end
  c.turn(-1, 30);
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());    // wraps past the start
  c.turn(-1, 40);
  TEST_ASSERT_EQUAL(idx("DATE"), c.current());
  c.turn(-1, 50);
  TEST_ASSERT_EQUAL(idx("STARS"), c.current());
  c.turn(3, 60);
  TEST_ASSERT_EQUAL(idx("STARS"), c.current());   // three detents, a full turn of three
  c.turn(2, 70);
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());
  TEST_ASSERT_FALSE(c.turn(0, 80));
}

static void test_turn_skips_off_and_other_orientation() {
  ScreenCursor c;
  c.begin(only("STARS,-DATE,CAT,FIRE"), 0, false, idx("STARS"), 0);
  c.turn(1, 10);
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());

  c.begin(only("STARS,-DATE,CODE,CAT,FIRE"), 0, true, idx("STARS"), 0);
  c.turn(1, 10);
  TEST_ASSERT_EQUAL(idx("CAT"), c.current());
  c.turn(-1, 20);
  TEST_ASSERT_EQUAL(idx("STARS"), c.current());
  c.turn(-1, 30);
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());
}

static void test_turn_reaches_a_screen_that_cannot_rotate() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("STARS,IMAGE,FIRE"), 10, false, idx("STARS"), now);
  c.tick(now, false, bit("IMAGE"));
  c.turn(1, now);
  TEST_ASSERT_EQUAL(idx("IMAGE"), c.current());
}

// --- Tilt, saves, requests, the button ---------------------------------------------

static void test_tilt_keeps_or_takes_the_first() {
  ScreenCursor c;
  c.begin(only("CODE,DATE,CAT,FIRE"), 0, false, idx("DATE"), 0);
  TEST_ASSERT_TRUE(c.tilt(true, 10));
  TEST_ASSERT_EQUAL(idx("DATE"), c.current());   // on and drawn for both
  c.turn(1, 20);
  TEST_ASSERT_EQUAL(idx("CAT"), c.current());
  TEST_ASSERT_TRUE(c.tilt(false, 30));
  TEST_ASSERT_EQUAL(idx("CODE"), c.current());   // CAT is portrait only: the list's first
  TEST_ASSERT_FALSE(c.tilt(false, 40));          // no change of orientation, nothing to do
}

static void test_apply_moves_off_the_screen_turned_off() {
  ScreenCursor c;
  c.begin(only("STARS,DATE,FIRE,CODE"), 0, false, idx("DATE"), 0);
  TEST_ASSERT_TRUE(c.apply(only("STARS,-DATE,CAT,FIRE"), 0, 5000));
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());   // after DATE's place, CAT skipped (portrait)
  TEST_ASSERT_FALSE(c.takeSave(5000 + SAVE_DELAY_MS));
  TEST_ASSERT_FALSE(c.takeSave(60000));

  // Off at the end of the list: the next one wraps to the start
  TEST_ASSERT_TRUE(c.apply(only("STARS,DATE,-FIRE"), 0, 70000));
  TEST_ASSERT_EQUAL(idx("STARS"), c.current());

  // Still on: it stays, and its animation goes on
  TEST_ASSERT_FALSE(c.apply(only("DATE,STARS"), 0, 80000));
  TEST_ASSERT_EQUAL(idx("STARS"), c.current());
}

static void test_apply_all_at_once() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(screenListDefault(), 0, false, idx("FIRE"), now);
  const ScreenList l = only("FIRE,DATE,STARS");
  c.apply(l, 10, now);
  TEST_ASSERT_EQUAL(10, c.rotateSec());
  TEST_ASSERT_TRUE(screenListSameView(l, c.list()));
  assertChangeAt(c, now, 10000);
  TEST_ASSERT_EQUAL(idx("DATE"), c.current());
}

// The loop checks a queued save again, as it does a request: a list that is
// not every screen once, or an interval not offered, changes nothing
static void test_apply_refuses_an_invalid_list() {
  ScreenCursor c;
  c.begin(only("STARS,DATE"), 10, false, idx("DATE"), 0);
  const ScreenList before = c.list();
  ScreenList bad = screenListDefault();
  bad.order[0] = 40;
  bad.on = 0;
  TEST_ASSERT_FALSE(c.apply(bad, 10, 1000));
  TEST_ASSERT_EQUAL(idx("DATE"), c.current());
  TEST_ASSERT_TRUE(screenListSameView(before, c.list()));
  TEST_ASSERT_FALSE(c.apply(only("STARS,DATE"), 45, 1000));
  TEST_ASSERT_EQUAL(10, c.rotateSec());
  TEST_ASSERT_TRUE(c.current() < SCREEN_COUNT);
}

static void test_request_refuses_off_and_other_orientation() {
  ScreenCursor c;
  c.begin(only("STARS,-DATE,CODE,CAT"), 0, false, idx("STARS"), 0);
  TEST_ASSERT_FALSE(c.request(idx("DATE"), 5000));
  TEST_ASSERT_FALSE(c.request(idx("CAT"), 5000));
  TEST_ASSERT_FALSE(c.request(SCREEN_COUNT, 5000));
  TEST_ASSERT_EQUAL(idx("STARS"), c.current());
  TEST_ASSERT_FALSE(c.takeSave(5000 + SAVE_DELAY_MS));
  TEST_ASSERT_TRUE(c.request(idx("CODE"), 10000));
  TEST_ASSERT_EQUAL(idx("CODE"), c.current());
  TEST_ASSERT_TRUE(c.isOn(idx("CODE")));
  TEST_ASSERT_FALSE(c.isOn(idx("DATE")));
}

static void test_press_keeps_the_screen() {
  for (uint16_t rotate : {0, 10}) {
    ScreenCursor c;
    c.begin(only("STARS,DATE"), rotate, false, idx("STARS"), 0);
    c.press(5000);
    TEST_ASSERT_EQUAL(idx("STARS"), c.current());
    }
}

// --- Rotation -------------------------------------------------------------------------

static void test_knob_only_never_changes() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(screenListDefault(), 0, false, idx("FIRE"), now);
  TEST_ASSERT_EQUAL(-1, ticksUntilChange(c, now, 3600000UL));
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());
}

static void test_rotation_follows_the_list() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("FIRE,CAT,STARS,-DATE,CODE,BALL"), 10, false, idx("FIRE"), now);
  for (const char *next : {"STARS", "CODE", "BALL", "FIRE", "STARS"}) {
    assertChangeAt(c, now, 10000);
    TEST_ASSERT_EQUAL_MESSAGE(idx(next), c.current(), next);
  }
}

static void test_first_change_one_interval_after_start_and_save() {
  ScreenCursor c;
  uint32_t now = 1000;
  c.begin(only("FIRE,STARS"), 20, false, idx("FIRE"), now);
  assertChangeAt(c, now, 20000);
  now += 5000;
  c.tick(now, false, 0);
  c.apply(only("FIRE,STARS"), 20, now);   // any save counts again
  assertChangeAt(c, now, 20000);
}

static void test_hold_after_turn() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("FIRE,STARS,DATE"), 10, false, idx("FIRE"), now);
  now = 5000;
  c.turn(1, now);
  assertChangeAt(c, now, HOLD_MS);
  TEST_ASSERT_EQUAL(idx("DATE"), c.current());
}

static void test_hold_after_press() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("FIRE,STARS"), 60, false, idx("FIRE"), now);
  TEST_ASSERT_EQUAL(-1, ticksUntilChange(c, now, 49900));
  c.press(now);                                // 50 s after it appeared
  assertChangeAt(c, now, HOLD_MS);

  c.begin(only("FIRE,STARS"), 60, false, idx("FIRE"), now = 0);
  TEST_ASSERT_EQUAL(-1, ticksUntilChange(c, now, 9900));
  c.press(now);                                // 10 s after: the interval decides
  assertChangeAt(c, now, 50000);
}

static void test_hold_after_request() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("FIRE,STARS,DATE"), 10, false, idx("FIRE"), now);
  now = 3000;
  c.request(idx("STARS"), now);
  assertChangeAt(c, now, HOLD_MS);
  TEST_ASSERT_EQUAL(idx("DATE"), c.current());
}

// An agent's request (an unauthenticated hook) brings CODE on stage and
// holds the rotation as a request does, but stores nothing: neither the
// start screen nor the brightness
static void test_an_agent_request_holds_but_saves_nothing() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("FIRE,CODE,DATE,CAT"), 10, false, idx("FIRE"), now);
  TEST_ASSERT_FALSE(c.bringOn(idx("CAT"), 1000));   // other orientation
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());
  now = 3000;
  TEST_ASSERT_TRUE(c.bringOn(idx("CODE"), now));
  TEST_ASSERT_EQUAL(idx("CODE"), c.current());
  TEST_ASSERT_FALSE(c.takeSave(now + SAVE_DELAY_MS));
  assertChangeAt(c, now, HOLD_MS);
  TEST_ASSERT_EQUAL(idx("DATE"), c.current());
  TEST_ASSERT_FALSE(c.takeSave(now + 60000));
}

static void test_hold_with_a_long_interval() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("FIRE,STARS,DATE"), 300, false, idx("FIRE"), now);
  now = 1000;
  c.turn(1, now);
  assertChangeAt(c, now, 300000);
}

static void test_tilt_counts_again_without_hold() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), now);
  TEST_ASSERT_EQUAL(-1, ticksUntilChange(c, now, 6900));
  c.tilt(true, now);                           // 7 s in: counts 10 s from here, no 30 s hold
  assertChangeAt(c, now, 10000);
}

static void test_rotation_skips_what_cannot_rotate() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("STARS,IMAGE,FIRE"), 10, false, idx("STARS"), now);
  assertChangeAt(c, now, 10000, bit("IMAGE"));
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());
}

static void test_overlay_stops_and_restarts_the_count() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), now);
  TEST_ASSERT_EQUAL(-1, ticksUntilChange(c, now, 4900));
  for (; now < 20000; now += 100) TEST_ASSERT_FALSE(c.tick(now, true, 0));
  TEST_ASSERT_EQUAL(idx("FIRE"), c.current());
  assertChangeAt(c, now, 10000);               // a full interval once it shows again
}

static void test_single_rotating_screen_is_not_restarted() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("STARS,CAT,-DATE"), 10, false, idx("STARS"), now);
  TEST_ASSERT_EQUAL(-1, ticksUntilChange(c, now, 600000UL));
  c.begin(only("STARS,IMAGE"), 10, false, idx("STARS"), now);
  TEST_ASSERT_EQUAL(-1, ticksUntilChange(c, now, 600000UL, bit("IMAGE")));
  TEST_ASSERT_EQUAL(idx("STARS"), c.current());
}

static void test_automatic_change_saves_nothing() {
  ScreenCursor c;
  uint32_t now = 0;
  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), now);
  for (int n = 0; n < 60; n++) {
    assertChangeAt(c, now, 10000);
    TEST_ASSERT_FALSE(c.takeSave(now));
  }
  TEST_ASSERT_FALSE(c.takeSave(now + 60000));
}

static void test_save_three_seconds_after_a_user_cause() {
  ScreenCursor c;
  c.begin(only("FIRE,STARS"), 0, false, idx("FIRE"), 0);
  TEST_ASSERT_FALSE(c.takeSave(10000));        // a start stores nothing

  c.turn(1, 1000);
  c.turn(1, 3000);                             // the last cause counts
  TEST_ASSERT_FALSE(c.takeSave(5999));
  TEST_ASSERT_TRUE(c.takeSave(6000));
  TEST_ASSERT_FALSE(c.takeSave(7000));

  c.press(10000);                              // a press alone (a session switch) stores nothing
  TEST_ASSERT_FALSE(c.takeSave(20000));
  c.press(20000);                              // a press that changed the brightness
  c.brightnessChanged(20000);
  TEST_ASSERT_FALSE(c.takeSave(22999));
  TEST_ASSERT_TRUE(c.takeSave(23000));
  TEST_ASSERT_FALSE(c.takeSave(24000));

  TEST_ASSERT_TRUE(c.request(idx("FIRE"), 30000));
  TEST_ASSERT_TRUE(c.takeSave(33000));
  TEST_ASSERT_FALSE(c.takeSave(34000));

  c.brightnessChanged(40000);                  // from the API or MQTT
  TEST_ASSERT_TRUE(c.takeSave(43000));
  TEST_ASSERT_FALSE(c.takeSave(44000));
}

// What the next turn either way brings: on, fitting, wrapping; -1 alone
static void test_neighbours_follow_the_knob() {
  ScreenCursor c;
  c.begin(only("FIRE,-DATE,CAT,STARS,CODE"), 0, false, idx("FIRE"), 0);
  TEST_ASSERT_EQUAL(idx("STARS"), c.neighbour(1));    // DATE off, CAT portrait only
  TEST_ASSERT_EQUAL(idx("CODE"), c.neighbour(-1));    // wraps
  ScreenCursor alone;
  alone.begin(only("FIRE"), 0, false, idx("FIRE"), 0);
  TEST_ASSERT_EQUAL(-1, alone.neighbour(1));
}

static void test_clock_wraps() {
  ScreenCursor c;
  uint32_t now = 0xFFFFFFFFu - 5000;
  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), now);
  assertChangeAt(c, now, 10000);               // across the wrap
  TEST_ASSERT_EQUAL(idx("STARS"), c.current());

  // A press, its hold over, then 49.7 days without a change: the difference
  // to the press comes round under 30 s again, and must not hold
  now = 1000;
  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), now);
  c.press(2000);
  TEST_ASSERT_TRUE(c.tick(32000, false, 0));   // hold over, interval long reached
  TEST_ASSERT_TRUE(c.tick(22000, false, 0));   // 2^32 - 10 000 ms later
}

static void test_next_in_seconds() {
  ScreenCursor c;
  c.begin(only("FIRE,STARS"), 0, false, idx("FIRE"), 0);
  TEST_ASSERT_EQUAL(-1, c.nextInSeconds(0, 0));          // rotation off

  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), 0);
  TEST_ASSERT_EQUAL(10, c.nextInSeconds(0, 0));
  TEST_ASSERT_EQUAL(8, c.nextInSeconds(2500, 0));        // rounded up
  c.turn(1, 3000);
  TEST_ASSERT_EQUAL(30, c.nextInSeconds(3000, 0));       // under a hold
  TEST_ASSERT_EQUAL(25, c.nextInSeconds(8000, 0));
  c.tick(9000, true, 0);
  TEST_ASSERT_EQUAL(10, c.nextInSeconds(9000, 0));       // covered: a full interval

  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), 0);
  TEST_ASSERT_EQUAL(-1, c.nextInSeconds(0, bit("STARS")));   // nothing else can rotate
  c.begin(only("FIRE,CAT"), 10, false, idx("FIRE"), 0);
  TEST_ASSERT_EQUAL(-1, c.nextInSeconds(0, 0));              // nothing else fits
}

// The status strip's line: how far into the interval the screen on show is;
// nothing (negative) when the rotation is off, held, covered or has nowhere
// to go
static void test_rotation_fill() {
  ScreenCursor c;
  c.begin(only("FIRE,STARS"), 0, false, idx("FIRE"), 0);
  TEST_ASSERT_TRUE(c.rotationFill(5000, 0) < 0);             // rotation off

  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), 0);
  TEST_ASSERT_EQUAL_FLOAT(0, c.rotationFill(0, 0));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, c.rotationFill(5000, 0));
  TEST_ASSERT_EQUAL_FLOAT(1, c.rotationFill(10000, 0));
  TEST_ASSERT_EQUAL_FLOAT(1, c.rotationFill(12000, 0));      // never past the end
  TEST_ASSERT_TRUE(c.tick(10000, false, 0));                 // the change starts it again
  TEST_ASSERT_EQUAL_FLOAT(0, c.rotationFill(10000, 0));

  c.turn(1, 11000);
  TEST_ASSERT_TRUE(c.rotationFill(11000, 0) < 0);            // held after a touch
  TEST_ASSERT_TRUE(c.rotationFill(11000 + HOLD_MS - 1, 0) < 0);
  TEST_ASSERT_TRUE(c.rotationFill(11000 + HOLD_MS, 0) >= 0); // the hold is over
  c.bringOn(idx("STARS"), 50000);
  TEST_ASSERT_TRUE(c.rotationFill(50000, 0) < 0);            // an agent's request holds too

  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), 0);
  c.tick(3000, true, 0);
  TEST_ASSERT_TRUE(c.rotationFill(3000, 0) < 0);             // covered by an overlay

  c.begin(only("FIRE,STARS"), 10, false, idx("FIRE"), 0);
  TEST_ASSERT_TRUE(c.rotationFill(0, bit("STARS")) < 0);     // nothing else can rotate
  c.begin(only("FIRE,CAT"), 10, false, idx("FIRE"), 0);
  TEST_ASSERT_TRUE(c.rotationFill(0, 0) < 0);                // nothing else fits
}

// --- Brightness ramp ------------------------------------------------------------

static void test_a_ramp_runs_from_the_current_value_to_the_target() {
  TEST_ASSERT_EQUAL_UINT8(100, rampValue(100, 10, 0, BRIGHTNESS_RAMP_MS));
  TEST_ASSERT_EQUAL_UINT8(55, rampValue(100, 10, BRIGHTNESS_RAMP_MS / 2, BRIGHTNESS_RAMP_MS));
  TEST_ASSERT_EQUAL_UINT8(10, rampValue(100, 10, BRIGHTNESS_RAMP_MS, BRIGHTNESS_RAMP_MS));
  TEST_ASSERT_EQUAL_UINT8(132, rampValue(10, 255, BRIGHTNESS_RAMP_MS / 2, BRIGHTNESS_RAMP_MS));
}

static void test_a_ramp_stays_on_the_target_once_done_or_for_no_time() {
  TEST_ASSERT_EQUAL_UINT8(10, rampValue(100, 10, BRIGHTNESS_RAMP_MS * 10, BRIGHTNESS_RAMP_MS));
  TEST_ASSERT_EQUAL_UINT8(10, rampValue(100, 10, 5, 0));
  TEST_ASSERT_EQUAL_UINT8(77, rampValue(77, 77, 100, BRIGHTNESS_RAMP_MS));
}

static void test_a_ramp_is_monotonic_and_stays_between_its_ends() {
  uint8_t last = 255;
  for (uint32_t t = 0; t <= BRIGHTNESS_RAMP_MS; t++) {
    const uint8_t v = rampValue(255, 10, t, BRIGHTNESS_RAMP_MS);
    TEST_ASSERT_TRUE(v <= last && v >= 10);
    last = v;
  }
  last = 0;
  for (uint32_t t = 0; t <= BRIGHTNESS_RAMP_MS; t++) {
    const uint8_t v = rampValue(0, 255, t, BRIGHTNESS_RAMP_MS);
    TEST_ASSERT_TRUE(v >= last);
    last = v;
  }
}

// --- Gradual night --------------------------------------------------------------

static const int H = 3600;

static void test_by_day_the_level_is_the_day_level() {
  TEST_ASSERT_EQUAL_UINT8(100, nightLevel(12 * H, 22, 7, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(100, nightLevel(7 * H, 22, 7, 100, 10));      // the end itself is day
  TEST_ASSERT_EQUAL_UINT8(100, nightLevel(22 * H - 1, 22, 7, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(100, nightLevel(3 * H, 5, 5, 100, 10));       // same hours: no night
}

static void test_the_night_comes_in_over_three_minutes() {
  TEST_ASSERT_EQUAL_UINT8(100, nightLevel(22 * H, 22, 7, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(55, nightLevel(22 * H + 90, 22, 7, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(10, nightLevel(22 * H + NIGHT_RAMP_SEC, 22, 7, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(10, nightLevel(2 * H, 22, 7, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(10, nightLevel(7 * H - NIGHT_RAMP_SEC - 1, 22, 7, 100, 10));
}

static void test_the_day_comes_back_over_the_last_three_minutes() {
  TEST_ASSERT_EQUAL_UINT8(10, nightLevel(7 * H - NIGHT_RAMP_SEC, 22, 7, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(55, nightLevel(7 * H - 90, 22, 7, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(99, nightLevel(7 * H - 2, 22, 7, 100, 10));
}

static void test_the_night_ramp_works_in_a_window_that_does_not_cross_midnight() {
  TEST_ASSERT_EQUAL_UINT8(100, nightLevel(1 * H, 1, 5, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(10, nightLevel(3 * H, 1, 5, 100, 10));
  TEST_ASSERT_EQUAL_UINT8(55, nightLevel(5 * H - 90, 1, 5, 100, 10));
}

static void test_a_night_that_is_brighter_ramps_up() {
  TEST_ASSERT_EQUAL_UINT8(55, nightLevel(22 * H + 90, 22, 7, 10, 100));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_start_screen);
  RUN_TEST(test_turn_follows_the_list_both_ways);
  RUN_TEST(test_turn_skips_off_and_other_orientation);
  RUN_TEST(test_turn_reaches_a_screen_that_cannot_rotate);
  RUN_TEST(test_tilt_keeps_or_takes_the_first);
  RUN_TEST(test_apply_moves_off_the_screen_turned_off);
  RUN_TEST(test_apply_all_at_once);
  RUN_TEST(test_apply_refuses_an_invalid_list);
  RUN_TEST(test_request_refuses_off_and_other_orientation);
  RUN_TEST(test_press_keeps_the_screen);
  RUN_TEST(test_knob_only_never_changes);
  RUN_TEST(test_rotation_follows_the_list);
  RUN_TEST(test_first_change_one_interval_after_start_and_save);
  RUN_TEST(test_hold_after_turn);
  RUN_TEST(test_hold_after_press);
  RUN_TEST(test_hold_after_request);
  RUN_TEST(test_an_agent_request_holds_but_saves_nothing);
  RUN_TEST(test_hold_with_a_long_interval);
  RUN_TEST(test_tilt_counts_again_without_hold);
  RUN_TEST(test_rotation_skips_what_cannot_rotate);
  RUN_TEST(test_overlay_stops_and_restarts_the_count);
  RUN_TEST(test_single_rotating_screen_is_not_restarted);
  RUN_TEST(test_automatic_change_saves_nothing);
  RUN_TEST(test_save_three_seconds_after_a_user_cause);
  RUN_TEST(test_neighbours_follow_the_knob);
  RUN_TEST(test_clock_wraps);
  RUN_TEST(test_next_in_seconds);
  RUN_TEST(test_rotation_fill);
  RUN_TEST(test_a_ramp_runs_from_the_current_value_to_the_target);
  RUN_TEST(test_a_ramp_stays_on_the_target_once_done_or_for_no_time);
  RUN_TEST(test_a_ramp_is_monotonic_and_stays_between_its_ends);
  RUN_TEST(test_by_day_the_level_is_the_day_level);
  RUN_TEST(test_the_night_comes_in_over_three_minutes);
  RUN_TEST(test_the_day_comes_back_over_the_last_three_minutes);
  RUN_TEST(test_the_night_ramp_works_in_a_window_that_does_not_cross_midnight);
  RUN_TEST(test_a_night_that_is_brighter_ramps_up);
  return UNITY_END();
}
