// SPDX-License-Identifier: AGPL-3.0-only
// The HTTPS task's timetable without the panel: each job's turn comes at its
// own time or when another task asks for it, and the task never sleeps past
// the soonest turn nor longer than a minute, whatever millis() reads.
//
//   pio test -e native -f test_https
#include <unity.h>

#include "../../src/https_logic.cpp"

void setUp() {}
void tearDown() {}

static void test_turn_comes_at_its_time() {
  HttpsSchedule s;
  s.after(HTTPS_SOCIAL, 1000, 9000);
  bool asked = true;
  TEST_ASSERT_FALSE(s.take(HTTPS_SOCIAL, 9999, asked));
  TEST_ASSERT_TRUE(s.take(HTTPS_SOCIAL, 10000, asked));
  TEST_ASSERT_FALSE(asked);
  // Still due until the job says when it wants its next turn
  TEST_ASSERT_TRUE(s.take(HTTPS_SOCIAL, 10001, asked));
  s.after(HTTPS_SOCIAL, 10001, 600000);
  TEST_ASSERT_FALSE(s.take(HTTPS_SOCIAL, 10002, asked));
}

static void test_asked_comes_now_once() {
  HttpsSchedule s;
  s.after(HTTPS_WEATHER, 0, 3600000);
  s.after(HTTPS_UPDATE, 0, 60000);
  s.wake(HTTPS_WEATHER);
  bool asked = false;
  TEST_ASSERT_TRUE(s.take(HTTPS_WEATHER, 5000, asked));
  TEST_ASSERT_TRUE(asked);
  // The ask is taken with the turn and leaves the job's time as it was;
  // the other jobs keep theirs
  TEST_ASSERT_FALSE(s.take(HTTPS_WEATHER, 5001, asked));
  TEST_ASSERT_FALSE(asked);
  TEST_ASSERT_TRUE(s.take(HTTPS_WEATHER, 3600000, asked));
  TEST_ASSERT_FALSE(s.take(HTTPS_UPDATE, 5000, asked));
}

static void test_ask_survives_a_reschedule() {
  // Asked while its own turn runs: the next look is at once, and asked
  HttpsSchedule s;
  s.after(HTTPS_UPDATE, 0, 0);
  bool asked = false;
  TEST_ASSERT_TRUE(s.take(HTTPS_UPDATE, 0, asked));
  s.wake(HTTPS_UPDATE);
  s.after(HTTPS_UPDATE, 200, 60000);
  TEST_ASSERT_TRUE(s.take(HTTPS_UPDATE, 200, asked));
  TEST_ASSERT_TRUE(asked);
}

static void test_wait_until_the_soonest() {
  HttpsSchedule s;
  s.after(HTTPS_WEATHER, 0, 40000);
  s.after(HTTPS_UPDATE, 0, 60000);
  s.after(HTTPS_SOCIAL, 0, 9000);
  TEST_ASSERT_EQUAL_UINT32(9000, s.waitMs(0));
  TEST_ASSERT_EQUAL_UINT32(4000, s.waitMs(5000));
  TEST_ASSERT_EQUAL_UINT32(0, s.waitMs(12000));       // overdue: at once
  s.after(HTTPS_SOCIAL, 12000, 600000);
  s.wake(HTTPS_UPDATE);
  TEST_ASSERT_EQUAL_UINT32(0, s.waitMs(12000));       // asked: at once
}

static void test_wait_a_minute_at_most() {
  // The task looks once a minute anyway: the city table freed once unused
  HttpsSchedule s;
  for (int j = 0; j < HTTPS_JOBS; j++) s.after((HttpsJob)j, 0, 3600000);
  TEST_ASSERT_EQUAL_UINT32(HTTPS_LOOK_MS, s.waitMs(0));
}

static void test_millis_wraps() {
  HttpsSchedule s;
  const uint32_t now = 0xFFFFF000u;                    // 4 s before the wrap
  s.after(HTTPS_SOCIAL, now, 10000);
  s.after(HTTPS_WEATHER, now, 3600000);
  s.after(HTTPS_UPDATE, now, 60000);
  bool asked;
  TEST_ASSERT_FALSE(s.take(HTTPS_SOCIAL, 0x00000010u, asked));
  TEST_ASSERT_EQUAL_UINT32(10000 - 0x1010, s.waitMs(0x00000010u));
  TEST_ASSERT_TRUE(s.take(HTTPS_SOCIAL, now + 10000, asked));
  TEST_ASSERT_FALSE(s.take(HTTPS_WEATHER, now + 10000, asked));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_turn_comes_at_its_time);
  RUN_TEST(test_asked_comes_now_once);
  RUN_TEST(test_ask_survives_a_reschedule);
  RUN_TEST(test_wait_until_the_soonest);
  RUN_TEST(test_wait_a_minute_at_most);
  RUN_TEST(test_millis_wraps);
  return UNITY_END();
}
