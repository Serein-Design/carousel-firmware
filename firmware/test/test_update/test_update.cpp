// SPDX-License-Identifier: AGPL-3.0-only
// Signed updates without the network: which versions count as newer, which
// builds update themselves, and the exact message the release workflow signs.
//
//   pio test -e native
#include <unity.h>

#include "../../src/update_logic.cpp"

void setUp() {}
void tearDown() {}

static void test_versions() {
  TEST_ASSERT_TRUE(isNewer("v0.2.0", "v0.1.0"));
  TEST_ASSERT_TRUE(isNewer("v0.10.0", "v0.9.9"));          // numbers, not text
  TEST_ASSERT_TRUE(isNewer("v1.0.0", "v0.99.99"));
  TEST_ASSERT_FALSE(isNewer("v0.1.0", "v0.1.0"));          // same: nothing to do
  TEST_ASSERT_FALSE(isNewer("v0.1.0", "v0.2.0"));          // never downgrade
  TEST_ASSERT_TRUE(isNewer("v0.2.0", "v0.1.0-12-gabc"));   // dev build of 0.1.0
  TEST_ASSERT_FALSE(isNewer("garbage", "v0.1.0"));
  TEST_ASSERT_FALSE(isNewer("v0.2", "v0.1.0"));
  TEST_ASSERT_FALSE(isNewer("v0.2.0x", "v0.1.0"));
}

static void test_release_builds_only() {
  TEST_ASSERT_TRUE(isReleaseVersion("v0.2.0"));
  TEST_ASSERT_FALSE(isReleaseVersion("v0.2.0-3-gabcdef"));
  TEST_ASSERT_FALSE(isReleaseVersion("v0.2.0-dirty"));
  TEST_ASSERT_FALSE(isReleaseVersion("0.2.0"));
  TEST_ASSERT_FALSE(isReleaseVersion("abc1234"));          // no tag at all
}

// Must match .github/workflows/release.yml byte for byte
static void test_signed_message_format() {
  uint8_t sha[32];
  for (int i = 0; i < 32; i++) sha[i] = (uint8_t)(i * 8 + 1);
  char msg[120];
  signedMessage(msg, sizeof(msg), "v0.2.0", sha);
  TEST_ASSERT_EQUAL_STRING(
    "carousel-firmware v0.2.0 "
    "0109111921293139414951596169717981899199a1a9b1b9c1c9d1d9e1e9f1f9", msg);
}

static void test_hex() {
  uint8_t b[4];
  size_t n = 0;
  TEST_ASSERT_TRUE(hexToBytes("00ff10Ab", b, sizeof(b), n));
  TEST_ASSERT_EQUAL(4, n);
  TEST_ASSERT_EQUAL_HEX8(0xAB, b[3]);
  TEST_ASSERT_FALSE(hexToBytes("abc", b, sizeof(b), n));        // odd
  TEST_ASSERT_FALSE(hexToBytes("zz", b, sizeof(b), n));         // not hex
  TEST_ASSERT_FALSE(hexToBytes("0011223344", b, sizeof(b), n)); // too long
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_versions);
  RUN_TEST(test_release_builds_only);
  RUN_TEST(test_signed_message_format);
  RUN_TEST(test_hex);
  return UNITY_END();
}
