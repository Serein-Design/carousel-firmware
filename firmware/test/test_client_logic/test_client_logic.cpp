// SPDX-License-Identifier: AGPL-3.0-only
// The web server's one client at a time: a socket that has sent nothing for
// over IDLE_CLIENT_MS is dropped only when another connection waits behind
// it; a lone slow client, or one whose request has started, is kept.
//
//   pio test -e native -f test_client_logic
#include <unity.h>

#include "../../src/client_logic.h"

void setUp() {}
void tearDown() {}

// A browser's preconnected socket in front of a hook
static void test_drops_idle_client_when_another_waits() {
  TEST_ASSERT_TRUE(dropIdleClient(true, false, IDLE_CLIENT_MS + 1, true));
  TEST_ASSERT_TRUE(dropIdleClient(true, false, 4000, true));
}

// The threshold itself still waits: catches >= and a lower threshold
static void test_keeps_client_at_threshold() {
  TEST_ASSERT_EQUAL_UINT32(500, IDLE_CLIENT_MS);
  TEST_ASSERT_FALSE(dropIdleClient(true, false, 500, true));
  TEST_ASSERT_FALSE(dropIdleClient(true, false, 0, true));
}

// Alone, a slow client keeps the framework's 5 s
static void test_keeps_lone_idle_client() {
  TEST_ASSERT_FALSE(dropIdleClient(true, false, 501, false));
  TEST_ASSERT_FALSE(dropIdleClient(true, false, 4999, false));
}

// Bytes waiting to be read: the request has started
static void test_keeps_client_with_data() {
  TEST_ASSERT_FALSE(dropIdleClient(true, true, 501, true));
}

// No client yet, or one past its request (waiting to close)
static void test_keeps_client_not_waiting_read() {
  TEST_ASSERT_FALSE(dropIdleClient(false, false, 501, true));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_drops_idle_client_when_another_waits);
  RUN_TEST(test_keeps_client_at_threshold);
  RUN_TEST(test_keeps_lone_idle_client);
  RUN_TEST(test_keeps_client_with_data);
  RUN_TEST(test_keeps_client_not_waiting_read);
  return UNITY_END();
}
