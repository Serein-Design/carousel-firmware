// SPDX-License-Identifier: AGPL-3.0-only
// The browser routes' gate, body and values: who may post to /show,
// /brightness, /text and /wifi, how much is read, and what each value may
// be. The body parser reads what any device on the network sends: it is
// also run on seeded random and mutated bodies, under AddressSanitizer.
//
//   pio test -e native -f test_web_forms
#include <unity.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <random>
#include <string>

#include "../../src/i18n.cpp"
#include "../../src/screen_list.cpp"
#include "../../src/web_shell.cpp"
#include "../../src/posters.cpp"
#include "../../src/web_stage.cpp"
#include "../../src/web_forms.cpp"

void setUp() {}
void tearDown() {}

static FormBody body(const std::string &b) {
  FormBody f;
  TEST_ASSERT_TRUE_MESSAGE(f.set(b.data(), b.size()), b.c_str());
  return f;
}

static std::string value(const FormBody &f, const char *name, size_t size = 600) {
  char out[600];
  const int n = f.value(name, out, size);
  if (n < 0) return "<absent>";
  return out;
}

// --- The gate --------------------------------------------------------------

static void test_gate_order() {
  static const int32_t SIZES[] = {512, 513, -1};
  for (int bits = 0; bits < 16; bits++) {
    for (const int32_t size : SIZES) {
      FormInput in;
      in.hostOk = bits & 1;
      in.crossSite = bits & 2;
      in.cookieOk = bits & 4;
      in.locked = bits & 8;
      in.announced = size;
      const FormVerdict want = !in.hostOk || in.crossSite ? FORM_FORBIDDEN
                             : !in.cookieOk               ? FORM_SIGN_IN
                             : in.announced < 0           ? FORM_BAD_LENGTH
                             : in.announced > 512         ? FORM_TOO_LARGE
                             : in.locked                  ? FORM_LOCKED
                                                          : FORM_ACCEPT;
      char msg[32];
      snprintf(msg, sizeof(msg), "bits %d, size %d", bits, (int)size);
      TEST_ASSERT_EQUAL_MESSAGE(want, formGate(in), msg);
    }
  }
}

// The gate is given the session cookie's verdict only: a request carrying
// the API token and no cookie is signed out here, whatever the token
static void test_bearer_is_not_a_session() {
  const FormInput bearerOnly = {true, false, false, false, 10};
  TEST_ASSERT_EQUAL(FORM_SIGN_IN, formGate(bearerOnly));
}

static void test_gate_too_large() {
  FormInput in = {true, false, true, false, 512};
  TEST_ASSERT_EQUAL(FORM_ACCEPT, formGate(in));
  in.announced = 513;
  TEST_ASSERT_EQUAL(FORM_TOO_LARGE, formGate(in));
  in.announced = INT32_MAX;
  TEST_ASSERT_EQUAL(FORM_TOO_LARGE, formGate(in));
  in.announced = 0;
  TEST_ASSERT_EQUAL(FORM_ACCEPT, formGate(in));
}

// The library stores Content-Length as a signed int from the header's text:
// "Content-Length: -1" arrives as -1. Read as a size it would be the largest
// there is, and a buffer of size + 1 would wrap to nothing; refused instead,
// before the lock, like an oversized body
static void test_gate_negative_length() {
  FormInput in = {true, false, true, false, -1};
  TEST_ASSERT_EQUAL(FORM_BAD_LENGTH, formGate(in));
  in.announced = INT32_MIN;
  TEST_ASSERT_EQUAL(FORM_BAD_LENGTH, formGate(in));
  in.locked = true;
  TEST_ASSERT_EQUAL(FORM_BAD_LENGTH, formGate(in));
  in.cookieOk = false;
  TEST_ASSERT_EQUAL(FORM_SIGN_IN, formGate(in));
  in.cookieOk = true;
  in.crossSite = true;
  TEST_ASSERT_EQUAL(FORM_FORBIDDEN, formGate(in));
}

