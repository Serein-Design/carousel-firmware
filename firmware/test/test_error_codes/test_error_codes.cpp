// SPDX-License-Identifier: AGPL-3.0-only
// The framework's error texts without their tables: a TLS error reads as its
// code, the number mbedTLS's documentation lists.
//
//   pio test -e native -f test_error_codes
#include <unity.h>

#include <string.h>

#include "../../src/error_codes.cpp"

void setUp() {}
void tearDown() {}

static void test_tls_error_is_its_code() {
  char buf[100];
  mbedtls_strerror(-0x2700, buf, sizeof(buf));  // certificate verification failed
  TEST_ASSERT_EQUAL_STRING("-0x2700", buf);
  mbedtls_strerror(-0x7880, buf, sizeof(buf));  // peer closed the connection
  TEST_ASSERT_EQUAL_STRING("-0x7880", buf);
  mbedtls_strerror(-0x50, buf, sizeof(buf));    // low-level codes are padded alike
  TEST_ASSERT_EQUAL_STRING("-0x0050", buf);
}

static void test_tls_error_fits_the_buffer() {
  char buf[8];
  memset(buf, 'x', sizeof(buf));
  mbedtls_strerror(-0x2700, buf, 4);
  TEST_ASSERT_EQUAL_STRING("-0x", buf);         // cut, still terminated
  TEST_ASSERT_EQUAL_CHAR('x', buf[4]);
  mbedtls_strerror(-0x2700, buf, 0);            // no room: nothing written
  TEST_ASSERT_EQUAL_CHAR('-', buf[0]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tls_error_is_its_code);
  RUN_TEST(test_tls_error_fits_the_buffer);
  return UNITY_END();
}
