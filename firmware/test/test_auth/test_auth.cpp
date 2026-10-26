// SPDX-License-Identifier: AGPL-3.0-only
// Sign-in: the code shown on the panel, its expiry and attempt limit, the
// cookie that remembers a browser, and the hook token the agents' hooks send.
//
//   pio test -e native
#include <unity.h>

#include "../../src/auth.cpp"

void setUp() { fakeNvs.clear(); authBegin(); }
void tearDown() {}

static String cookieOf(const String &setCookie) {
  const char *s = setCookie.c_str();
  return String(std::string(s, strchr(s, ';') - s));    // "carousel=<secret>"
}

static void test_cookie_signs_in_and_survives_restart() {
  const String c = cookieOf(authSetCookie());
  TEST_ASSERT_TRUE(authCookieValid(c));
  TEST_ASSERT_TRUE(authCookieValid(String("theme=dark; ") + c + "; x=1"));
  authBegin();                                   // restart: same secret from NVS
  TEST_ASSERT_TRUE(authCookieValid(c));
}

static void test_wrong_or_lookalike_cookies_fail() {
  const String c = cookieOf(authSetCookie());
  TEST_ASSERT_FALSE(authCookieValid(""));
  TEST_ASSERT_FALSE(authCookieValid("carousel="));
  TEST_ASSERT_FALSE(authCookieValid("carousel=00000000000000000000000000000000"));
  TEST_ASSERT_FALSE(authCookieValid(String("x") + c));                 // xcarousel=
  TEST_ASSERT_FALSE(authCookieValid(c + "0"));                         // longer
}

static void test_bearer_token() {
  TEST_ASSERT_TRUE(authBearerValid(String("Bearer ") + authToken()));
  // Kept apart: the API token cannot be turned into a browser session, and
  // the session cookie is not an API token
  TEST_ASSERT_FALSE(authCookieValid(String("carousel=") + authToken()));
  const String cookie = cookieOf(authSetCookie());
  TEST_ASSERT_FALSE(authBearerValid(String("Bearer ") + (cookie.c_str() + 9)));
  TEST_ASSERT_FALSE(authBearerValid(authToken()));                     // no scheme
  TEST_ASSERT_FALSE(authBearerValid("Bearer 00000000000000000000000000000000"));
  TEST_ASSERT_FALSE(authBearerValid(""));
}

static void test_sign_out_everywhere() {
  const String c = cookieOf(authSetCookie());
  const String bearer = String("Bearer ") + authToken();
  authSignOutEverywhere();
  TEST_ASSERT_FALSE(authCookieValid(c));
  TEST_ASSERT_FALSE(authBearerValid(bearer));          // the API token changes too
  TEST_ASSERT_TRUE(authCookieValid(cookieOf(authSetCookie())));
}

static void test_code_right_once() {
  TEST_ASSERT_EQUAL_UINT32(0, pairingCodeShown());
  pairingRequest();
  const uint32_t code = pairingCodeShown();
  TEST_ASSERT_TRUE(code >= 100000 && code <= 999999);
  pairingRequest();                              // page reloaded: same code
  TEST_ASSERT_EQUAL_UINT32(code, pairingCodeShown());
  TEST_ASSERT_TRUE(pairingCheck(String((unsigned long)code)));
  TEST_ASSERT_EQUAL_UINT32(0, pairingCodeShown());
  TEST_ASSERT_FALSE(pairingCheck(String((unsigned long)code)));   // used up
}

static void test_code_expires() {
  pairingRequest();
  const uint32_t code = pairingCodeShown();
  fakeClock += PAIRING_MS + 1;
  TEST_ASSERT_EQUAL_UINT32(0, pairingCodeShown());
  TEST_ASSERT_FALSE(pairingCheck(String((unsigned long)code)));
}

