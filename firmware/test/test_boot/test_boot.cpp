// SPDX-License-Identifier: AGPL-3.0-only
// The boot sequence: which mode the setting means on a build, ready on the
// Wi-Fi alone, the beats' timing (the mark in, held and out, the black gap,
// the word in, the minimum, ready early, ready late), the frame's hand-over
// from the mark's task to loop(), the opening (its length, what it shows of the first
// screen and of the word beat as it runs, in landscape and portrait), the
// line's fill, Detailed's IP and the layout, and the text fitting.
//
//   pio test -e native -f test_boot
#include <unity.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "../../src/boot_logic.cpp"
#include "../../src/feedback_style.h"
#include "../../src/fonts_data.cpp"
#include "../../src/i18n.cpp"
#include "../../src/type.cpp"

void setUp() {}
void tearDown() {}

static const Strings &EN = STRINGS[LANG_EN];
static const Strings &FR = STRINGS[LANG_FR];

// --- Mode ---------------------------------------------------------------------

static void test_auto_is_simple_on_a_release_tag() {
  TEST_ASSERT_FALSE(bootDetailedFor(BOOT_AUTO, "v0.2.0", false));
  TEST_ASSERT_FALSE(bootDetailedFor(BOOT_AUTO, "v1.10.3-rc1", false));
}

static void test_auto_is_detailed_on_a_development_build() {
  TEST_ASSERT_TRUE(bootDetailedFor(BOOT_AUTO, "v0.1.0-23-gd8f4fee", false));
  TEST_ASSERT_TRUE(bootDetailedFor(BOOT_AUTO, "v0.1.0-dirty", false));
  TEST_ASSERT_TRUE(bootDetailedFor(BOOT_AUTO, "d8f4fee", false));       // no tag reachable
  TEST_ASSERT_TRUE(bootDetailedFor(BOOT_AUTO, "dev", false));           // git missing
  TEST_ASSERT_TRUE(bootDetailedFor(BOOT_AUTO, "v0.2.0", true));         // test console built in
}

static void test_simple_and_detailed_are_kept_whatever_the_build() {
  TEST_ASSERT_FALSE(bootDetailedFor(BOOT_SIMPLE, "v0.1.0-23-gd8f4fee", true));
  TEST_ASSERT_TRUE(bootDetailedFor(BOOT_DETAILED, "v0.2.0", false));
}

static void test_an_unknown_setting_reads_as_auto() {
  TEST_ASSERT_FALSE(bootDetailedFor(BOOT_DETAIL_COUNT, "v0.2.0", false));
  TEST_ASSERT_TRUE(bootDetailedFor(200, "v0.1.0-3-gabc1234", false));
}

static void test_the_ip_splits_after_its_second_dot() {
  char a[16], b[16];
  bootSplitIp("192.168.1.42", a, b, sizeof(a));
  TEST_ASSERT_EQUAL_STRING("192.168.", a);
  TEST_ASSERT_EQUAL_STRING("1.42", b);
  bootSplitIp("10.0", a, b, sizeof(a));
  TEST_ASSERT_EQUAL_STRING("10.0", a);
  TEST_ASSERT_EQUAL_STRING("", b);
}


// --- Ready --------------------------------------------------------------------

static BootInputs none() { return BootInputs{false, false, false}; }

// The Wi-Fi joined is enough: the time is no longer waited for (the screens
// that need it show their own waiting state)
static void test_ready_on_the_wifi_without_the_time() {
  BootProgress b;
  BootInputs in = none();
  b.update(in, 900);
  b.update(in, 5900);
  TEST_ASSERT_EQUAL_UINT8(BEAT_WORD, b.beat(5900));
  in.wifiJoined = true;
  b.update(in, 6000);
  TEST_ASSERT_EQUAL_UINT8(BEAT_OPEN, b.beat(6000));
}

// --- Sequence -----------------------------------------------------------------
// The clock starts at the first frame: millis() is well past 0 by then

static const uint32_t T0 = 900;
static BootInputs ready() { return BootInputs{true, false, false}; }

