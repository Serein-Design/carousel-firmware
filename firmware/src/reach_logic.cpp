// SPDX-License-Identifier: AGPL-3.0-only
#include "reach_logic.h"

#include <stdio.h>
#include <string.h>

#include "type.h"

Press PressTimer::update(bool closed, uint32_t now) {
  if (!closed) {
    opened_ = true;
    if (!down_) return Press::NONE;
    down_ = false;
    return now - since_ >= LONG_PRESS_MS ? Press::LONG : Press::SHORT;
  }
  if (opened_ && !down_) {
    down_  = true;
    since_ = now;
  }
  return Press::NONE;
}

bool ReachCard::shown(uint32_t now) {
  if (on_ && now - shownAt_ >= REACH_SHOW_MS) on_ = false;
  return on_;
}

uint8_t setupPage(uint32_t ms) { return ms / REACH_PAGE_MS % SETUP_PAGES; }

// snprintf's count, so a cut text is refused rather than encoded
static bool fitted(int len, size_t n) { return len > 0 && (size_t)len < n; }

bool apQrText(char *out, size_t n, const char *ssid) {
  return fitted(snprintf(out, n, "WIFI:S:%s;T:nopass;;", ssid), n);
}

bool pageQrText(char *out, size_t n, const uint8_t ip[4]) {
  return fitted(snprintf(out, n, "http://%u.%u.%u.%u/", ip[0], ip[1], ip[2], ip[3]), n);
}

void addressLines(const char *host, uint16_t maxPx, char *a, char *b, size_t n) {
  const char *dot = strchr(host, '.');
  if (textWidth(host, Face::LABEL) <= maxPx || !dot) {
    snprintf(a, n, "%s", host);
    b[0] = '\0';
    return;
  }
  snprintf(a, n, "%.*s", (int)(dot - host), host);
  snprintf(b, n, "%s", dot);
}

void ipLines(const uint8_t ip[4], char *a, char *b, size_t n) {
  snprintf(a, n, "%u.%u.", ip[0], ip[1]);
  snprintf(b, n, "%u.%u", ip[2], ip[3]);
}

void apNameTall(const char *ap, char lines[3][8]) {
  const char *dash = strchr(ap, '-');
  snprintf(lines[0], 8, "%.4s-", ap);
  snprintf(lines[1], 8, "%.4s-", strlen(ap) > 4 ? ap + 4 : "");
  snprintf(lines[2], 8, "%s", dash ? dash + 1 : "");
}
