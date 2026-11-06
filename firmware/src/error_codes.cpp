// SPDX-License-Identifier: AGPL-3.0-only
// The framework's error texts, replaced by the error code: mbedTLS's
// mbedtls_strerror (about 15 KB of message tables) and ESP-IDF's
// esp_err_to_name (about 7.5 KB). Their text never reached a log here: the
// Arduino logs are compiled out (CORE_DEBUG_LEVEL=0) and nothing prints
// lastError(). The code is what the documentation lists, so nothing is lost.
// These definitions are linked before the framework's archives, so the
// archive members holding the tables are no longer pulled in; if a new
// framework reached them again, the link would fail on a duplicate symbol.
// test/test_error_codes checks the texts.
#include <esp_err.h>
#include <mbedtls/error.h>
#include <stdio.h>

#include <atomic>

// "-0x2700", as mbedTLS's documentation writes its codes; "0x0101" for ESP-IDF
static void codeText(int code, char *buf, size_t len) {
  const unsigned magnitude = code < 0 ? 0u - (unsigned)code : (unsigned)code;
  snprintf(buf, len, "%s0x%04x", code < 0 ? "-" : "", magnitude);
}

void mbedtls_strerror(int errnum, char *buffer, size_t buflen) { codeText(errnum, buffer, buflen); }

const char *esp_err_to_name_r(esp_err_t code, char *buf, size_t buflen) {
  codeText(code, buf, buflen);
  return buf;
}

// esp_err_to_name returns its text with no buffer from the caller, and any
// task may call it. A ring of buffers keeps each text until 4 more calls:
// enough for the few names one log line holds, while two tasks failing at the
// same instant stay rare. Past that, a text can be overwritten before it is
// printed; a buffer per task (thread_local) would lift the limit.
const char *esp_err_to_name(esp_err_t code) {
  static char ring[4][sizeof("-0x80000000")];
  static std::atomic<unsigned> next{0};
  char *buf = ring[next.fetch_add(1) % 4];
  codeText(code, buf, sizeof(ring[0]));
  return buf;
}