// The owner's timings (2026-10-23): the mark in over 600 ms, held until
// 1.5 s, out over 600 ms; 300 ms of black; the word in over 600 ms
static void test_the_beats_take_the_owners_timings() {
  TEST_ASSERT_EQUAL_UINT32(600, BOOT_MARK_IN_MS);
  TEST_ASSERT_EQUAL_UINT32(1500, BOOT_MARK_HOLD_MS);
  TEST_ASSERT_EQUAL_UINT32(600, BOOT_MARK_OUT_MS);
  TEST_ASSERT_EQUAL_UINT32(300, BOOT_BLACK_MS);
  TEST_ASSERT_EQUAL_UINT32(2400, BOOT_WORD_AT_MS);
  TEST_ASSERT_EQUAL_UINT32(600, BOOT_WORD_FADE_MS);
  TEST_ASSERT_EQUAL_UINT32(4000, BOOT_MIN_MS);
  TEST_ASSERT_EQUAL_UINT32(15000, BOOT_MAX_MS);
}

static void test_the_mark_fades_in_holds_and_fades_out_to_black() {
  BootProgress b;
  b.update(none(), T0);
  TEST_ASSERT_EQUAL_UINT8(BEAT_MARK, b.beat(T0));
  TEST_ASSERT_EQUAL_UINT8(0, b.markLevel(T0));
  const uint8_t in = b.markLevel(T0 + BOOT_MARK_IN_MS / 2);
  TEST_ASSERT_TRUE(in > 100 && in < 155);
  TEST_ASSERT_EQUAL_UINT8(255, b.markLevel(T0 + BOOT_MARK_IN_MS));
  TEST_ASSERT_EQUAL_UINT8(255, b.markLevel(T0 + BOOT_MARK_HOLD_MS - 1));
  TEST_ASSERT_EQUAL_UINT8(255, b.markLevel(T0 + BOOT_MARK_HOLD_MS));
  const uint8_t out = b.markLevel(T0 + BOOT_MARK_HOLD_MS + BOOT_MARK_OUT_MS / 2);
  TEST_ASSERT_TRUE(out > 100 && out < 155);
  TEST_ASSERT_EQUAL_UINT8(0, b.markLevel(T0 + BOOT_MARK_HOLD_MS + BOOT_MARK_OUT_MS));
  TEST_ASSERT_EQUAL_UINT8(0, b.markLevel(T0 + 60000));
  // never brighter on the way out, never darker on the way in (60 fps)
  uint8_t last = 0;
  for (uint32_t t = T0; t <= T0 + BOOT_MARK_HOLD_MS; t += 16) {
    TEST_ASSERT_TRUE(b.markLevel(t) >= last);
    last = b.markLevel(t);
  }
  for (uint32_t t = T0 + BOOT_MARK_HOLD_MS; t <= T0 + BOOT_WORD_AT_MS; t += 16) {
    TEST_ASSERT_TRUE(b.markLevel(t) <= last);
    last = b.markLevel(t);
  }
}

// Between the mark and the word, the panel is black: no crossfade any more
static void test_a_black_gap_parts_the_mark_and_the_word() {
  BootProgress b;
  b.update(none(), T0);
  const uint32_t dark = T0 + BOOT_MARK_HOLD_MS + BOOT_MARK_OUT_MS;
  TEST_ASSERT_EQUAL_UINT32(dark + BOOT_BLACK_MS, T0 + BOOT_WORD_AT_MS);
  for (uint32_t t = T0; t < T0 + BOOT_WORD_AT_MS + BOOT_WORD_FADE_MS; t++)
    TEST_ASSERT_FALSE(b.markLevel(t) && b.wordLevel(t));   // never both lit
  for (uint32_t t = dark; t < T0 + BOOT_WORD_AT_MS; t++) {
    TEST_ASSERT_EQUAL_UINT8(0, b.markLevel(t));
    TEST_ASSERT_EQUAL_UINT8(0, b.wordLevel(t));
    TEST_ASSERT_EQUAL_UINT8(BEAT_MARK, b.beat(t));
  }
  TEST_ASSERT_EQUAL_UINT8(BEAT_WORD, b.beat(T0 + BOOT_WORD_AT_MS));
}

// After a software restart (update, the page's restart): no mark, no black
// gap, the word at once; a power-on keeps the full sequence (the default)
static void test_a_short_boot_starts_at_the_word() {
  BootProgress b;
  b.shorten();
  b.update(none(), T0);
  TEST_ASSERT_EQUAL_UINT8(BEAT_WORD, b.beat(T0));
  TEST_ASSERT_EQUAL_UINT8(0, b.markLevel(T0));
  TEST_ASSERT_EQUAL_UINT8(0, b.markLevel(T0 + 300));
  TEST_ASSERT_EQUAL_UINT8(0, b.wordLevel(T0));
  const uint8_t mid = b.wordLevel(T0 + BOOT_WORD_FADE_MS / 2);
  TEST_ASSERT_TRUE(mid > 100 && mid < 155);
  TEST_ASSERT_EQUAL_UINT8(255, b.wordLevel(T0 + BOOT_WORD_FADE_MS));
  TEST_ASSERT_EQUAL_UINT8(0, b.fill(30, T0));   // the line starts empty with the word
  TEST_ASSERT_TRUE(b.fill(30, T0 + 1000) > 0);
}