static void test_code_dropped_after_wrong_attempts() {
  pairingRequest();
  const uint32_t code = pairingCodeShown();
  const String bad((unsigned long)(code == 999999 ? 100000 : code + 1));
  for (int i = 0; i < PAIRING_ATTEMPTS; i++) TEST_ASSERT_FALSE(pairingCheck(bad));
  TEST_ASSERT_FALSE(pairingCheck(String((unsigned long)code)));   // too late
  TEST_ASSERT_EQUAL_UINT32(0, pairingCodeShown());
  pairingRequest();                              // cool-down: no new code yet
  TEST_ASSERT_EQUAL_UINT32(0, pairingCodeShown());
  fakeClock += PAIRING_COOLDOWN_MS;
  pairingRequest();
  TEST_ASSERT_NOT_EQUAL(0, pairingCodeShown());
}

// --- The hook token ----------------------------------------------------------

static bool isHex32(const char *s) {
  if (strlen(s) != 32) return false;
  for (const char *c = s; *c; c++)
    if (!strchr("0123456789abcdef", *c)) return false;
  return true;
}

// The token with one character changed: right but for one place
static String oneOff(const char *token, size_t at) {
  char c[33];
  snprintf(c, sizeof(c), "%s", token);
  c[at] = c[at] == '0' ? '1' : '0';
  return String(c);
}

static void test_hook_token_made_once_and_kept() {
  TEST_ASSERT_TRUE(isHex32(authHookToken()));
  TEST_ASSERT_EQUAL_STRING(authHookToken(), fakeNvs["hookToken"].c_str());
  const String before(authHookToken());
  authBegin();                                   // restart: the same token from NVS
  TEST_ASSERT_EQUAL_STRING(before.c_str(), authHookToken());
  fakeNvs.clear();                               // factory reset: a new one at the next start
  authBegin();
  TEST_ASSERT_TRUE(isHex32(authHookToken()));
  TEST_ASSERT_NOT_EQUAL(0, strcmp(before.c_str(), authHookToken()));
}

// Three secrets, none opening what another opens
static void test_hook_token_apart_from_session_and_api() {
  const String cookie = cookieOf(authSetCookie());
  const char *session = cookie.c_str() + 9;      // after "carousel="
  TEST_ASSERT_NOT_EQUAL(0, strcmp(authHookToken(), session));
  TEST_ASSERT_NOT_EQUAL(0, strcmp(authHookToken(), authToken()));
  TEST_ASSERT_FALSE(authHookTokenIs(authToken(), strlen(authToken())));
  TEST_ASSERT_FALSE(authHookTokenIs(session, strlen(session)));
  TEST_ASSERT_FALSE(authHookAllowed(authToken()));
  TEST_ASSERT_FALSE(authHookAllowed(String(session)));
  TEST_ASSERT_FALSE(authHookAllowed(String("Bearer ") + authToken()));
  TEST_ASSERT_FALSE(authCookieValid(String("carousel=") + authHookToken()));
  TEST_ASSERT_FALSE(authBearerValid(String("Bearer ") + authHookToken()));
}

// Signing browsers out leaves the computers set up as they are
static void test_sign_out_everywhere_keeps_hook_token() {
  const String before(authHookToken());
  authSignOutEverywhere();
  TEST_ASSERT_EQUAL_STRING(before.c_str(), authHookToken());
  TEST_ASSERT_TRUE(authHookAllowed(before));
}

// NVS refuses the new token: the panel would come back after a restart with
// the old one, so it keeps the old one now, and says it failed
static void test_renew_kept_when_not_stored() {
  const String before(authHookToken());
  for (int k = 0; k < 2; k++) {
    if (k) nvsFailBegin = true; else nvsFailKey = "hookToken";
    TEST_ASSERT_FALSE(authRenewHookToken());
    TEST_ASSERT_EQUAL_STRING(before.c_str(), authHookToken());
    TEST_ASSERT_TRUE(authHookAllowed(before));
    TEST_ASSERT_EQUAL_STRING(before.c_str(), fakeNvs["hookToken"].c_str());
    nvsFailBegin = false; nvsFailKey.clear();
  }
  TEST_ASSERT_TRUE(authRenewHookToken());
  TEST_ASSERT_NOT_EQUAL(0, strcmp(before.c_str(), authHookToken()));
}

