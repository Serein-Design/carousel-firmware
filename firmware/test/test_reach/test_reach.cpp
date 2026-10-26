// SPDX-License-Identifier: AGPL-3.0-only
// Reaching the pages: a long press tells itself from the brightness tap and
// from the boot-time hold, the reach card keeps its 20 s and its two pages,
// the setup screen turns its three, the QR codes carry what a phone needs,
// and every line of both fits its face in both languages.
//
//   pio test -e native -f test_reach
#include <unity.h>

#include <cstdio>
#include <cstring>

#include "../../src/fonts_data.cpp"
#include "../../src/type.cpp"
#include "../../src/i18n.cpp"
#include "../../src/reach_logic.cpp"

void setUp() {}
void tearDown() {}

static const char HEX_DIGITS[] = "0123456789ABCDEF";

// A press from t0 held for ms: what the release says
static Press pressFor(PressTimer &p, uint32_t t0, uint32_t ms) {
  TEST_ASSERT_EQUAL(Press::NONE, p.update(true, t0));
  TEST_ASSERT_EQUAL(Press::NONE, p.update(true, t0 + ms / 2));   // nothing while held
  return p.update(false, t0 + ms);
}

static void test_short_and_long_press() {
  PressTimer p;
  TEST_ASSERT_EQUAL(Press::NONE, p.update(false, 0));
  TEST_ASSERT_EQUAL(Press::SHORT, pressFor(p, 1000, 150));
  TEST_ASSERT_EQUAL(Press::SHORT, pressFor(p, 2000, LONG_PRESS_MS - 1));
  TEST_ASSERT_EQUAL(Press::LONG, pressFor(p, 5000, LONG_PRESS_MS));
  TEST_ASSERT_EQUAL(Press::LONG, pressFor(p, 9000, 2500));
  TEST_ASSERT_EQUAL(Press::NONE, p.update(false, 12000));   // told once
}

// The knob held through the boot (setup, reset) was handled before the
// loop: its release is not a press
static void test_hold_from_boot_is_not_a_press() {
  PressTimer p;
  TEST_ASSERT_EQUAL(Press::NONE, p.update(true, 0));
  TEST_ASSERT_EQUAL(Press::NONE, p.update(true, 2000));
  TEST_ASSERT_EQUAL(Press::NONE, p.update(false, 2100));
  TEST_ASSERT_EQUAL(Press::SHORT, pressFor(p, 3000, 100));   // the next one counts
}

static void test_press_across_the_millis_wrap() {
  PressTimer p;
  p.update(false, 0xFFFFFF00UL);
  TEST_ASSERT_EQUAL(Press::LONG, pressFor(p, 0xFFFFFF00UL, 1000));
}

static void test_reach_card_time_and_pages() {
  ReachCard c;
  TEST_ASSERT_FALSE(c.shown(0));
  c.show(1000);
  TEST_ASSERT_TRUE(c.shown(1000));
  TEST_ASSERT_EQUAL_UINT8(0, c.page(1000));                       // the address first
  TEST_ASSERT_EQUAL_UINT8(0, c.page(1000 + REACH_PAGE_MS - 1));
  TEST_ASSERT_EQUAL_UINT8(1, c.page(1000 + REACH_PAGE_MS));       // then the QR
  TEST_ASSERT_EQUAL_UINT8(0, c.page(1000 + 2 * REACH_PAGE_MS));
  TEST_ASSERT_TRUE(c.shown(1000 + REACH_SHOW_MS - 1));
  TEST_ASSERT_FALSE(c.shown(1000 + REACH_SHOW_MS));
  TEST_ASSERT_FALSE(c.shown(1000 + REACH_SHOW_MS - 1));           // gone stays gone
  c.show(40000);
  c.hide();                                                       // the knob moved
  TEST_ASSERT_FALSE(c.shown(40001));
}

static void test_setup_pages_take_turns() {
  TEST_ASSERT_EQUAL_UINT8(0, setupPage(0));
  TEST_ASSERT_EQUAL_UINT8(0, setupPage(REACH_PAGE_MS - 1));
  TEST_ASSERT_EQUAL_UINT8(1, setupPage(REACH_PAGE_MS));
  TEST_ASSERT_EQUAL_UINT8(2, setupPage(2 * REACH_PAGE_MS));
  TEST_ASSERT_EQUAL_UINT8(0, setupPage(3 * REACH_PAGE_MS));
}

static void test_qr_texts() {
  char t[48];
  TEST_ASSERT_TRUE(apQrText(t, sizeof(t), "CAROUSEL-695C"));
  TEST_ASSERT_EQUAL_STRING("WIFI:S:CAROUSEL-695C;T:nopass;;", t);
  TEST_ASSERT_LESS_OR_EQUAL_size_t(32, strlen(t));                // a version 2 code
  const uint8_t widest[4] = {255, 255, 255, 255};
  TEST_ASSERT_TRUE(pageQrText(t, sizeof(t), widest));
  TEST_ASSERT_EQUAL_STRING("http://255.255.255.255/", t);
  TEST_ASSERT_LESS_OR_EQUAL_size_t(32, strlen(t));
  const uint8_t lan[4] = {192, 168, 1, 42};
  TEST_ASSERT_TRUE(pageQrText(t, sizeof(t), lan));
  TEST_ASSERT_EQUAL_STRING("http://192.168.1.42/", t);
  char small[12];
  TEST_ASSERT_FALSE(apQrText(small, sizeof(small), "CAROUSEL-695C"));
  TEST_ASSERT_FALSE(pageQrText(small, sizeof(small), lan));
}