static void test_a_short_boot_keeps_the_words_beat_and_the_opening() {
  BootProgress b;
  b.shorten();
  b.update(ready(), T0);
  const uint32_t beat = BOOT_MIN_MS - BOOT_WORD_AT_MS;   // the word's beat, as in the full sequence
  TEST_ASSERT_EQUAL_UINT8(BEAT_WORD, b.beat(T0 + beat - 1));
  TEST_ASSERT_EQUAL_UINT8(BEAT_OPEN, b.beat(T0 + beat));
  TEST_ASSERT_EQUAL_UINT8(BEAT_OPEN, b.beat(T0 + beat + BOOT_OPEN_MS - 1));
  TEST_ASSERT_EQUAL_UINT8(BEAT_OVER, b.beat(T0 + beat + BOOT_OPEN_MS));
}

static void test_a_power_on_boot_keeps_the_full_sequence() {
  BootProgress b;
  b.update(none(), T0);
  TEST_ASSERT_EQUAL_UINT8(BEAT_MARK, b.beat(T0));
  TEST_ASSERT_EQUAL_UINT8(0, b.wordLevel(T0 + BOOT_WORD_AT_MS - 1));
  TEST_ASSERT_TRUE(b.markLevel(T0 + BOOT_MARK_IN_MS) > 0);
}

static void test_the_word_fades_in_after_the_black() {
  BootProgress b;
  b.update(none(), T0);
  const uint32_t w = T0 + BOOT_WORD_AT_MS;
  TEST_ASSERT_EQUAL_UINT8(0, b.wordLevel(w - 1));
  TEST_ASSERT_EQUAL_UINT8(0, b.wordLevel(w));
  const uint8_t mid = b.wordLevel(w + BOOT_WORD_FADE_MS / 2);
  TEST_ASSERT_TRUE(mid > 100 && mid < 155);
  TEST_ASSERT_EQUAL_UINT8(255, b.wordLevel(w + BOOT_WORD_FADE_MS));
  TEST_ASSERT_EQUAL_UINT8(255, b.wordLevel(w + 60000));
  // its line starts empty with it
  TEST_ASSERT_EQUAL_UINT8(0, b.fill(30, w));
}

// --- Hand-over ----------------------------------------------------------------
// The mark's task draws the first frames from setup(), loop() the rest

// The clock is the first frame's, whoever draws it: loop()'s first frame
// continues the sequence instead of starting it again (a mark back at 0, a flash)
static void test_loops_first_frame_continues_the_tasks_sequence() {
  BootProgress b;
  b.update(none(), T0);                       // the task's first frame
  b.update(none(), T0 + 16);
  const uint32_t loopFirst = T0 + 1200;       // setup() done, mid-hold
  b.update(none(), loopFirst);
  TEST_ASSERT_EQUAL_UINT8(255, b.markLevel(loopFirst));
  TEST_ASSERT_EQUAL_UINT8(BEAT_WORD, b.beat(T0 + BOOT_WORD_AT_MS));
  TEST_ASSERT_EQUAL_UINT8(255, b.wordLevel(T0 + BOOT_WORD_AT_MS + BOOT_WORD_FADE_MS));
}

static void test_without_a_task_loop_owns_the_frame() {
  BootBaton f;
  TEST_ASSERT_FALSE(f.taskMayDraw());
  TEST_ASSERT_TRUE(f.loopMayDraw());
  f.loopAsks();
  TEST_ASSERT_TRUE(f.loopMayDraw());
}

// One owner at a time, at every step of the hand-over
static void test_the_frame_has_one_owner_at_a_time() {
  BootBaton f;
  f.taskTakes();
  TEST_ASSERT_TRUE(f.taskMayDraw());
  TEST_ASSERT_FALSE(f.loopMayDraw());
  f.loopAsks();   // the task's frame under way is its last
  TEST_ASSERT_FALSE(f.taskMayDraw());
  TEST_ASSERT_FALSE(f.loopMayDraw());
  f.taskLeft();
  TEST_ASSERT_FALSE(f.taskMayDraw());
  TEST_ASSERT_TRUE(f.loopMayDraw());
  f.loopAsks();   // asked again: still loop()'s
  TEST_ASSERT_FALSE(f.taskMayDraw());
  TEST_ASSERT_TRUE(f.loopMayDraw());
}

