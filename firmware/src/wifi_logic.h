// SPDX-License-Identifier: AGPL-3.0-only
// When to open, keep or close the Wi-Fi setup access point, and when to try
// the saved network again. Pure logic, no Arduino: network.cpp feeds it the
// current situation and carries out the answer; test/test_wifi_logic checks
// it on the PC.
#pragma once

#include <stdint.h>

#define WIFI_MISSING_MS     45000UL   // never connected since boot: setup
#define WIFI_RETRY_MS       15000UL   // offline: new attempt
#define SETUP_RETRY_MS      30000UL   // setup open, nobody on it: new attempt
#define SETUP_FORCED_MS     120000UL  // button held at boot: no attempt before
#define WIFI_AUTH_FAILS_MAX 3         // rejected that many times: setup

// Why the last attempt failed, as the user can act on it.
enum WifiFailure : uint8_t { FAIL_NONE, FAIL_NOT_FOUND, FAIL_WRONG_PASSWORD };

// From the ESP-IDF disconnect reason (wifi_err_reason_t). Anything else
// (our own disconnect, a beacon lost) says nothing about the settings.
WifiFailure classifyDisconnect(uint8_t reason);

struct WifiView {
  bool     haveCredentials;   // an SSID is saved
  bool     connected;         // associated, also while the access point is open
  bool     everConnected;     // since boot
  bool     setupOpen;
  bool     setupForced;       // encoder button held at boot
  uint8_t  apClients;         // phones on the access point
  uint8_t  authFailures;      // consecutive wrong-password rejections
  uint32_t offlineMs;         // since boot or since the connection was lost
  uint32_t apIdleMs;          // since someone was last on the access point
  uint32_t sinceAttemptMs;    // since the last WiFi.begin()
};

enum WifiAction : uint8_t { WIFI_WAIT, WIFI_OPEN_SETUP, WIFI_CLOSE_SETUP, WIFI_RETRY };

WifiAction wifiDecide(const WifiView &v);
