// SPDX-License-Identifier: AGPL-3.0-only
#include "update_logic.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

bool parseVersion(const char *v, int out[3]) {
  if (!v) return false;
  if (*v == 'v') v++;
  for (int i = 0; i < 3; i++) {
    if (!isdigit((unsigned char)*v)) return false;
    int n = 0;
    while (isdigit((unsigned char)*v)) {
      n = n * 10 + (*v++ - '0');
      if (n > 9999) return false;
    }
    out[i] = n;
    if (i < 2 && *v++ != '.') return false;
  }
  return *v == '\0' || *v == '-';
}

bool isNewer(const char *candidate, const char *current) {
  int a[3], b[3];
  if (!parseVersion(candidate, a) || !parseVersion(current, b)) return false;
  for (int i = 0; i < 3; i++)
    if (a[i] != b[i]) return a[i] > b[i];
  return false;
}

bool isReleaseVersion(const char *v) {
  int n[3];
  return v && v[0] == 'v' && parseVersion(v, n) && !strchr(v, '-');
}

void signedMessage(char *out, size_t size, const char *version, const uint8_t sha[32]) {
  int n = snprintf(out, size, "carousel-firmware %s ", version);
  for (int i = 0; i < 32 && n > 0 && (size_t)n + 2 < size; i++)
    n += snprintf(out + n, size - n, "%02x", sha[i]);
}

static int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool hexToBytes(const char *hex, uint8_t *out, size_t max, size_t &len) {
  const size_t n = strlen(hex);
  if (n % 2 || n / 2 > max) return false;
  for (size_t i = 0; i < n / 2; i++) {
    const int hi = hexDigit(hex[2 * i]), lo = hexDigit(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)(hi << 4 | lo);
  }
  len = n / 2;
  return true;
}