// Locked: the owner still learns the body was too large; a body that fits
// gets the locked page
static void test_gate_locked_after_size() {
  FormInput in = {true, false, true, true, 513};
  TEST_ASSERT_EQUAL(FORM_TOO_LARGE, formGate(in));
  in.announced = 20;
  TEST_ASSERT_EQUAL(FORM_LOCKED, formGate(in));
  in.cookieOk = false;
  TEST_ASSERT_EQUAL(FORM_SIGN_IN, formGate(in));
}

// --- The body --------------------------------------------------------------

static void test_body_cap() {
  std::string b = "msg=" + std::string(508, 'a');   // 512
  FormBody f;
  TEST_ASSERT_TRUE(f.set(b.data(), b.size()));
  TEST_ASSERT_EQUAL(508, f.value("msg", nullptr, 0));
  b += 'a';                                         // 513
  TEST_ASSERT_FALSE(f.set(b.data(), b.size()));
  TEST_ASSERT_FALSE(f.has("msg"));                  // refused: empty, not the last body
  TEST_ASSERT_TRUE(f.set("", 0));
  TEST_ASSERT_FALSE(f.has("msg"));
  // A NUL ends the body there
  TEST_ASSERT_TRUE(f.set("b=3\0&s=CLOCK", 12));
  TEST_ASSERT_TRUE(f.has("b"));
  TEST_ASSERT_FALSE(f.has("s"));
}

