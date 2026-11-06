// SPDX-License-Identifier: AGPL-3.0-only
// The framework's error texts without their tables: a TLS or ESP-IDF error
// reads as its code, the number its documentation lists.
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

static void test_esp_error_is_its_code() {
  TEST_ASSERT_EQUAL_STRING("0x0101", esp_err_to_name(0x101));  // ESP_ERR_NO_MEM
  TEST_ASSERT_EQUAL_STRING("0x3001", esp_err_to_name(0x3001)); // ESP_ERR_WIFI_NOT_INIT
  TEST_ASSERT_EQUAL_STRING("0x0000", esp_err_to_name(ESP_OK));
  TEST_ASSERT_EQUAL_STRING("-0x0001", esp_err_to_name(ESP_FAIL));
  TEST_ASSERT_EQUAL_STRING("-0x80000000", esp_err_to_name(-2147483647 - 1));
}

static void test_esp_names_outlive_the_next_calls() {
  // A log line may hold a few names at once: each keeps its text
  const char *a = esp_err_to_name(0x101), *b = esp_err_to_name(0x102),
             *c = esp_err_to_name(0x103), *d = esp_err_to_name(0x104);
  TEST_ASSERT_EQUAL_STRING("0x0101", a);
  TEST_ASSERT_EQUAL_STRING("0x0102", b);
  TEST_ASSERT_EQUAL_STRING("0x0103", c);
  TEST_ASSERT_EQUAL_STRING("0x0104", d);
}

static void test_esp_name_in_callers_buffer() {
  char buf[16];
  TEST_ASSERT_EQUAL_PTR(buf, esp_err_to_name_r(0x3001, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("0x3001", buf);
  TEST_ASSERT_EQUAL_PTR(buf, esp_err_to_name_r(0x3001, buf, 5));
  TEST_ASSERT_EQUAL_STRING("0x30", buf);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tls_error_is_its_code);
  RUN_TEST(test_tls_error_fits_the_buffer);
  RUN_TEST(test_esp_error_is_its_code);
  RUN_TEST(test_esp_names_outlive_the_next_calls);
  RUN_TEST(test_esp_name_in_callers_buffer);
  return UNITY_END();
}