static void test_address_lines() {
  char a[32], b[32];
  addressLines("carousel.local", 60, a, b, sizeof(a));
  TEST_ASSERT_EQUAL_STRING("carousel.local", a);
  TEST_ASSERT_EQUAL_STRING("", b);
  addressLines("carousel-695c.local", 60, a, b, sizeof(a));   // the panel's own name
  TEST_ASSERT_EQUAL_STRING("carousel-695c", a);
  TEST_ASSERT_EQUAL_STRING(".local", b);
  // Every unique name fits its two lines, whatever its last 4 hex digits
  for (const char *h = HEX_DIGITS; *h; h++) {
    char host[24];
    snprintf(host, sizeof(host), "carousel-%c%c%c%c.local", *h, *h, *h, *h);
    for (int i = 0; host[i]; i++) if (host[i] >= 'A' && host[i] <= 'F') host[i] += 32;
    addressLines(host, 60, a, b, sizeof(a));
    TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(60, textWidth(a, Face::LABEL), host);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(60, textWidth(b, Face::LABEL), host);
  }
}

static void test_ip_and_name_fit_portrait() {
  char a[16], b[16];
  const uint8_t widest[4] = {255, 255, 255, 255};
  ipLines(widest, a, b, sizeof(a));
  TEST_ASSERT_EQUAL_STRING("255.255.", a);
  TEST_ASSERT_EQUAL_STRING("255.255", b);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(28, textWidth(a, Face::LABEL));
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(28, textWidth(b, Face::LABEL));
  char tall[3][8];
  apNameTall("CAROUSEL-695C", tall);
  TEST_ASSERT_EQUAL_STRING("CARO-", tall[0]);
  TEST_ASSERT_EQUAL_STRING("USEL-", tall[1]);
  TEST_ASSERT_EQUAL_STRING("695C", tall[2]);
  for (const char *h = HEX_DIGITS; *h; h++) {
    char ap[16];
    snprintf(ap, sizeof(ap), "CAROUSEL-%c%c%c%c", *h, *h, *h, *h);
    apNameTall(ap, tall);
    for (auto &line : tall) TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(28, textWidth(line, Face::LABEL), ap);
    // and across, on the setup screen's first page
    TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(60, textWidth(ap, Face::LABEL), ap);
  }
  // The access point's own address, on the second page
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(60, textWidth("192.168.4.1", Face::LABEL));
}

static void fits(const char *s, Face f, uint16_t px, const char *lang, const char *field) {
  char msg[96];
  snprintf(msg, sizeof(msg), "%s.%s = \"%s\"", lang, field, s);
  TEST_ASSERT_NOT_NULL_MESSAGE(s, msg);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(px, textWidth(s, f), msg);
}

static void test_strings_fit_their_faces() {
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    fits(t.saved, Face::BODY, 60, t.code, "saved");
    fits(t.setupJoin, Face::BODY, 60, t.code, "setupJoin");
    fits(t.setupOpen, Face::BODY, 60, t.code, "setupOpen");
    fits(t.setupLabel, Face::LABEL, 60, t.code, "setupLabel");
    fits(t.phoneConnected, Face::LABEL, 60, t.code, "phoneConnected");
    fits(t.wrongPassword, Face::LABEL, 60, t.code, "wrongPassword");
    fits(t.reachLabel, Face::LABEL, 60, t.code, "reachLabel");
    fits(t.setupTall, Face::LABEL, 28, t.code, "setupTall");
    for (const char *s : t.setupScan) fits(s, Face::LABEL, 30, t.code, "setupScan");
    for (const char *s : t.reachScan) fits(s, Face::LABEL, 30, t.code, "reachScan");
  }
  // What the pages say, as the board gives them
  TEST_ASSERT_EQUAL_STRING("Join Wi-Fi", STRINGS[LANG_EN].setupJoin);
  TEST_ASSERT_EQUAL_STRING("Rejoignez", STRINGS[LANG_FR].setupJoin);
  TEST_ASSERT_EQUAL_STRING("OPEN THE PAGES", STRINGS[LANG_EN].reachLabel);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_short_and_long_press);
  RUN_TEST(test_hold_from_boot_is_not_a_press);
  RUN_TEST(test_press_across_the_millis_wrap);
  RUN_TEST(test_reach_card_time_and_pages);
  RUN_TEST(test_setup_pages_take_turns);
  RUN_TEST(test_qr_texts);
  RUN_TEST(test_address_lines);
  RUN_TEST(test_ip_and_name_fit_portrait);
  RUN_TEST(test_strings_fit_their_faces);
  return UNITY_END();
}