static void test_renew_refuses_old_hook_token() {
  const String before(authHookToken());
  const String cookie = cookieOf(authSetCookie());
  const String api(authToken());
  TEST_ASSERT_TRUE(authRenewHookToken());
  TEST_ASSERT_TRUE(isHex32(authHookToken()));
  TEST_ASSERT_NOT_EQUAL(0, strcmp(before.c_str(), authHookToken()));
  TEST_ASSERT_FALSE(authHookAllowed(before));
  TEST_ASSERT_TRUE(authHookAllowed(authHookToken()));
  TEST_ASSERT_EQUAL_STRING(authHookToken(), fakeNvs["hookToken"].c_str());   // stored at once
  TEST_ASSERT_TRUE(authCookieValid(cookie));                // browsers stay signed in
  TEST_ASSERT_EQUAL_STRING(api.c_str(), authToken());       // and the API token stays
}

static void test_hook_allowed_when_closed() {
  const String token(authHookToken());
  TEST_ASSERT_FALSE(authHookOpen());
  TEST_ASSERT_FALSE(authHookAllowed(""));
  TEST_ASSERT_FALSE(authHookAllowed("00000000000000000000000000000000"));
  TEST_ASSERT_FALSE(authHookAllowed(oneOff(token.c_str(), 0)));
  TEST_ASSERT_FALSE(authHookAllowed(oneOff(token.c_str(), 31)));
  TEST_ASSERT_FALSE(authHookAllowed(token.substring(0, 31)));
  TEST_ASSERT_FALSE(authHookAllowed(token + "0"));
  TEST_ASSERT_TRUE(authHookAllowed(token));
}

// Open means open: no token or any token goes on, and nothing counts as refused
static void test_hook_allowed_when_open() {
  authSetHookOpen(true);
  const uint32_t before = authHookRefusals();
  TEST_ASSERT_TRUE(authHookAllowed(""));
  TEST_ASSERT_TRUE(authHookAllowed("00000000000000000000000000000000"));
  TEST_ASSERT_TRUE(authHookAllowed(oneOff(authHookToken(), 5)));
  TEST_ASSERT_TRUE(authHookAllowed(authHookToken()));
  TEST_ASSERT_EQUAL_UINT32(before, authHookRefusals());
}

// The count lives in RAM since start: each no adds one, a yes nothing, and
// nothing reaches NVS
static void test_hook_refusals_counted_once_each() {
  const uint32_t before = authHookRefusals();
  const int writes = nvsWriteCount;
  TEST_ASSERT_FALSE(authHookAllowed(""));
  TEST_ASSERT_FALSE(authHookAllowed("0123"));
  TEST_ASSERT_FALSE(authHookAllowed(oneOff(authHookToken(), 9)));
  TEST_ASSERT_EQUAL_UINT32(before + 3, authHookRefusals());
  TEST_ASSERT_TRUE(authHookAllowed(authHookToken()));
  TEST_ASSERT_EQUAL_UINT32(before + 3, authHookRefusals());
  TEST_ASSERT_EQUAL(writes, nvsWriteCount);
}

// The serial line names the kind, never the value, at most once every 10 s
static void test_hook_refusal_logged_by_kind_without_value() {
  fakeClock += 10000;
  Serial.last[0] = '\0';
  authHookAllowed("");
  TEST_ASSERT_EQUAL_STRING("[HOOK] refused: no token\n", Serial.last);
  Serial.last[0] = '\0';
  fakeClock += 9999;
  authHookAllowed("feedfacefeedfacefeedfacefeedface");      // within 10 s: quiet
  TEST_ASSERT_EQUAL_STRING("", Serial.last);
  fakeClock += 1;
  authHookAllowed("feedfacefeedfacefeedfacefeedface");
  TEST_ASSERT_EQUAL_STRING("[HOOK] refused: wrong token\n", Serial.last);
}

// A wrong install link: the first one logged, then at most once every 10 s,
// apart from the hooks' own refusals
static void test_install_refusal_logged_at_most_every_10_s() {
  Serial.last[0] = '\0';
  authLogInstallRefused();
  TEST_ASSERT_EQUAL_STRING("[HOOK] install refused: wrong link\n", Serial.last);
  Serial.last[0] = '\0';
  fakeClock += 9999;
  authLogInstallRefused();
  TEST_ASSERT_EQUAL_STRING("", Serial.last);
  authHookAllowed("");                       // a hook refusal does not reset it
  Serial.last[0] = '\0';
  fakeClock += 1;
  authLogInstallRefused();
  TEST_ASSERT_EQUAL_STRING("[HOOK] install refused: wrong link\n", Serial.last);
}

