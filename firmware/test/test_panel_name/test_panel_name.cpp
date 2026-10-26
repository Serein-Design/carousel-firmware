// SPDX-License-Identifier: AGPL-3.0-only
// Every panel answers at its own name, carousel-xxxx.local (the end of its
// MAC, as its access point), so two panels on one network stay apart; the
// name shows in /id, /api/info and the pages, and is the only name besides
// the IP and carousel.local whose requests are served.
//
//   pio test -e native -f test_panel_name
#include <unity.h>

#include <cstring>

#include "../../src/panel_name.cpp"

void setUp() {}
void tearDown() {}

static const uint8_t MAC[6] = {0x24, 0x6F, 0x28, 0xA1, 0x69, 0x5C};

static void test_before_the_mac_the_shared_name() {
  TEST_ASSERT_EQUAL_STRING("carousel", panelMdnsName());
  TEST_ASSERT_EQUAL_STRING("carousel.local", panelHostName());
}

static void test_the_name_from_the_mac_lowercase() {
  panelNameFromMac(MAC);
  TEST_ASSERT_EQUAL_STRING("carousel-695c", panelMdnsName());
  TEST_ASSERT_EQUAL_STRING("carousel-695c.local", panelHostName());
}

static void test_leading_zeros_kept() {
  const uint8_t mac[6] = {0, 0, 0, 0, 0x0A, 0x01};
  panelNameFromMac(mac);
  TEST_ASSERT_EQUAL_STRING("carousel-0a01.local", panelHostName());
  panelNameFromMac(MAC);
}

// DNS rebinding: only the panel's own names, in any case
static void test_hosts_served() {
  panelNameFromMac(MAC);
  TEST_ASSERT_TRUE(panelNameIsOurs("carousel.local"));
  TEST_ASSERT_TRUE(panelNameIsOurs("carousel-695c.local"));
  TEST_ASSERT_TRUE(panelNameIsOurs("CAROUSEL-695C.local"));
  TEST_ASSERT_FALSE(panelNameIsOurs("carousel-695d.local"));
  TEST_ASSERT_FALSE(panelNameIsOurs("carousel-695c"));
  TEST_ASSERT_FALSE(panelNameIsOurs("carousel-695c.local.evil.com"));
  TEST_ASSERT_FALSE(panelNameIsOurs(""));
}

static void test_id_fields() {
  panelNameFromMac(MAC);
  char out[ID_JSON_SIZE];
  const int n = panelIdJson(out, sizeof(out), "v0.2.0-5-gabc1234", "24:6F:28:A1:69:5C",
                            "power-on", 86400UL * 49);
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"device\":\"carousel\",\"name\":\"carousel-695c\",\"version\":\"v0.2.0-5-gabc1234\","
      "\"mac\":\"24:6F:28:A1:69:5C\",\"reset\":\"power-on\",\"uptime\":4233600}",
      out);
}

// Asked with a nonce, /id adds the hook token's proof last; without, the
// answer stays as machines already read it
static void test_id_with_proof() {
  panelNameFromMac(MAC);
  char out[ID_JSON_SIZE];
  const int n = panelIdJson(out, sizeof(out), "v0.2.0", "24:6F:28:A1:69:5C", "power-on", 7,
                            "9cfb18e582494ddeaac90c454e09c866");
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"device\":\"carousel\",\"name\":\"carousel-695c\",\"version\":\"v0.2.0\","
      "\"mac\":\"24:6F:28:A1:69:5C\",\"reset\":\"power-on\",\"uptime\":7,"
      "\"proof\":\"9cfb18e582494ddeaac90c454e09c866\"}",
      out);
}

// /api/info opens with the same identity, the screens follow
static void test_info_head() {
  panelNameFromMac(MAC);
  char out[INFO_HEAD_SIZE];
  const int n = panelInfoHead(out, sizeof(out), "v0.2.0");
  TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"device\":\"carousel\",\"name\":\"carousel-695c\",\"version\":\"v0.2.0\"", out);
}

// The longest version git describe gives still fits
static void test_long_version_fits() {
  char out[INFO_HEAD_SIZE];
  const char *v = "v10.20.30-rc.1-1234-gabcdef0-dirty";
  TEST_ASSERT_TRUE(panelInfoHead(out, sizeof(out), v) < (int)sizeof(out));
  char id[ID_JSON_SIZE];
  TEST_ASSERT_TRUE(panelIdJson(id, sizeof(id), v, "24:6F:28:A1:69:5C", "brownout",
                               4294967295UL) < (int)sizeof(id));
  TEST_ASSERT_TRUE(panelIdJson(id, sizeof(id), v, "24:6F:28:A1:69:5C", "brownout", 4294967295UL,
                               "9cfb18e582494ddeaac90c454e09c866") < (int)sizeof(id));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_before_the_mac_the_shared_name);
  RUN_TEST(test_the_name_from_the_mac_lowercase);
  RUN_TEST(test_leading_zeros_kept);
  RUN_TEST(test_hosts_served);
  RUN_TEST(test_id_fields);
  RUN_TEST(test_id_with_proof);
  RUN_TEST(test_info_head);
  RUN_TEST(test_long_version_fits);
  return UNITY_END();
}
