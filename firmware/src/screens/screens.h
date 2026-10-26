// SPDX-License-Identifier: AGPL-3.0-only
// Every screen draws one frame and adapts to W() x H().
// `init` is true on the first frame after a change.
#pragma once

#include <Arduino.h>

#include "i18n.h"
#include "states_logic.h"

// --- Mascot ------------------------------------------------------------------
void screenMascotWalk(uint32_t f, bool init);
void screenAgentMonitor(uint32_t f, bool init);
void screenMascotDance(uint32_t f, bool init);
void screenMascotSleep(uint32_t f, bool init);
void screenMascotRun(uint32_t f, bool init);

// --- Effects ----------------------------------------------------------------
void screenLife(uint32_t f, bool init);
void screenBall(uint32_t f, bool init);
void screenMatrix(uint32_t f, bool init);
void screenPlasma(uint32_t f, bool init);
void screenFire(uint32_t f, bool init);
void screenStars(uint32_t f, bool init);
void screenText(uint32_t f, bool init);
void screenRays(uint32_t f, bool init);
void screenBars(uint32_t f, bool init);
void screenBlackHole(uint32_t f, bool init);
void screenCat(uint32_t f, bool init);
void screenIss(uint32_t f, bool init);

// --- Utilities --------------------------------------------------------------
void screenClock(uint32_t f, bool init);
void screenWeather(uint32_t f, bool init);
void screenDate(uint32_t f, bool init);
void screenSystem(uint32_t f, bool init);
void screenMoney(uint32_t f, bool init);
void screenSocial(uint32_t f, bool init);

// --- Images -------------------------------------------------------------------
void screenImage(uint32_t f, bool init);
void screenImageLeave();         // gives the decoder's memory back
uint32_t imageTurns();           // moves at the end of every image's turn: one play

// From the first frame until the panel is ready (boot_logic.h): draws the
// boot sequence and returns true, then false for good once the line opens.
// Called by the mark's task during setup() (boot_task.h), then by loop().
bool drawBootScreen(uint32_t now);
// Over the first screens, after everything else: the opening while it runs
// (the line widening, then the screen rising from it)
void drawBootOpening(uint32_t now);
// Before the screen draws: its own last frame back while the opening runs
void bootStageBack(uint16_t *frame);

// Shown instead of the gallery while the Wi-Fi setup access point is open
void drawWifiSetupScreen(uint32_t f);
bool drawPairingCode();          // sign-in code over everything, false if none
// The reach card (reach.cpp): built when it opens (its QR code from the IP,
// false without a network), then drawn every frame, page 0 or 1 (reach_logic.h)
bool reachCardBuild();
void drawReachCard(uint8_t page);

// The states nobody chooses (screens/states.cpp)
void drawSignInCode(const char *digits, float left);   // six characters; left: 0..1 of its validity
void drawLockedState(const Strings &s);           // in one language: the setting is unreadable
void drawNoImage();                               // IMAGE with nothing stored
// After the screen drew: its reading at half light, the reason in a foot band
void drawStale(Reason why);

// --- Shared helpers ---------------------------------------------------------
// True for 6 frames, then schedules the next blink 2 to 5 s later
bool isBlinking(uint32_t f, uint32_t &next);

// Status dot in the top-right corner: green synced, orange connected, red offline
void drawNetworkStatus(uint32_t f);
