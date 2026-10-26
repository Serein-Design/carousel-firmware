// SPDX-License-Identifier: AGPL-3.0-only
// Signed firmware updates, pulled by the panel itself from the project's
// GitHub releases. A release carries manifest.json:
//   {"version":"v0.2.0","url":"https://.../firmware.bin","size":1234567,
//    "sig":"<ECDSA P-256 signature, DER, hex>"}
// The panel downloads the image, hashes it on the way, and installs it only
// if the signature over "carousel-firmware <version> <sha256>" matches the
// public key built in, and the version is newer. A new image then runs on
// trial (ota.h): if it fails, the bootloader goes back and that version is
// never tried again.
//
// Release builds check every night around 4 h and install by themselves
// (unless switched off on the settings page); any build can be told to
// check and install from the settings page.
#pragma once

#include <Arduino.h>

enum UpdateState : uint8_t {
  UPD_IDLE,          // never checked
  UPD_CHECKING,
  UPD_UP_TO_DATE,
  UPD_AVAILABLE,     // latest holds the version
  UPD_INSTALLING,
  UPD_FAILED,        // error says why
};

struct UpdateStatus {
  volatile UpdateState state = UPD_IDLE;
  char  latest[24] = "";
  char  error[48]  = "";
  volatile float progress = -1;   // 0..1 while installing, -1 otherwise
};
extern UpdateStatus updateStatus;

void updateBegin();          // before httpsBegin(): the last install's outcome
// The HTTPS task's turn (https_task.cpp): checks, and installs, when asked
// or at night; returns the ms until the next turn
uint32_t updateStep(bool asked);
void updateCheckNow();       // from the settings page
void updateInstallNow();     // from the settings page: check, then install if newer
bool drawUpdateProgress();   // display loop: progress bar while installing