// A task that stops on its own (the sequence handed to the opening) leaves
// the frame to loop() before it asks
static void test_a_task_that_leaves_first_hands_the_frame_over() {
  BootBaton f;
  f.taskTakes();
  f.taskLeft();
  TEST_ASSERT_FALSE(f.taskMayDraw());
  TEST_ASSERT_TRUE(f.loopMayDraw());
}

// Two threads, as on the board: the task draws frames until asked, loop()
// asks, waits for the frame and draws. Every frame is logged with its
// drawer: none of the task's after loop()'s first, never both at once.
static void test_no_frame_is_drawn_by_both_owners() {
  for (int round = 0; round < 50; round++) {
    BootBaton f;
    std::atomic<int> drawing{0};
    std::atomic<bool> overlap{false};
    std::vector<char> log;
    std::mutex logLock;
    auto draw = [&](char who) {
      if (drawing.fetch_add(1) != 0) overlap = true;
      {
        std::lock_guard<std::mutex> l(logLock);
        log.push_back(who);
      }
      std::this_thread::yield();
      drawing.fetch_sub(1);
    };
    f.taskTakes();
    std::thread task([&] {
      while (f.taskMayDraw()) draw('t');
      f.taskLeft();
    });
    std::this_thread::sleep_for(std::chrono::microseconds(round * 20));
    f.loopAsks();
    while (!f.loopMayDraw()) std::this_thread::yield();
    for (int i = 0; i < 5; i++) draw('l');
    task.join();
    TEST_ASSERT_FALSE(overlap);
    bool loopDrew = false;
    for (char c : log) {
      if (c == 'l') loopDrew = true;
      else TEST_ASSERT_FALSE(loopDrew);
    }
    TEST_ASSERT_TRUE(loopDrew);
  }
}

// Ready before the two beats end: they still play whole, then the opening
static void test_ready_early_still_plays_the_minimum() {
  BootProgress b;
  b.update(ready(), T0);
  TEST_ASSERT_EQUAL_UINT8(BEAT_MARK, b.beat(T0));
  TEST_ASSERT_EQUAL_UINT8(BEAT_WORD, b.beat(T0 + BOOT_MIN_MS - 1));
  TEST_ASSERT_EQUAL_UINT8(BEAT_OPEN, b.beat(T0 + BOOT_MIN_MS));
  TEST_ASSERT_EQUAL_UINT8(BEAT_OPEN, b.beat(T0 + BOOT_MIN_MS + BOOT_OPEN_MS - 1));
  TEST_ASSERT_EQUAL_UINT8(BEAT_OVER, b.beat(T0 + BOOT_MIN_MS + BOOT_OPEN_MS));
}

// Ready late: the opening starts at once, no hold after the line fills
static void test_ready_late_opens_right_away() {
  BootProgress b;
  BootInputs in = none();
  b.update(in, T0);
  b.update(in, T0 + 5500);
  TEST_ASSERT_EQUAL_UINT8(BEAT_WORD, b.beat(T0 + 5500));
  b.update(ready(), T0 + 6000);
  TEST_ASSERT_EQUAL_UINT8(BEAT_OPEN, b.beat(T0 + 6000));
  TEST_ASSERT_EQUAL_UINT8(BEAT_OVER, b.beat(T0 + 6000 + BOOT_OPEN_MS));
}

static void test_the_opening_starts_after_15_seconds_whatever_is_pending() {
  BootProgress b;
  BootInputs in = none();
  b.update(in, T0);
  b.update(in, BOOT_MAX_MS - 1);
  TEST_ASSERT_EQUAL_UINT8(BEAT_WORD, b.beat(BOOT_MAX_MS - 1));
  b.update(in, BOOT_MAX_MS);
  TEST_ASSERT_EQUAL_UINT8(BEAT_OPEN, b.beat(BOOT_MAX_MS));
}

// The setup screen is the boot's way out: no minimum, no opening
static void test_over_when_the_access_point_opens() {
  BootProgress b;
  BootInputs in = none();
  b.update(in, T0);
  TEST_ASSERT_EQUAL_UINT8(BEAT_MARK, b.beat(T0));
  in.setupOpen = true;
  b.update(in, T0 + 100);
  TEST_ASSERT_EQUAL_UINT8(BEAT_OVER, b.beat(T0 + 100));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, b.opened(T0 + 100));
}

