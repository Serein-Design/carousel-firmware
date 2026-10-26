// SPDX-License-Identifier: AGPL-3.0-only
// The setup access point opens when the user needs it and stays open until
// the saved network really works.
//
//   pio test -e native
#include <unity.h>

#include "../../src/wifi_logic.cpp"

void setUp() {}
void tearDown() {}

// Saved network, booting, nothing happened yet
static WifiView booting() {
  WifiView v = {};
  v.haveCredentials = true;
  return v;
}

// Access point opened after the saved network failed, nobody on it
static WifiView inSetup() {
  WifiView v = booting();
  v.setupOpen = true;
  v.offlineMs = WIFI_MISSING_MS;
  return v;
}

static void test_new_panel_opens_setup_at_once() {
  WifiView v = {};
  TEST_ASSERT_EQUAL(WIFI_OPEN_SETUP, wifiDecide(v));
}

static void test_boot_waits_then_opens_setup() {
  WifiView v = booting();
  v.offlineMs = WIFI_MISSING_MS - 1;
  TEST_ASSERT_EQUAL(WIFI_WAIT, wifiDecide(v));
  v.offlineMs = WIFI_MISSING_MS;
  TEST_ASSERT_EQUAL(WIFI_OPEN_SETUP, wifiDecide(v));
}

// The bug: the saved network is in range but the password is wrong. The
// access point used to close as soon as a scan saw the network's name.
static void test_setup_stays_open_while_password_is_wrong() {
  WifiView v = inSetup();
  for (uint32_t t = 0; t < 600000; t += 500) {
    v.apIdleMs = v.sinceAttemptMs = t % SETUP_RETRY_MS;
    TEST_ASSERT_NOT_EQUAL(WIFI_CLOSE_SETUP, wifiDecide(v));
  }
}

static void test_setup_retries_and_closes_once_connected() {
  WifiView v = inSetup();
  v.sinceAttemptMs = SETUP_RETRY_MS;
  TEST_ASSERT_EQUAL(WIFI_RETRY, wifiDecide(v));
  v.connected = true;
  TEST_ASSERT_EQUAL(WIFI_CLOSE_SETUP, wifiDecide(v));
}

static void test_phone_on_access_point_is_never_disturbed() {
  WifiView v = inSetup();
  v.apClients = 1;
  v.sinceAttemptMs = 10 * SETUP_RETRY_MS;
  TEST_ASSERT_EQUAL(WIFI_WAIT, wifiDecide(v));
  v.connected = true;
  TEST_ASSERT_EQUAL(WIFI_WAIT, wifiDecide(v));
}

static void test_forced_setup_waits_for_the_user() {
  WifiView v = inSetup();
  v.setupForced = true;
  v.sinceAttemptMs = v.apIdleMs = SETUP_FORCED_MS - 1;
  TEST_ASSERT_EQUAL(WIFI_WAIT, wifiDecide(v));
  // Nobody came: back to normal, the saved network takes over
  v.sinceAttemptMs = v.apIdleMs = SETUP_FORCED_MS;
  TEST_ASSERT_EQUAL(WIFI_RETRY, wifiDecide(v));
}

static void test_setup_without_credentials_never_retries() {
  WifiView v = inSetup();
  v.haveCredentials = false;
  v.sinceAttemptMs = 10 * SETUP_RETRY_MS;
  TEST_ASSERT_EQUAL(WIFI_WAIT, wifiDecide(v));
}

static void test_lost_network_retries_without_setup() {
  WifiView v = booting();
  v.everConnected = true;
  v.offlineMs = 10 * WIFI_MISSING_MS;
  v.sinceAttemptMs = WIFI_RETRY_MS;
  TEST_ASSERT_EQUAL(WIFI_RETRY, wifiDecide(v));
}

static void test_password_changed_opens_setup() {
  WifiView v = booting();
  v.everConnected = true;
  v.authFailures = WIFI_AUTH_FAILS_MAX - 1;
  TEST_ASSERT_NOT_EQUAL(WIFI_OPEN_SETUP, wifiDecide(v));
  v.authFailures = WIFI_AUTH_FAILS_MAX;
  TEST_ASSERT_EQUAL(WIFI_OPEN_SETUP, wifiDecide(v));
}

static void test_disconnect_reasons() {
  TEST_ASSERT_EQUAL(FAIL_NOT_FOUND, classifyDisconnect(201));
  TEST_ASSERT_EQUAL(FAIL_WRONG_PASSWORD, classifyDisconnect(15));
  TEST_ASSERT_EQUAL(FAIL_WRONG_PASSWORD, classifyDisconnect(202));
  TEST_ASSERT_EQUAL(FAIL_WRONG_PASSWORD, classifyDisconnect(204));
  TEST_ASSERT_EQUAL(FAIL_NONE, classifyDisconnect(8));     // ASSOC_LEAVE: ours
  TEST_ASSERT_EQUAL(FAIL_NONE, classifyDisconnect(200));   // BEACON_TIMEOUT
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_new_panel_opens_setup_at_once);
  RUN_TEST(test_boot_waits_then_opens_setup);
  RUN_TEST(test_setup_stays_open_while_password_is_wrong);
  RUN_TEST(test_setup_retries_and_closes_once_connected);
  RUN_TEST(test_phone_on_access_point_is_never_disturbed);
  RUN_TEST(test_forced_setup_waits_for_the_user);
  RUN_TEST(test_setup_without_credentials_never_retries);
  RUN_TEST(test_lost_network_retries_without_setup);
  RUN_TEST(test_password_changed_opens_setup);
  RUN_TEST(test_disconnect_reasons);
  return UNITY_END();
}
