// SPDX-License-Identifier: AGPL-3.0-only
// The framework's error texts, replaced by the error code: mbedTLS's
// mbedtls_strerror (about 15 KB of message tables). Its text never reached a
// log here: the Arduino logs are compiled out (CORE_DEBUG_LEVEL=0) and nothing
// prints lastError(). The code is what the documentation lists, so nothing is
// lost. This definition is linked before the framework's archives, so the
// archive member holding the tables is no longer pulled in; if a new
// framework reached it again, the link would fail on a duplicate symbol.
// test/test_error_codes checks the texts.
#include <mbedtls/error.h>
#include <stdio.h>

// "-0x2700", as mbedTLS's documentation writes its codes
static void codeText(int code, char *buf, size_t len) {
  const unsigned magnitude = code < 0 ? 0u - (unsigned)code : (unsigned)code;
  snprintf(buf, len, "%s0x%04x", code < 0 ? "-" : "", magnitude);
}

void mbedtls_strerror(int errnum, char *buffer, size_t buflen) { codeText(errnum, buffer, buflen); }
