// SPDX-License-Identifier: AGPL-3.0-only
// Wi-Fi, NTP time and the setup access point. Everything is non-blocking:
// animations keep running while the connection is made or restored.
#pragma once

#include <Arduino.h>

#include "panel_name.h"   // HOSTNAME, the panel's names
#include "wifi_logic.h"

enum NetworkState : uint8_t {
  NET_CONNECTING,   // attempt in progress
  NET_CONNECTED,    // Wi-Fi ok, time not received yet
  NET_SYNCED        // valid time
};

extern NetworkState networkState;
extern bool         timeValid;

#define SETUP_AP_NAME "CAROUSEL"   // + "-XXXX", end of the MAC: setupApName

extern char wifiSsid[33];
extern char wifiPass[65];
extern char setupApName[16];     // "CAROUSEL-695C"

// Setup access point open. Written by loop(), read by the web server.
extern volatile bool     wifiSetupMode;
extern volatile uint8_t  apClientCount;
// Why the saved network refused the last attempt, FAIL_NONE once connected.
extern volatile WifiFailure wifiFailure;
// Restart scheduled after saving: the confirmation page must go out first.
// 0 = none.
extern volatile uint32_t restartAt;

// forceSetup: button held at boot, open the access point right away.
void networkBegin(bool forceSetup);
void networkUpdate();
void saveWifiCredentials(const char *ssid, const char *pass);   // web server side
// True once, after the first join of a network saved from the setup page
bool networkTakeJoinedNew();
// Locked start only (settings.h), before NVS is closed: the radio driver
// keeps its storage in RAM and forgets the network it loaded from NVS.
void networkForgetSaved();
// The Date header of an HTTP answer (HTTPS task): sets the clock while
// NTP has not answered yet; NTP corrects it later.
void networkHttpDate(const char *value);
// The place's time zone, a POSIX TZ (places_logic), daylight saving included
void applyZone(const char *posix);