// The opening's progress runs 0 to 1, linear in time, over BOOT_OPEN_MS from
// the moment it starts; nothing before or after
static void test_the_opening_progress_runs_0_to_1() {
  BootProgress b;
  b.update(ready(), T0);
  const uint32_t open = T0 + BOOT_MIN_MS;
  TEST_ASSERT_EQUAL_UINT32(1400, BOOT_OPEN_MS);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, b.opened(open - 1));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, b.opened(open));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, b.opened(open + BOOT_OPEN_MS / 2));
  TEST_ASSERT_EQUAL_UINT8(BEAT_OPEN, b.beat(open + BOOT_OPEN_MS - 1));
  TEST_ASSERT_EQUAL_UINT8(BEAT_OVER, b.beat(open + BOOT_OPEN_MS));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, b.opened(open + BOOT_OPEN_MS));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, b.opened(open + 60000));
}

// --- Text ---------------------------------------------------------------------

static void assertFits(const char *s, uint16_t width, Face f) {
  char msg[96];
  snprintf(msg, sizeof(msg), "\"%s\" is %u px", s, textWidth(s, f));
  TEST_ASSERT_TRUE_MESSAGE(textWidth(s, f) <= width, msg);
}


// --- Line ---------------------------------------------------------------------

// Dark before the word, empty when the word comes in
static void test_the_line_starts_empty() {
  BootProgress b;
  b.update(none(), 1000);
  TEST_ASSERT_EQUAL_UINT8(0, b.fill(30, 1000));
  b.update(none(), 1000 + BOOT_WORD_AT_MS);
  TEST_ASSERT_EQUAL_UINT8(0, b.fill(30, 1000 + BOOT_WORD_AT_MS));
}

// Over the Wi-Fi wait the line creeps towards three quarters, then waits:
// never full before the Wi-Fi is there
static void test_the_line_creeps_over_the_wifi_wait() {
  BootProgress b;
  const uint32_t w = 1000 + BOOT_WORD_AT_MS;
  uint8_t last = 0;
  for (uint32_t t = 1000; t < BOOT_MAX_MS; t += 250) {
    b.update(none(), t);
    const uint8_t f = b.fill(40, t);
    TEST_ASSERT_TRUE(f >= last);
    TEST_ASSERT_TRUE(f <= 30);
    last = f;
  }
  TEST_ASSERT_EQUAL_UINT8(30, last);
  TEST_ASSERT_TRUE(b.fill(40, w + BOOT_CREEP_MS / 2) > 10);
}

// The Wi-Fi early: the line still fills before the eyes, ending full as the
// opening starts
static void test_the_line_fills_by_the_opening_when_the_wifi_is_early() {
  BootProgress b;
  b.update(ready(), 1000);
  const uint32_t w = 1000 + BOOT_WORD_AT_MS, open = 1000 + BOOT_MIN_MS;
  TEST_ASSERT_EQUAL_UINT8(0, b.fill(40, w));
  const uint8_t mid = b.fill(40, (w + open) / 2);
  TEST_ASSERT_TRUE(mid >= 18 && mid <= 22);
  TEST_ASSERT_TRUE(b.fill(40, open - 1) < 40);
  TEST_ASSERT_EQUAL_UINT8(40, b.fill(40, open));
}

// The Wi-Fi late: the line is full at once and the opening starts
static void test_the_line_is_full_at_once_when_the_wifi_is_late() {
  BootProgress b;
  b.update(none(), 1000);
  b.update(none(), 7000);
  TEST_ASSERT_TRUE(b.fill(40, 7000) < 40);
  b.update(ready(), 7100);
  TEST_ASSERT_EQUAL_UINT8(40, b.fill(40, 7100));
}

static void test_a_failed_wifi_stops_the_line() {
  BootProgress b;
  BootInputs in = none();
  b.update(in, 1000);
  b.update(in, 5000);
  in.wifiFailed = true;
  b.update(in, 5000);
  TEST_ASSERT_TRUE(b.stopped());
  const uint8_t held = b.fill(40, 5000);
  b.update(in, 6500);
  TEST_ASSERT_EQUAL_UINT8(held, b.fill(40, 6500));
  TEST_ASSERT_EQUAL_UINT8(BEAT_WORD, b.beat(6500));   // until the setup screen opens
  in.wifiFailed = false;                     // a retry got through
  in.wifiJoined = true;
  b.update(in, 7000);
  TEST_ASSERT_FALSE(b.stopped());
  TEST_ASSERT_EQUAL_UINT8(40, b.fill(40, 7000));
}

