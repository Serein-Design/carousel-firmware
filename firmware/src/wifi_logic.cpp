// SPDX-License-Identifier: AGPL-3.0-only
#include "wifi_logic.h"

WifiFailure classifyDisconnect(uint8_t reason) {
  switch (reason) {
    case 201:                 // NO_AP_FOUND
      return FAIL_NOT_FOUND;
    case 15:                  // 4WAY_HANDSHAKE_TIMEOUT: WPA2 key refused
    case 202:                 // AUTH_FAIL
    case 204:                 // HANDSHAKE_TIMEOUT
      return FAIL_WRONG_PASSWORD;
    default:
      return FAIL_NONE;
  }
}

WifiAction wifiDecide(const WifiView &v) {
  if (v.setupOpen) {
    // A phone is on the access point: an attempt sweeps every channel and
    // would kick it off.
    if (v.apClients) return WIFI_WAIT;
    // Opened on purpose: leave time to join before the saved network, if it
    // works, takes over.
    if (v.setupForced && v.apIdleMs < SETUP_FORCED_MS) return WIFI_WAIT;
    // Only a real association proves the network is back and the password
    // right. Seeing its name in a scan proved neither: with a wrong password,
    // the access point closed within seconds, again and again.
    if (v.connected) return WIFI_CLOSE_SETUP;
    if (v.haveCredentials && v.sinceAttemptMs >= SETUP_RETRY_MS) return WIFI_RETRY;
    return WIFI_WAIT;
  }

  if (v.connected) return WIFI_WAIT;
  if (!v.haveCredentials) return WIFI_OPEN_SETUP;    // new panel: nothing to wait for
  // Refused again and again: the password changed, waiting will not help
  if (v.authFailures >= WIFI_AUTH_FAILS_MAX) return WIFI_OPEN_SETUP;
  // Never connected since boot: wrong place or wrong settings. Connected
  // then lost: the network is the right one and will come back (router
  // rebooting, hotspot switched off); the access point would only delay it.
  if (!v.everConnected && v.offlineMs >= WIFI_MISSING_MS) return WIFI_OPEN_SETUP;
  return v.sinceAttemptMs >= WIFI_RETRY_MS ? WIFI_RETRY : WIFI_WAIT;
}