// /id?c=<nonce> proves the panel holds the hook token without showing it:
// the first 32 hex characters of HMAC-SHA256(token, nonce ":" ip), ip the
// panel's own address as the request reached it
static void test_hook_proof_vector() {
  fakeNvs["hookToken"] = "0123456789abcdef0123456789abcdef";   // a fake token
  authBegin();
  char proof[33];
  TEST_ASSERT_TRUE(authHookProof("00112233445566778899aabbccddeeff", "192.168.1.50", proof));
  TEST_ASSERT_EQUAL_STRING("9cfb18e582494ddeaac90c454e09c866", proof);
  fakeNvs["hookToken"] = "6666666666666666abcdef0123456789";   // a key byte xor 0x36 is 0
  authBegin();
  TEST_ASSERT_TRUE(authHookProof("abcdef0123456789", "10.0.0.7", proof));   // 16 characters: the shortest
  TEST_ASSERT_EQUAL_STRING("675881da0b160c0b65b1a1764248bd65", proof);
}

static void test_hook_proof_needs_16_to_32_hex() {
  const char *bad[] = {"", "0123456789abcde", "0123456789abcdef0123456789abcdef0",
                       "0123456789abcdeg", "0123456789abcde ", "0123456789\"bcdef", nullptr};
  for (const char *c : bad) {
    char proof[33] = "unchanged";
    TEST_ASSERT_FALSE(authHookProof(c, "192.168.1.50", proof));
    TEST_ASSERT_EQUAL_STRING("", proof);
  }
  char proof[33];
  TEST_ASSERT_TRUE(authHookProof("0123456789ABCDEF0123456789abcdef", "192.168.1.50", proof));   // 32, any case
}

// No IPv4 address of its own (an IPv6 client, none read): no proof
static void test_hook_proof_needs_an_ipv4_address() {
  const char *bad[] = {"", "::1", "192.168.1.500x", "192.168.1.50 ", "1.2.3", "1234.123.123.123",
                       nullptr};
  for (const char *ip : bad) {
    char proof[33] = "unchanged";
    TEST_ASSERT_FALSE(authHookProof("00112233445566778899aabbccddeeff", ip, proof));
    TEST_ASSERT_EQUAL_STRING("", proof);
  }
}

// Another nonce, another address or a new token: another proof
static void test_hook_proof_changes_with_nonce_ip_and_token() {
  char a[33], b[33], c[33], d[33];
  TEST_ASSERT_TRUE(authHookProof("00112233445566778899aabbccddeeff", "192.168.1.50", a));
  TEST_ASSERT_TRUE(authHookProof("00112233445566778899aabbccddeefe", "192.168.1.50", b));
  TEST_ASSERT_TRUE(strcmp(a, b) != 0);
  // A relay at .51 that passes the nonce on gets the panel's proof for .50,
  // which the hook, asking .51, refuses
  TEST_ASSERT_TRUE(authHookProof("00112233445566778899aabbccddeeff", "192.168.1.51", d));
  TEST_ASSERT_TRUE(strcmp(a, d) != 0);
  authRenewHookToken();
  TEST_ASSERT_TRUE(authHookProof("00112233445566778899aabbccddeeff", "192.168.1.50", c));
  TEST_ASSERT_TRUE(strcmp(a, c) != 0);
  TEST_ASSERT_TRUE(strstr(c, authHookToken()) == nullptr);
}