// The word's inked box, whatever face draws it
static void test_the_ink_box_bounds_what_was_drawn() {
  BootInk ink(64, 32);
  TEST_ASSERT_FALSE(ink.any());
  ink.drawPixel(12, 4, 1);
  ink.drawPixel(50, 10, 1);
  ink.drawPixel(30, 7, 1);
  TEST_ASSERT_TRUE(ink.any());
  TEST_ASSERT_EQUAL_INT16(12, ink.left);
  TEST_ASSERT_EQUAL_INT16(50, ink.right);
  TEST_ASSERT_EQUAL_INT16(4, ink.top);
  TEST_ASSERT_EQUAL_INT16(10, ink.bottom);
}

// --- Detailed and the layout -----------------------------------------------------

// Detailed's one line is the IP: dark until the Wi-Fi is joined, then fading in
static void test_the_ip_shows_once_the_wifi_is_joined() {
  BootProgress b;
  b.update(none(), 1000);
  b.update(none(), 5000);
  TEST_ASSERT_EQUAL_UINT8(0, b.ipLevel(5000));
  b.update(ready(), 5000);
  TEST_ASSERT_EQUAL_UINT8(0, b.ipLevel(5000));
  const uint8_t mid = b.ipLevel(5000 + BOOT_IP_FADE_MS / 2);
  TEST_ASSERT_TRUE(mid > 100 && mid < 155);
  TEST_ASSERT_EQUAL_UINT8(255, b.ipLevel(5000 + BOOT_IP_FADE_MS));
}

// Simple: the word and its line, centred, no other row. Detailed: the same
// word and line, the IP under the line and nothing else
static void test_simple_has_no_text_row_and_detailed_only_the_ip() {
  // the body face's word in landscape: ink rows 0-6
  const BootLayout s = bootLayout(32, 7, false);
  TEST_ASSERT_EQUAL_INT16(11, s.wordY);
  TEST_ASSERT_EQUAL_INT16(21, s.lineY);
  TEST_ASSERT_EQUAL_INT16(-1, s.ipY);
  const BootLayout d = bootLayout(32, 7, true);
  TEST_ASSERT_EQUAL_INT16(s.wordY, d.wordY);
  TEST_ASSERT_EQUAL_INT16(s.lineY, d.lineY);
  TEST_ASSERT_EQUAL_INT16(25, d.ipY);   // the foot row
  // portrait: two body lines, 17 ink rows; the IP's two lines still fit
  const BootLayout p = bootLayout(64, 17, true);
  TEST_ASSERT_EQUAL_INT16(22, p.wordY);
  TEST_ASSERT_EQUAL_INT16(42, p.lineY);
  TEST_ASSERT_EQUAL_INT16(46, p.ipY);
  TEST_ASSERT_TRUE(p.ipY + 7 + 5 <= 64);
}

// No step names any more: the label face's strings left are the IP's
static void test_the_widest_address_fits() {
  assertFits("255.255.255.255", 60, Face::LABEL);
  char a[16], b[16];
  bootSplitIp("255.255.255.255", a, b, sizeof(a));
  assertFits(a, 28, Face::LABEL);
  assertFits(b, 28, Face::LABEL);
}

static void test_a_faded_colour_scales_each_channel() {
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, bootFaded(0xFFFF, 255));
  TEST_ASSERT_EQUAL_HEX16(0x0000, bootFaded(0xFFFF, 0));
  TEST_ASSERT_EQUAL_HEX16(0x7BEF, bootFaded(0xFFFF, 128));   // 15/31, 31/63, 15/31
  TEST_ASSERT_EQUAL_HEX16(0xF800, bootFaded(0xF800, 255));
  TEST_ASSERT_EQUAL_HEX16(0x0000, bootFaded(0x0000, 200));
}

// The word in the body face: one line in landscape, two in portrait
static void test_the_word_fits() {
  for (const Strings *t : {&EN, &FR}) {
    assertFits(t->bootWord, 60, Face::BODY);
    for (const char *s : t->bootWordTall) assertFits(s, 28, Face::BODY);
  }
}

// --- The opening --------------------------------------------------------------