static void test_body_decodes() {
  const FormBody f = body("a=one+two&b=1%2B1&c=caf%C3%A9&d=%zz&e=50%&f=&g&h=%4&i=%00x&%6A=k");
  TEST_ASSERT_EQUAL_STRING("one two", value(f, "a").c_str());
  TEST_ASSERT_EQUAL_STRING("1+1", value(f, "b").c_str());
  TEST_ASSERT_EQUAL_STRING("caf\xC3\xA9", value(f, "c").c_str());
  TEST_ASSERT_EQUAL_STRING("%zz", value(f, "d").c_str());
  TEST_ASSERT_EQUAL_STRING("50%", value(f, "e").c_str());
  TEST_ASSERT_EQUAL_STRING("", value(f, "f").c_str());
  TEST_ASSERT_TRUE(f.has("f"));
  TEST_ASSERT_EQUAL_STRING("", value(f, "g").c_str());   // no '=': present, empty
  TEST_ASSERT_TRUE(f.has("g"));
  TEST_ASSERT_EQUAL_STRING("%4", value(f, "h").c_str());
  TEST_ASSERT_EQUAL_STRING("%00x", value(f, "i").c_str());   // never a NUL inside
  TEST_ASSERT_EQUAL_STRING("k", value(f, "j").c_str());      // names are decoded too
  TEST_ASSERT_EQUAL_STRING("<absent>", value(f, "z").c_str());
  TEST_ASSERT_FALSE(f.has("z"));
  TEST_ASSERT_FALSE(f.has(""));
  TEST_ASSERT_FALSE(f.has("on"));                            // whole names only
  char out[4] = "xyz";
  TEST_ASSERT_EQUAL(-1, f.value("z", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);
}

static void test_body_first_value_wins() {
  const FormBody f = body("s=CLOCK&s=SLEEP&ss=X&b=1&b=5");
  TEST_ASSERT_EQUAL_STRING("CLOCK", value(f, "s").c_str());
  TEST_ASSERT_EQUAL_STRING("X", value(f, "ss").c_str());
  TEST_ASSERT_EQUAL_STRING("1", value(f, "b").c_str());
}

static void test_body_value_reports_full_length() {
  const FormBody f = body("ssid=" + std::string(40, 'n'));
  char out[33];
  TEST_ASSERT_EQUAL(40, f.value("ssid", out, sizeof(out)));
  TEST_ASSERT_EQUAL(32, strlen(out));
  // Decoded length, not encoded: %C3%A9 is two bytes
  const FormBody g = body("m=%C3%A9%C3%A9");
  char two[2];
  TEST_ASSERT_EQUAL(4, g.value("m", two, sizeof(two)));
  TEST_ASSERT_EQUAL_STRING("\xC3", two);
  TEST_ASSERT_EQUAL(4, g.value("m", nullptr, 0));
}

// Whatever the bytes, value() writes inside out, ends it, and reports a
// length no longer than the body; set() refuses past the cap
static void checkBody(const std::string &b) {
  FormBody f;
  const bool ok = f.set(b.data(), b.size());
  TEST_ASSERT_EQUAL(b.size() <= FORM_BODY_MAX, ok);
  static const char *const NAMES[] = {"s", "b", "msg", "ssid", "mdp", "", "%", "a=b"};
  static const size_t SIZES[] = {0, 1, 2, 24, 96, 513};
  for (const char *name : NAMES) {
    for (size_t size : SIZES) {
      char *out = size ? new char[size] : nullptr;
      if (out) memset(out, 'X', size);
      const int n = f.value(name, out, size);
      TEST_ASSERT_TRUE(n >= -1 && n <= (int)b.size());
      if (out) {
        const size_t kept = strnlen(out, size);
        TEST_ASSERT_TRUE(kept < size);
        TEST_ASSERT_EQUAL(n < 0 ? 0 : (size_t)n < size ? (size_t)n : size - 1, kept);
      }
      delete[] out;
    }
    TEST_ASSERT_EQUAL(f.value(name, nullptr, 0) >= 0, f.has(name));
  }
}

static void test_body_survives_mutations() {
  std::mt19937 rng(20261018);
  static const char ALPHABET[] = "=&%+abcsmgd0123456789ABCDEF\xC3\xA9\x00 ";
  for (int i = 0; i < 1000; i++) {
    std::string b(rng() % 600, '\0');
    for (char &c : b)
      c = rng() % 4 ? ALPHABET[rng() % (sizeof(ALPHABET) - 1)] : (char)(rng() & 0xFF);
    checkBody(b);
  }
  static const char *const REAL[] = {
    "s=CLOCK", "b=3", "msg=Caf%C3%A9+%C3%A0+14h+%2A",
    "ssid=Home&other=&mdp=secret%26123&tz=Europe%2FParis",
    "annuler=1", "msg=",
  };
  for (int i = 0; i < 1000; i++) {
    std::string b = REAL[rng() % (sizeof(REAL) / sizeof(REAL[0]))];
    for (int m = 1 + rng() % 8; m > 0; m--) {
      const size_t at = b.empty() ? 0 : rng() % b.size();
      switch (rng() % 4) {
        case 0: if (!b.empty()) b[at] = (char)(rng() & 0xFF); break;
        case 1: b.insert(at, 1, ALPHABET[rng() % (sizeof(ALPHABET) - 1)]); break;
        case 2: if (!b.empty()) b.erase(at, 1); break;
        default: b += b.substr(0, rng() % 300); break;
      }
    }
    checkBody(b);
  }
}

// --- The values ------------------------------------------------------------

static void test_level_is_strict() {
  int level = -7;
  TEST_ASSERT_TRUE(formLevel("3", 6, &level));
  TEST_ASSERT_EQUAL(3, level);
  TEST_ASSERT_TRUE(formLevel("0", 6, &level));
  TEST_ASSERT_EQUAL(0, level);
  TEST_ASSERT_TRUE(formLevel("5", 6, &level));
  TEST_ASSERT_EQUAL(5, level);
  for (const char *bad : {"6", "-1", "03", "3x", " 3", "3 ", "", "+3", "99999999999", "x"}) {
    level = 42;
    TEST_ASSERT_FALSE_MESSAGE(formLevel(bad, 6, &level), bad);
    TEST_ASSERT_EQUAL_MESSAGE(42, level, bad);   // untouched
  }
  TEST_ASSERT_FALSE(formLevel(nullptr, 6, &level));
  TEST_ASSERT_TRUE(formLevel("11", 12, &level));
  TEST_ASSERT_EQUAL(11, level);
}

static void test_panel_message_like_the_field() {
  char out[96];
  panelMessage("Caf\xC3\xA9 \xC3\xA0 14h", out, sizeof(out));   // French letters stay: the body face draws them
  TEST_ASSERT_EQUAL_STRING("Caf\xC3\xA9 \xC3\xA0 14h * ", out);
  panelMessage("a\xC3\xB1\xC3\xA1", out, sizeof(out));           // letters it lacks are folded
  TEST_ASSERT_EQUAL_STRING("ana * ", out);
  panelMessage("HELLO * CAROUSEL * ", out, sizeof(out));   // the stored one, sent back
  TEST_ASSERT_EQUAL_STRING("HELLO * CAROUSEL * ", out);
  panelMessage("", out, sizeof(out));                       // an empty message stays empty
  TEST_ASSERT_EQUAL_STRING("", out);
  const std::string full(95, 'a');
  panelMessage(full.c_str(), out, sizeof(out));             // 95 characters kept
  TEST_ASSERT_EQUAL_STRING(full.c_str(), out);
  panelMessage(std::string(93, 'b').c_str(), out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING((std::string(93, 'b') + " *").c_str(), out);
  panelMessage(std::string(200, 'c').c_str(), out, sizeof(out));
  TEST_ASSERT_EQUAL(95, strlen(out));
  char tiny[1] = {'x'};
  panelMessage("abc", tiny, sizeof(tiny));
  TEST_ASSERT_EQUAL_STRING("", tiny);
}

// /show reads at most 24 bytes of the name: anything longer than the
// longest name (23) is unknown, never cut into a real one
static void test_show_name_bounded() {
  const ScreenList l = screenListDefault();
  char name[25];
  const FormBody f = body("s=CLOCK" + std::string(30, 'K'));
  TEST_ASSERT_EQUAL(35, f.value("s", name, sizeof(name)));
  TEST_ASSERT_EQUAL(SHOW_UNKNOWN, showOutcome(l, name, false));
  TEST_ASSERT_EQUAL(SHOW_UNKNOWN, showOutcome(l, ("CLOCK" + std::string(19, ' ')).c_str(), false));
  const FormBody g = body("s=clock");
  g.value("s", name, sizeof(name));
  TEST_ASSERT_EQUAL(SHOW_OK, showOutcome(l, name, false));
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) TEST_ASSERT_TRUE(strlen(screenNameEn(i)) <= 23);
}

// The known network chosen with no password typed: nothing is saved, the
// panel restarts and retries it with the stored password (the page's
// Continue on the known row, which used to be "Keep the current network and
// retry"). A password typed for it, or another name, is a save.
static void test_wifi_known_network_retries() {
  TEST_ASSERT_TRUE(wifiRetrySaved("Maison-2.4G", -1, "Maison-2.4G"));
  TEST_ASSERT_TRUE(wifiRetrySaved("Maison-2.4G", 0, "Maison-2.4G"));
  TEST_ASSERT_FALSE(wifiRetrySaved("Maison-2.4G", 10, "Maison-2.4G"));   // a new password
  TEST_ASSERT_FALSE(wifiRetrySaved("Livebox-7F2A", -1, "Maison-2.4G"));   // an open network
  TEST_ASSERT_FALSE(wifiRetrySaved("maison-2.4g", -1, "Maison-2.4G"));    // names are exact
  TEST_ASSERT_FALSE(wifiRetrySaved("Maison-2.4G", -1, ""));               // nothing saved
  TEST_ASSERT_FALSE(wifiRetrySaved("", -1, ""));
  TEST_ASSERT_FALSE(wifiRetrySaved("Maison-2.4G", -1, nullptr));
  TEST_ASSERT_FALSE(wifiRetrySaved(nullptr, -1, "Maison-2.4G"));
}

static void test_wifi_values_bounded() {
  TEST_ASSERT_EQUAL(WIFI_VALUES_OK, wifiValues(1, -1));
  TEST_ASSERT_EQUAL(WIFI_VALUES_OK, wifiValues(32, 0));
  TEST_ASSERT_EQUAL(WIFI_VALUES_OK, wifiValues(8, 8));
  TEST_ASSERT_EQUAL(WIFI_VALUES_OK, wifiValues(8, 63));
  TEST_ASSERT_EQUAL(WIFI_BAD_SSID, wifiValues(33, 10));
  TEST_ASSERT_EQUAL(WIFI_BAD_SSID, wifiValues(0, 10));
  TEST_ASSERT_EQUAL(WIFI_BAD_SSID, wifiValues(-1, 10));
  TEST_ASSERT_EQUAL(WIFI_BAD_PASSWORD, wifiValues(8, 7));
  TEST_ASSERT_EQUAL(WIFI_BAD_PASSWORD, wifiValues(8, 64));
  TEST_ASSERT_EQUAL(WIFI_BAD_SSID, wifiValues(33, 64));   // the name is told first
  // As the form reads them: a 33-byte name into its 33-byte field still
  // reports 33
  const FormBody f = body("ssid=" + std::string(33, 's') + "&mdp=" + std::string(64, 'p'));
  char ssid[33], pass[64];
  TEST_ASSERT_EQUAL(WIFI_BAD_SSID, wifiValues(f.value("ssid", ssid, sizeof(ssid)),
                                              f.value("mdp", pass, sizeof(pass))));
  const FormBody g = body("ssid=Home&mdp=" + std::string(64, 'p'));
  TEST_ASSERT_EQUAL(WIFI_BAD_PASSWORD, wifiValues(g.value("ssid", ssid, sizeof(ssid)),
                                                  g.value("mdp", pass, sizeof(pass))));
}

// The wipe the compiler must keep: a plain memset before free or the end of
// a lifetime is a dead store GCC may drop, leaving a password in the heap
static void test_wipe_clears_every_byte() {
  char secret[16];
  memset(secret, 'x', sizeof(secret));
  wipeSecret(secret, sizeof(secret));
  for (size_t i = 0; i < sizeof(secret); i++) TEST_ASSERT_EQUAL_INT(0, secret[i]);
  wipeSecret(nullptr, 0);   // nothing to wipe: no crash
}

// A FormBody's storage holds no byte of its body once it is destroyed. The
// text is its first member; the length after it is no secret.
static void test_body_wiped_when_destroyed() {
  alignas(FormBody) unsigned char storage[sizeof(FormBody)];
  FormBody *body = new (storage) FormBody;
  TEST_ASSERT_TRUE(body->set("ssid=Home&mdp=hunter22", 22));
  TEST_ASSERT_EQUAL_MEMORY("ssid=Home", storage, 9);
  body->~FormBody();
  for (size_t i = 0; i <= FORM_BODY_MAX; i++) TEST_ASSERT_EQUAL_INT(0, storage[i]);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_gate_order);
  RUN_TEST(test_bearer_is_not_a_session);
  RUN_TEST(test_gate_too_large);
  RUN_TEST(test_gate_negative_length);
  RUN_TEST(test_gate_locked_after_size);
  RUN_TEST(test_body_cap);
  RUN_TEST(test_body_decodes);
  RUN_TEST(test_body_first_value_wins);
  RUN_TEST(test_body_value_reports_full_length);
  RUN_TEST(test_body_survives_mutations);
  RUN_TEST(test_level_is_strict);
  RUN_TEST(test_panel_message_like_the_field);
  RUN_TEST(test_show_name_bounded);
  RUN_TEST(test_wifi_values_bounded);
  RUN_TEST(test_wifi_known_network_retries);
  RUN_TEST(test_wipe_clears_every_byte);
  RUN_TEST(test_body_wiped_when_destroyed);
  return UNITY_END();
}
