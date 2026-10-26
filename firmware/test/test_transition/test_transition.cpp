// SPDX-License-Identifier: AGPL-3.0-only
// The passage between two screens without the panel: each kind combined at
// its start, middle and end on two small frames, the push's direction (also
// on a rotated frame), and the fallback to a cut when the outgoing frame
// cannot be kept.
//
//   pio test -e native -f test_transition
#include <unity.h>

#include <cstring>

#include "../../src/transition.cpp"

static const int16_t FW = 8, FH = 4;   // a small unrotated frame
static const uint16_t A = 0xAAAA, B = 0xBBBB, LINE = 0x1111;

static uint16_t from[FW * FH], to[FW * FH], out[FW * FH];

// Each pixel of from and to tells its own column, so a shift is visible
static uint16_t fromAt(int x) { return (uint16_t)(0xA000 + x); }
static uint16_t toAt(int x)   { return (uint16_t)(0xB000 + x); }

void setUp() {
  for (int y = 0; y < FH; y++)
    for (int x = 0; x < FW; x++) {
      from[y * FW + x] = fromAt(x);
      to[y * FW + x]   = toAt(x);
    }
  memset(out, 0, sizeof(out));
}
void tearDown() {}

static void combine(uint8_t kind, float p, int8_t dir = 1, uint8_t rotation = 0) {
  transitionCombine(kind, from, to, out, FW, FH, rotation, p, dir, LINE);
}

static void assertAll(const uint16_t *expected) {
  TEST_ASSERT_EQUAL_HEX16_ARRAY(expected, out, FW * FH);
}

// --- durations and the curve --------------------------------------------------

static void test_durations_follow_the_design() {
  TEST_ASSERT_EQUAL_UINT16(240, transitionMs(TRANSITION_PUSH));
  TEST_ASSERT_EQUAL_UINT16(400, transitionMs(TRANSITION_DISSOLVE));
  TEST_ASSERT_EQUAL_UINT16(240, transitionMs(TRANSITION_WIPE));
  TEST_ASSERT_EQUAL_UINT16(0, transitionMs(TRANSITION_CUT));
  TEST_ASSERT_EQUAL_UINT16(0, transitionMs(TRANSITION_COUNT));   // unknown: a cut
}