// The line: it widens from the word's ink to the full width as the word
// fades, then rises to the top with the screen hanging under it
static void test_the_line_widens_then_the_screen_rises_from_it() {
  const float widen = (float)BOOT_LINE_WIDEN_MS / BOOT_OPEN_MS;
  const BootOpeningFrame a = bootOpeningAt(0.0f, 64, 32, 24, 2, 61);
  TEST_ASSERT_EQUAL_INT16(24, a.lineRow);
  TEST_ASSERT_EQUAL_INT16(2, a.lineLeft);
  TEST_ASSERT_EQUAL_INT16(61, a.lineRight);
  TEST_ASSERT_EQUAL_UINT8(255, a.wordLight);
  TEST_ASSERT_EQUAL_INT16(32, a.stageTop);   // none of the screen yet
  const BootOpeningFrame b = bootOpeningAt(widen, 64, 32, 24, 2, 61);
  TEST_ASSERT_EQUAL_INT16(0, b.lineLeft);
  TEST_ASSERT_EQUAL_INT16(63, b.lineRight);
  TEST_ASSERT_EQUAL_UINT8(0, b.wordLight);
  TEST_ASSERT_EQUAL_INT16(24, b.lineRow);
  TEST_ASSERT_EQUAL_INT16(25, b.stageTop);   // the screen starts right under the line
  int16_t last = b.lineRow;
  for (float p = widen; p <= 1.0f; p += 0.02f) {
    const BootOpeningFrame f = bootOpeningAt(p, 64, 32, 24, 2, 61);
    TEST_ASSERT_TRUE(f.lineRow <= last);
    TEST_ASSERT_EQUAL_INT16(f.lineRow + 1, f.stageTop);
    TEST_ASSERT_EQUAL_INT16(0, f.lineLeft);
    TEST_ASSERT_EQUAL_INT16(63, f.lineRight);
    last = f.lineRow;
  }
  const BootOpeningFrame end = bootOpeningAt(1.0f, 64, 32, 24, 2, 61);
  TEST_ASSERT_EQUAL_INT16(0, end.stageTop);
  TEST_ASSERT_EQUAL_INT16(-1, end.lineRow);   // gone over the top
}

// The word fades out while the line widens, on the symmetric ease: it starts
// and lands softly, half lit at half
static void test_the_word_fades_as_the_line_widens() {
  const float widen = (float)BOOT_LINE_WIDEN_MS / BOOT_OPEN_MS;
  TEST_ASSERT_TRUE(bootOpeningAt(widen / 10, 64, 32, 24, 2, 61).wordLight >= 245);
  const uint8_t half = bootOpeningAt(widen / 2, 64, 32, 24, 2, 61).wordLight;
  TEST_ASSERT_TRUE(half >= 120 && half <= 135);
  TEST_ASSERT_TRUE(bootOpeningAt(widen * 9 / 10, 64, 32, 24, 2, 61).wordLight <= 10);
  // the rise: half way at half, the line at the middle of its way
  const int16_t row = bootOpeningAt(widen + (1.0f - widen) / 2, 64, 32, 24, 2, 61).lineRow;
  TEST_ASSERT_INT16_WITHIN(1, 12, row);
}

// Portrait: the same along its 64 rows
static void test_portrait_rises_along_its_long_side() {
  const BootOpeningFrame a = bootOpeningAt(0.0f, 32, 64, 40, 2, 29);
  TEST_ASSERT_EQUAL_INT16(64, a.stageTop);
  TEST_ASSERT_EQUAL_INT16(40, a.lineRow);
  const BootOpeningFrame b = bootOpeningAt((float)BOOT_LINE_WIDEN_MS / BOOT_OPEN_MS, 32, 64, 40, 2, 29);
  TEST_ASSERT_EQUAL_INT16(31, b.lineRight);
  TEST_ASSERT_EQUAL_INT16(41, b.stageTop);
  TEST_ASSERT_EQUAL_INT16(0, bootOpeningAt(1.0f, 32, 64, 40, 2, 29).stageTop);
}

// The screen as composed: its own pixels where it shows, moved down by the
// rows it has still to rise; nothing written elsewhere, so the word beat
// drawn first stays
static const uint16_t MARK = 0xBEEF;
static uint16_t stage[64 * 32], out[64 * 32];

static void fillStage() {
  for (int i = 0; i < 64 * 32; i++) {
    stage[i] = (uint16_t)(i + 1);
    out[i]   = MARK;
  }
}

