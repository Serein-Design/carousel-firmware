// SPDX-License-Identifier: AGPL-3.0-only
// Stand-in for the ESP32 Wi-Fi object: the link state, its signal and the
// address the screens print. A test sets them.
#pragma once

#include <cstdint>

enum wl_status_t : uint8_t { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };

struct IPAddress {
  uint8_t b[4] = {0, 0, 0, 0};
  uint8_t operator[](int i) const { return b[i]; }
};

struct FakeWiFi {
  wl_status_t link = WL_DISCONNECTED;
  IPAddress ip;
  int rssi = 0;
  wl_status_t status() const { return link; }
  int RSSI() const { return rssi; }
  IPAddress localIP() const { return ip; }
};
inline FakeWiFi WiFi;
