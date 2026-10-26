// SPDX-License-Identifier: AGPL-3.0-only
#include "states_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "type.h"
#include "weather_logic.h"

DigitSlot signInSlot(uint8_t i, bool tall) {
  // Pierre digits are 8 px wide on a 9 px pitch. Landscape: 3 + 3 across,
  // 4 px more between the groups, centred on the 64 px (3 px each side).
  // Portrait: a group per row, 3 px in, a clear row between them.
  if (!tall) return {(int16_t)(3 + 9 * i + (i >= 3 ? 4 : 0)), 9};
  return {(int16_t)(3 + 9 * (i % 3)), (int16_t)(i < 3 ? 14 : 30)};
}

int16_t signInBarWidth(float left, int16_t maxW) {
  if (!(left > 0)) return 0;
  if (left >= 1) return maxW;
  const int16_t w = (int16_t)lroundf(left * maxW);
  return w < 1 ? 1 : w;
}

void signInDigits(uint32_t code, char out[7]) { snprintf(out, 7, "%06lu", (unsigned long)(code % 1000000UL)); }

Reason serviceReason(const ServiceFacts &f, uint32_t now, uint32_t freshMs) {
  if (!f.hasData || now - f.updatedAt <= freshMs) return REASON_NONE;
  if (!f.wifiUp) return REASON_OFFLINE;
  if (f.keyMissing) return REASON_NO_KEY;
  return REASON_SERVER;
}

const char *reasonWord(Reason r, const Strings &t) {
  switch (r) {
    case REASON_OFFLINE: return t.staleWords[0];
    case REASON_NO_KEY:  return t.staleWords[1];
    case REASON_SERVER:  return t.staleWords[2];
    default:             return nullptr;
  }
}

uint16_t dimmed(uint16_t c) {
  // The shift drags each channel's lowest bit into the one below: the mask
  // clears those (bits 10 and 4 here, and bit 15)
  return (uint16_t)((c >> 1) & 0x7BEF);
}

AddressLines addressLines(const char *host, int16_t maxW) {
  AddressLines a = {};
  if (!host || !host[0]) return a;
  snprintf(a.line[0], sizeof(a.line[0]), "%s", host);
  a.count = 1;
  if (textWidth(a.line[0], Face::LABEL) <= maxW) return a;
  // The name and its ".local" apart: the unique name is wider than a line
  const char *dot = strchr(host, '.');
  if (dot && dot != host) {
    char head[24], tail[24];
    snprintf(head, sizeof(head), "%.*s", (int)(dot - host), host);
    snprintf(tail, sizeof(tail), "%s", dot);
    if (textWidth(head, Face::LABEL) <= maxW && textWidth(tail, Face::LABEL) <= maxW) {
      memcpy(a.line[0], head, sizeof(head));
      memcpy(a.line[1], tail, sizeof(tail));
      a.count = 2;
      return a;
    }
  }
  fitText(a.line[0], Face::LABEL, (uint16_t)maxW);
  return a;
}