// ease.h's curve, the one the overlays use too
static void test_ease_starts_fast_and_lands_on_one() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, easeOut(-0.5f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, easeOut(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.96875f, easeOut(0.5f));   // 1 - 2^-5
  TEST_ASSERT_EQUAL_FLOAT(1.0f, easeOut(1.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, easeOut(2.0f));
}

// The symmetric curve (smoothstep) for motions that must start and land softly
static void test_ease_in_out_is_symmetric_and_soft_at_both_ends() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, easeInOut(-0.5f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, easeInOut(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.028f, easeInOut(0.1f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, easeInOut(0.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.972f, easeInOut(0.9f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, easeInOut(1.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, easeInOut(2.0f));
}

// --- each kind at t = 0, middle, end ---------------------------------------------

static void test_push_starts_on_the_outgoing_frame() {
  combine(TRANSITION_PUSH, 0.0f);
  assertAll(from);
}

static void test_push_ends_on_the_incoming_frame() {
  combine(TRANSITION_PUSH, 1.0f);
  assertAll(to);
}

static void test_push_next_moves_everything_left() {
  // p = 0.1: ease 0.5, the frames have moved 4 of 8 columns
  combine(TRANSITION_PUSH, 0.1f, 1);
  for (int y = 0; y < FH; y++)
    for (int x = 0; x < FW; x++)
      TEST_ASSERT_EQUAL_HEX16(x < 4 ? fromAt(x + 4) : toAt(x - 4), out[y * FW + x]);
}

static void test_push_previous_moves_everything_right() {
  combine(TRANSITION_PUSH, 0.1f, -1);
  for (int y = 0; y < FH; y++)
    for (int x = 0; x < FW; x++)
      TEST_ASSERT_EQUAL_HEX16(x < 4 ? toAt(x + 4) : fromAt(x - 4), out[y * FW + x]);
}

static void test_push_follows_the_frame_rotation() {
  // Landscape is rotated 180 degrees: pushing left on screen moves the
  // pixels right in the unrotated buffer
  combine(TRANSITION_PUSH, 0.1f, 1, 2);
  for (int y = 0; y < FH; y++)
    for (int x = 0; x < FW; x++)
      TEST_ASSERT_EQUAL_HEX16(x < 4 ? toAt(x + 4) : fromAt(x - 4), out[y * FW + x]);
}

static void test_push_on_a_portrait_frame_moves_along_the_rows() {
  // Rotation 1: the screen's x runs down the buffer's rows (y = x), its
  // width is the buffer's height. Each pixel tells its buffer row.
  for (int y = 0; y < FH; y++)
    for (int x = 0; x < FW; x++) {
      from[y * FW + x] = (uint16_t)(0xA000 + y);
      to[y * FW + x]   = (uint16_t)(0xB000 + y);
    }
  combine(TRANSITION_PUSH, 0.1f, 1, 1);   // ease 0.5: 2 of 4 rows
  for (int y = 0; y < FH; y++)
    for (int x = 0; x < FW; x++)
      TEST_ASSERT_EQUAL_HEX16(y < 2 ? 0xA000 + y + 2 : 0xB000 + y - 2, out[y * FW + x]);
}

static void test_dissolve_starts_on_the_outgoing_frame() {
  combine(TRANSITION_DISSOLVE, 0.0f);
  assertAll(from);
}

static void test_dissolve_middle_follows_the_bayer_order() {
  // Halfway: the 8 cells of each 4x4 block whose order is under 8
  static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  combine(TRANSITION_DISSOLVE, 0.5f);
  int incoming = 0;
  for (int y = 0; y < FH; y++)
    for (int x = 0; x < FW; x++) {
      const bool in = BAYER[(y & 3) * 4 + (x & 3)] < 8;
      TEST_ASSERT_EQUAL_HEX16(in ? toAt(x) : fromAt(x), out[y * FW + x]);
      incoming += in;
    }
  TEST_ASSERT_EQUAL_INT(FW * FH / 2, incoming);
}

static void test_dissolve_ends_on_the_incoming_frame() {
  combine(TRANSITION_DISSOLVE, 1.0f);
  assertAll(to);
}

static void test_wipe_starts_on_the_outgoing_frame_and_its_edge() {
  combine(TRANSITION_WIPE, 0.0f);
  for (int y = 0; y < FH; y++)
    for (int x = 0; x < FW; x++) TEST_ASSERT_EQUAL_HEX16(fromAt(x), out[y * FW + x]);
}

static void test_wipe_middle_reveals_the_left_with_a_line_edge() {
  // p = 0.1: ease 0.5, x0 = round(0.5 * 9) = 5 (4.5 rounds up)
  combine(TRANSITION_WIPE, 0.1f);
  for (int y = 0; y < FH; y++)
    for (int x = 0; x < FW; x++) {
      const uint16_t want = x < 5 ? toAt(x) : x == 5 ? LINE : fromAt(x);
      TEST_ASSERT_EQUAL_HEX16(want, out[y * FW + x]);
    }
}

static void test_wipe_ends_on_the_incoming_frame_without_edge() {
  combine(TRANSITION_WIPE, 1.0f);
  assertAll(to);
}

static void test_cut_is_the_incoming_frame_at_once() {
  combine(TRANSITION_CUT, 0.0f);
  assertAll(to);
  combine(TRANSITION_CUT, 0.5f);
  assertAll(to);
  combine(TRANSITION_CUT, 1.0f);
  assertAll(to);
}

// --- direction --------------------------------------------------------------------

static void test_knob_direction() {
  TEST_ASSERT_EQUAL_INT8(1, transitionDirection(1));     // next pushes left
  TEST_ASSERT_EQUAL_INT8(1, transitionDirection(3));
  TEST_ASSERT_EQUAL_INT8(-1, transitionDirection(-1));   // previous pushes right
  TEST_ASSERT_EQUAL_INT8(-1, transitionDirection(-2));
}

// --- the passage over several ticks, and its fallback ------------------------------

static int allocations = 0, frees = 0;
static void *countingAlloc(size_t n) { allocations++; return malloc(n); }
static void *failingAlloc(size_t) { allocations++; return nullptr; }
static void countingFree(void *p) { if (p) frees++; free(p); }

static uint16_t frame[FW * FH];

static void fill(uint16_t *f, uint16_t c) { for (int i = 0; i < FW * FH; i++) f[i] = c; }

static void test_passage_cuts_when_the_outgoing_frame_cannot_be_kept() {
  allocations = frees = 0;
  Passage p(failingAlloc, countingFree);
  fill(frame, A);
  TEST_ASSERT_FALSE(p.begin(TRANSITION_PUSH, 1, frame, FW, FH, 0, 0));
  TEST_ASSERT_FALSE(p.running());
  TEST_ASSERT_EQUAL_INT(1, allocations);
  // The incoming screen is drawn and shown as it is
  p.beforeDraw(frame);
  fill(frame, B);
  p.afterDraw(frame, 0, 10, LINE);
  for (int i = 0; i < FW * FH; i++) TEST_ASSERT_EQUAL_HEX16(B, frame[i]);
}

static void test_passage_cut_kind_allocates_nothing() {
  allocations = frees = 0;
  Passage p(countingAlloc, countingFree);
  TEST_ASSERT_FALSE(p.begin(TRANSITION_CUT, 1, frame, FW, FH, 0, 0));
  TEST_ASSERT_FALSE(p.begin(TRANSITION_COUNT, 1, frame, FW, FH, 0, 0));
  TEST_ASSERT_EQUAL_INT(0, allocations);
}

static void test_passage_runs_its_duration_then_frees_its_buffer() {
  allocations = frees = 0;
  {
    Passage p(countingAlloc, countingFree);
    fill(frame, A);
    TEST_ASSERT_TRUE(p.begin(TRANSITION_DISSOLVE, 1, frame, FW, FH, 0, 1000));
    TEST_ASSERT_TRUE(p.running());

    // t = 0: the incoming screen draws, the panel still shows the outgoing one
    p.beforeDraw(frame);
    fill(frame, B);
    p.afterDraw(frame, 0, 1000, LINE);
    for (int i = 0; i < FW * FH; i++) TEST_ASSERT_EQUAL_HEX16(A, frame[i]);

    // The next tick hands the screen back its own last frame, not the mix
    p.beforeDraw(frame);
    for (int i = 0; i < FW * FH; i++) TEST_ASSERT_EQUAL_HEX16(B, frame[i]);
    p.afterDraw(frame, 0, 1200, LINE);
    int incoming = 0;
    for (int i = 0; i < FW * FH; i++) incoming += frame[i] == B;
    TEST_ASSERT_EQUAL_INT(FW * FH / 2, incoming);

    // 400 ms in: the incoming frame, the buffer given back
    p.beforeDraw(frame);
    p.afterDraw(frame, 0, 1400, LINE);
    for (int i = 0; i < FW * FH; i++) TEST_ASSERT_EQUAL_HEX16(B, frame[i]);
    TEST_ASSERT_FALSE(p.running());
    TEST_ASSERT_EQUAL_INT(1, allocations);
    TEST_ASSERT_EQUAL_INT(1, frees);
  }
  TEST_ASSERT_EQUAL_INT(1, frees);
}

static void test_passage_restarts_from_what_is_on_show() {
  allocations = frees = 0;
  Passage p(countingAlloc, countingFree);
  fill(frame, A);
  p.begin(TRANSITION_PUSH, 1, frame, FW, FH, 0, 0);
  p.beforeDraw(frame);
  fill(frame, B);
  p.afterDraw(frame, 0, 24, LINE);   // half A, half B on show
  uint16_t shown[FW * FH];
  memcpy(shown, frame, sizeof(shown));
  // A second turn mid-way: the mix on show is the new outgoing frame, the
  // buffer is kept
  TEST_ASSERT_TRUE(p.begin(TRANSITION_PUSH, 1, frame, FW, FH, 0, 30));
  TEST_ASSERT_EQUAL_INT(1, allocations);
  p.beforeDraw(frame);   // nothing drawn yet by the incoming screen: kept
  fill(frame, LINE);
  p.afterDraw(frame, 0, 30, LINE);
  TEST_ASSERT_EQUAL_HEX16_ARRAY(shown, frame, FW * FH);
}

static void test_passage_cuts_when_the_panel_turns() {
  allocations = frees = 0;
  Passage p(countingAlloc, countingFree);
  fill(frame, A);
  p.begin(TRANSITION_PUSH, 1, frame, FW, FH, 0, 0);
  p.beforeDraw(frame);
  fill(frame, B);
  p.afterDraw(frame, 1, 10, LINE);   // the frame's rotation changed
  for (int i = 0; i < FW * FH; i++) TEST_ASSERT_EQUAL_HEX16(B, frame[i]);
  TEST_ASSERT_FALSE(p.running());
  TEST_ASSERT_EQUAL_INT(1, frees);
}

static void test_passage_cancel_frees_and_leaves_the_frame() {
  allocations = frees = 0;
  Passage p(countingAlloc, countingFree);
  fill(frame, A);
  p.begin(TRANSITION_WIPE, 1, frame, FW, FH, 0, 0);
  p.cancel();
  TEST_ASSERT_FALSE(p.running());
  TEST_ASSERT_EQUAL_INT(1, frees);
  fill(frame, B);
  p.beforeDraw(frame);
  p.afterDraw(frame, 0, 10, LINE);
  for (int i = 0; i < FW * FH; i++) TEST_ASSERT_EQUAL_HEX16(B, frame[i]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_durations_follow_the_design);
  RUN_TEST(test_ease_starts_fast_and_lands_on_one);
  RUN_TEST(test_ease_in_out_is_symmetric_and_soft_at_both_ends);
  RUN_TEST(test_push_starts_on_the_outgoing_frame);
  RUN_TEST(test_push_ends_on_the_incoming_frame);
  RUN_TEST(test_push_next_moves_everything_left);
  RUN_TEST(test_push_previous_moves_everything_right);
  RUN_TEST(test_push_follows_the_frame_rotation);
  RUN_TEST(test_push_on_a_portrait_frame_moves_along_the_rows);
  RUN_TEST(test_dissolve_starts_on_the_outgoing_frame);
  RUN_TEST(test_dissolve_middle_follows_the_bayer_order);
  RUN_TEST(test_dissolve_ends_on_the_incoming_frame);
  RUN_TEST(test_wipe_starts_on_the_outgoing_frame_and_its_edge);
  RUN_TEST(test_wipe_middle_reveals_the_left_with_a_line_edge);
  RUN_TEST(test_wipe_ends_on_the_incoming_frame_without_edge);
  RUN_TEST(test_cut_is_the_incoming_frame_at_once);
  RUN_TEST(test_knob_direction);
  RUN_TEST(test_passage_cuts_when_the_outgoing_frame_cannot_be_kept);
  RUN_TEST(test_passage_cut_kind_allocates_nothing);
  RUN_TEST(test_passage_runs_its_duration_then_frees_its_buffer);
  RUN_TEST(test_passage_restarts_from_what_is_on_show);
  RUN_TEST(test_passage_cuts_when_the_panel_turns);
  RUN_TEST(test_passage_cancel_frees_and_leaves_the_frame);
  return UNITY_END();
}