// A proof is an HMAC any host on the LAN can ask for in a loop: at most 4 a
// second overall, one more every 250 ms; beyond, /id answers without one.
// A malformed request costs none.
static void test_hook_proof_at_most_4_a_second() {
  const char *n = "00112233445566778899aabbccddeeff";
  char proof[33];
  for (int i = 0; i < 10; i++) authHookProof("short", "192.168.1.50", proof);
  for (int i = 0; i < 4; i++) TEST_ASSERT_TRUE(authHookProof(n, "192.168.1.50", proof));
  strcpy(proof, "unchanged");
  TEST_ASSERT_FALSE(authHookProof(n, "192.168.1.50", proof));
  TEST_ASSERT_EQUAL_STRING("", proof);
  fakeClock += 249;
  TEST_ASSERT_FALSE(authHookProof(n, "192.168.1.50", proof));
  fakeClock += 1;
  TEST_ASSERT_TRUE(authHookProof(n, "192.168.1.50", proof));
  TEST_ASSERT_FALSE(authHookProof(n, "192.168.1.50", proof));
  fakeClock += 60000;                            // a long pause refills 4, never more
  for (int i = 0; i < 4; i++) TEST_ASSERT_TRUE(authHookProof(n, "192.168.1.50", proof));
  TEST_ASSERT_FALSE(authHookProof(n, "192.168.1.50", proof));
}

static void test_hook_open_absent_reads_off_and_is_stored() {
  TEST_ASSERT_TRUE(fakeNvs.find("hookOpen") == fakeNvs.end());
  TEST_ASSERT_FALSE(authHookOpen());
  TEST_ASSERT_TRUE(fakeNvs.find("hookOpen") == fakeNvs.end());   // reading writes nothing
  authSetHookOpen(true);
  TEST_ASSERT_TRUE(authHookOpen());
  TEST_ASSERT_EQUAL_STRING("1", fakeNvs["hookOpen"].c_str());
  authBegin();                                   // restart: kept
  TEST_ASSERT_TRUE(authHookOpen());
  authSetHookOpen(false);
  authBegin();
  TEST_ASSERT_FALSE(authHookOpen());
  TEST_ASSERT_FALSE(authHookAllowed(""));
}

// The install link's k: the token itself, by length and content, whatever
// the setting, and counting no refusal
static void test_hook_token_is_for_install_link() {
  const String t(authHookToken());
  TEST_ASSERT_TRUE(authHookTokenIs(t.c_str(), 32));
  TEST_ASSERT_FALSE(authHookTokenIs(t.c_str(), 31));
  const String longer = t + "0";
  TEST_ASSERT_FALSE(authHookTokenIs(longer.c_str(), 33));
  TEST_ASSERT_FALSE(authHookTokenIs("", 0));
  TEST_ASSERT_FALSE(authHookTokenIs(oneOff(t.c_str(), 16).c_str(), 32));
  const uint32_t before = authHookRefusals();
  authSetHookOpen(true);
  TEST_ASSERT_FALSE(authHookTokenIs("00000000000000000000000000000000", 32));
  TEST_ASSERT_TRUE(authHookTokenIs(t.c_str(), 32));
  TEST_ASSERT_EQUAL_UINT32(before, authHookRefusals());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_cookie_signs_in_and_survives_restart);
  RUN_TEST(test_wrong_or_lookalike_cookies_fail);
  RUN_TEST(test_bearer_token);
  RUN_TEST(test_sign_out_everywhere);
  RUN_TEST(test_code_right_once);
  RUN_TEST(test_code_expires);
  RUN_TEST(test_code_dropped_after_wrong_attempts);
  RUN_TEST(test_hook_token_made_once_and_kept);
  RUN_TEST(test_hook_token_apart_from_session_and_api);
  RUN_TEST(test_sign_out_everywhere_keeps_hook_token);
  RUN_TEST(test_renew_refuses_old_hook_token);
  RUN_TEST(test_renew_kept_when_not_stored);
  RUN_TEST(test_hook_allowed_when_closed);
  RUN_TEST(test_hook_allowed_when_open);
  RUN_TEST(test_hook_refusals_counted_once_each);
  RUN_TEST(test_hook_refusal_logged_by_kind_without_value);
  RUN_TEST(test_install_refusal_logged_at_most_every_10_s);
  RUN_TEST(test_hook_proof_vector);
  RUN_TEST(test_hook_proof_needs_16_to_32_hex);
  RUN_TEST(test_hook_proof_needs_an_ipv4_address);
  RUN_TEST(test_hook_proof_changes_with_nonce_ip_and_token);
  RUN_TEST(test_hook_proof_at_most_4_a_second);
  RUN_TEST(test_hook_open_absent_reads_off_and_is_stored);
  RUN_TEST(test_hook_token_is_for_install_link);
  return UNITY_END();
}
