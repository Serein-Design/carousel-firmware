// SPDX-License-Identifier: AGPL-3.0-only
// Firmware updates over Wi-Fi (ArduinoOTA), so the board never needs the
// BOOT/EN dance again once this firmware is on it.
#pragma once

// True while an update is being received: background fetches wait, so the
// update gets the network to itself.
extern volatile bool otaInProgress;

void otaBegin();     // once connected; does nothing without OTA_PASSWORD
void otaHandle();    // every frame

// A firmware fresh from an update runs on trial: if it crashes or restarts
// before proving itself (Wi-Fi up and time received, 30 s in), the
// bootloader goes back to the previous one. Every frame.
void otaConfirmWhenHealthy();
