// SPDX-License-Identifier: AGPL-3.0-only
#include "web_forms.h"

#include <string.h>

#include "i18n.h"

FormVerdict formGate(const FormInput &in) {
  if (!in.hostOk || in.crossSite) return FORM_FORBIDDEN;
  if (!in.cookieOk) return FORM_SIGN_IN;
  if (in.announced < 0) return FORM_BAD_LENGTH;
  if ((size_t)in.announced > FORM_BODY_MAX) return FORM_TOO_LARGE;
  if (in.locked) return FORM_LOCKED;
  return FORM_ACCEPT;
}

void wipeSecret(void *p, size_t n) {
  volatile unsigned char *b = (volatile unsigned char *)p;
  while (n--) *b++ = 0;
}

bool FormBody::set(const char *data, size_t n) {
  n_ = 0;
  data_[0] = '\0';
  if (n > FORM_BODY_MAX) return false;
  memcpy(data_, data, n);
  data_[n] = '\0';
  n_ = strlen(data_);   // a NUL ends it: every value stays a C string
  return true;
}

static int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Decodes [p, end) into out (at most size - 1 bytes and a NUL; size 0:
// nothing written) and returns the whole decoded length. cmp: compared
// against instead, byte for byte; returns -1 on the first difference.
static int decode(const char *p, const char *end, char *out, size_t size, const char *cmp) {
  int n = 0;
  while (p < end) {
    char c = *p++;
    if (c == '+') {
      c = ' ';
    } else if (c == '%' && end - p >= 2) {
      const int hi = hexDigit(p[0]), lo = hexDigit(p[1]);
      if (hi >= 0 && lo >= 0 && (hi | lo)) {   // %00 kept as typed
        c = (char)(hi << 4 | lo);
        p += 2;
      }
    }
    if (cmp) {
      if (cmp[n] != c) return -1;
    } else if ((size_t)n + 1 < size) {
      out[n] = c;
    }
    n++;
  }
  if (cmp) return cmp[n] ? -1 : n;
  if (size) out[(size_t)n < size ? n : size - 1] = '\0';
  return n;
}

int FormBody::value(const char *name, char *out, size_t size) const {
  if (size) out[0] = '\0';
  if (!name || !*name) return -1;
  const char *const end = data_ + n_;
  for (const char *pair = data_; pair < end;) {
    const char *amp = (const char *)memchr(pair, '&', end - pair);
    if (!amp) amp = end;
    const char *eq = (const char *)memchr(pair, '=', amp - pair);
    const char *nameEnd = eq ? eq : amp;
    if (decode(pair, nameEnd, nullptr, 0, name) >= 0)
      return eq ? decode(eq + 1, amp, out, size, nullptr) : 0;
    pair = amp + 1;
  }
  return -1;
}

bool FormBody::has(const char *name) const { return value(name, nullptr, 0) >= 0; }

bool formLevel(const char *text, int levels, int *level) {
  if (!text || !*text || (text[0] == '0' && text[1])) return false;
  long v = 0;
  for (const char *p = text; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10 + (*p - '0');
    if (v >= levels) return false;
  }
  *level = (int)v;
  return true;
}

void panelMessage(const char *typed, char *out, size_t size) {
  if (!size) return;
  toMessageText(typed, out, size);
  size_t n = strlen(out);
  if (!n || out[n - 1] == ' ') return;
  // A gap before it loops, cut as the field cuts it when the text is long
  for (const char *g = " * "; *g && n + 1 < size; g++) out[n++] = *g;
  out[n] = '\0';
}

WifiValues wifiValues(int ssidLen, int passLen) {
  if (ssidLen < 1 || ssidLen > 32) return WIFI_BAD_SSID;
  if (passLen > 0 && (passLen < 8 || passLen > 63)) return WIFI_BAD_PASSWORD;
  return WIFI_VALUES_OK;
}

bool wifiRetrySaved(const char *ssid, int passLen, const char *saved) {
  return passLen <= 0 && ssid && saved && saved[0] && strcmp(ssid, saved) == 0;
}