static void test_the_screen_shows_from_the_line_down() {
  fillStage();
  BootOpeningFrame f = {16, 0, 15, 0, 63};   // the line on row 15, the screen under it
  bootOpeningStage(f, stage, out, 64, 32, 0);
  TEST_ASSERT_EQUAL_HEX16(MARK, out[15 * 64 + 5]);
  TEST_ASSERT_EQUAL_HEX16(stage[0 * 64 + 5], out[16 * 64 + 5]);
  TEST_ASSERT_EQUAL_HEX16(stage[15 * 64 + 63], out[31 * 64 + 63]);

  fillStage();   // the word fading out over black: nothing of the screen written yet
  f = bootOpeningAt(0.1f, 64, 32, 24, 2, 61);
  bootOpeningStage(f, stage, out, 64, 32, 0);
  for (int i = 0; i < 64 * 32; i++) TEST_ASSERT_EQUAL_HEX16(MARK, out[i]);

  fillStage();   // the end: the screen whole
  f = bootOpeningAt(1.0f, 64, 32, 24, 2, 61);
  bootOpeningStage(f, stage, out, 64, 32, 0);
  for (int i = 0; i < 64 * 32; i++) TEST_ASSERT_EQUAL_HEX16(stage[i], out[i]);
}

// Portrait: the frame's rotation turns the panel, the screen still rises
// along the rows the viewer sees (rotation 1: on-screen (x, y) is buffer
// column 63 - y, row x)
static void test_a_portrait_screen_rises_along_its_own_rows() {
  fillStage();
  const BootOpeningFrame f = {32, 0, 31, 0, 31};   // the screen from row 32
  bootOpeningStage(f, stage, out, 64, 32, 1);
  const auto at = [](const uint16_t *b, int x, int y) { return b[(63 - y) + x * 64]; };
  TEST_ASSERT_EQUAL_HEX16(MARK, at(out, 4, 31));
  TEST_ASSERT_EQUAL_HEX16(at(stage, 4, 0), at(out, 4, 32));
  TEST_ASSERT_EQUAL_HEX16(at(stage, 4, 31), at(out, 4, 63));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_auto_is_simple_on_a_release_tag);
  RUN_TEST(test_auto_is_detailed_on_a_development_build);
  RUN_TEST(test_simple_and_detailed_are_kept_whatever_the_build);
  RUN_TEST(test_an_unknown_setting_reads_as_auto);
  RUN_TEST(test_the_ip_splits_after_its_second_dot);
  RUN_TEST(test_ready_on_the_wifi_without_the_time);
  RUN_TEST(test_the_beats_take_the_owners_timings);
  RUN_TEST(test_the_mark_fades_in_holds_and_fades_out_to_black);
  RUN_TEST(test_a_black_gap_parts_the_mark_and_the_word);
  RUN_TEST(test_the_word_fades_in_after_the_black);
  RUN_TEST(test_a_short_boot_starts_at_the_word);
  RUN_TEST(test_a_short_boot_keeps_the_words_beat_and_the_opening);
  RUN_TEST(test_a_power_on_boot_keeps_the_full_sequence);
  RUN_TEST(test_loops_first_frame_continues_the_tasks_sequence);
  RUN_TEST(test_without_a_task_loop_owns_the_frame);
  RUN_TEST(test_the_frame_has_one_owner_at_a_time);
  RUN_TEST(test_a_task_that_leaves_first_hands_the_frame_over);
  RUN_TEST(test_no_frame_is_drawn_by_both_owners);
  RUN_TEST(test_ready_early_still_plays_the_minimum);
  RUN_TEST(test_ready_late_opens_right_away);
  RUN_TEST(test_the_opening_starts_after_15_seconds_whatever_is_pending);
  RUN_TEST(test_over_when_the_access_point_opens);
  RUN_TEST(test_the_opening_progress_runs_0_to_1);
  RUN_TEST(test_the_line_starts_empty);
  RUN_TEST(test_the_line_creeps_over_the_wifi_wait);
  RUN_TEST(test_the_line_fills_by_the_opening_when_the_wifi_is_early);
  RUN_TEST(test_the_line_is_full_at_once_when_the_wifi_is_late);
  RUN_TEST(test_a_failed_wifi_stops_the_line);
  RUN_TEST(test_the_ink_box_bounds_what_was_drawn);
  RUN_TEST(test_the_ip_shows_once_the_wifi_is_joined);
  RUN_TEST(test_simple_has_no_text_row_and_detailed_only_the_ip);
  RUN_TEST(test_the_widest_address_fits);
  RUN_TEST(test_a_faded_colour_scales_each_channel);
  RUN_TEST(test_the_word_fits);
  RUN_TEST(test_the_line_widens_then_the_screen_rises_from_it);
  RUN_TEST(test_the_word_fades_as_the_line_widens);
  RUN_TEST(test_portrait_rises_along_its_long_side);
  RUN_TEST(test_the_screen_shows_from_the_line_down);
  RUN_TEST(test_a_portrait_screen_rises_along_its_own_rows);
  return UNITY_END();
}
