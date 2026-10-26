// SPDX-License-Identifier: AGPL-3.0-only
// What the panel's states say and where, without the drawing: the sign-in
// code's digits and line, why a service shows an old reading, how a long
// address breaks across lines. screens/states.cpp draws them; host-tested in
// test_states.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "i18n.h"

// --- Sign-in code (S1) -----------------------------------------------------------
// Six digits, three and three: a row in landscape, two rows in portrait.
struct DigitSlot {
  int16_t x, y;   // the digit's top left corner
};
DigitSlot signInSlot(uint8_t i, bool tall);

// The code's line, counting its time left down: left is 0..1 of the validity
// and maxW the line's width. A code with any time left keeps one pixel.
int16_t signInBarWidth(float left, int16_t maxW);
static const int16_t SIGNIN_BAR_Y = 29;        // landscape: the foot's last row
static const int16_t SIGNIN_BAR_Y_TALL = 58;   // portrait: under the second group

// The code as the six digits it shows, zeros kept ("000007")
void signInDigits(uint32_t code, char out[7]);

// --- A service with no fresh data (E1) --------------------------------------------
// The reading stays, dimmed, and the foot band gives the reason in a word.
enum Reason : uint8_t { REASON_NONE, REASON_OFFLINE, REASON_NO_KEY, REASON_SERVER };

// Two fetches missed: WEATHER asks about every hour (up to 2 h for the
// first one after start, at the panel's second of the hour), SOCIAL every 10
static const uint32_t WEATHER_FRESH_MS = 130UL * 60UL * 1000UL;
static const uint32_t SOCIAL_FRESH_MS  = 25UL * 60UL * 1000UL;

struct ServiceFacts {
  bool     hasData;      // a reading was fetched at some point
  uint32_t updatedAt;    // millis() of the last success
  bool     wifiUp;
  bool     keyMissing;   // the service needs a key the owner has not given
};

// REASON_NONE while the reading is fresh, or when there is none to keep (the
// screen's own loading state says so). Else the first cause that applies: no
// Wi-Fi (nothing else can be fixed before it), a missing key, the server.
Reason serviceReason(const ServiceFacts &f, uint32_t now, uint32_t freshMs);

// The reason in a word, in the label face; nullptr for REASON_NONE
const char *reasonWord(Reason r, const Strings &t);

// A colour at half its light, each channel alone (RGB565)
uint16_t dimmed(uint16_t c);

// --- The address ---------------------------------------------------------------------
// The panel's name as lines of at most maxW px in the label face: one if it
// fits, else the name and ".local" on two, else cut short to fit. Empty or
// null: no line.
struct AddressLines {
  char    line[2][24];
  uint8_t count;
};
AddressLines addressLines(const char *host, int16_t maxW);
