// SPDX-License-Identifier: AGPL-3.0-only
#include "auth.h"

#include <Preferences.h>
#include <mbedtls/md.h>

#include "settings.h"

static char secret[33]   = "";          // 128 bits, hex: browser sessions
static char apiSecret[33] = "";          // 128 bits, hex: the API token, kept apart so
                                         // a token pasted into a script or a home
                                         // automation file cannot sign a browser in
static char hookSecret[33] = "";         // 128 bits, hex: the hook token, in agent
                                         // setups on every computer, so it opens
                                         // nothing but the hook routes
static bool hookOpen = false;
static uint32_t hookRefusals = 0;
static bool hookLogged = false;
static uint32_t hookLoggedAt = 0;
static bool installLogged = false;
static uint32_t installLoggedAt = 0;
static uint8_t proofsLeft = 4;           // the /id proof's bucket (authHookProof)
static uint32_t proofsAt = 0;

static volatile uint32_t code      = 0;  // 0 = none
static volatile uint32_t codeSince = 0;
static volatile uint8_t  wrong     = 0;
static volatile uint32_t droppedAt = 0;  // too many wrong codes, 0 = never

// A new secret in out, stored under key; answers whether NVS took it
static bool newSecret(char *out, const char *key) {
  for (int i = 0; i < 4; i++) snprintf(out + i * 8, 9, "%08lx", (unsigned long)esp_random());
  Preferences p;
  const bool stored = p.begin(NVS_NAMESPACE, false) && p.putString(key, out) == 32;
  p.end();
  return stored;
}

static void loadSecret(char *out, const char *key) {
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  const String s = p.getString(key, "");
  p.end();
  if (s.length() == 32) strcpy(out, s.c_str());
  else newSecret(out, key);
}

void authBegin() {
  loadSecret(secret, "authSecret");
  loadSecret(apiSecret, "apiSecret");
  loadSecret(hookSecret, "hookToken");
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  hookOpen = p.getBool("hookOpen", false);
  p.end();
  proofsLeft = 4;
  proofsAt = millis();
}

void authSignOutEverywhere() {
  newSecret(secret, "authSecret");
  newSecret(apiSecret, "apiSecret");
}

const char *authHookToken() { return hookSecret; }

// A token NVS refused would be replaced by the old one at the next start,
// and the computers set up with it refused: the old one stays, now too.
bool authRenewHookToken() {
  char fresh[sizeof(hookSecret)];
  if (!newSecret(fresh, "hookToken")) return false;
  memcpy(hookSecret, fresh, sizeof(fresh));
  return true;
}

bool authHookOpen() { return hookOpen; }

void authSetHookOpen(bool open) {
  hookOpen = open;
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putBool("hookOpen", open);
  p.end();
}

// Same time whatever the input: the comparison does not tell how many
// leading characters were right.
static bool same(const char *a, size_t len, const char *expected) {
  if (len != 32) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < 32; i++) diff |= a[i] ^ expected[i];
  return diff == 0;
}

bool authCookieValid(const String &header) {
  // "a=1; carousel=<secret>; b=2"
  const char *h = header.c_str();
  for (const char *p = strstr(h, "carousel="); p; p = strstr(p + 1, "carousel=")) {
    if (p != h && p[-1] != ' ' && p[-1] != ';') continue;      // "xcarousel="
    const char *v = p + 9;
    size_t n = 0;
    while (v[n] && v[n] != ';') n++;
    if (same(v, n, secret)) return true;
  }
  return false;
}

bool authBearerValid(const String &header) {
  const char *h = header.c_str();
  if (strncmp(h, "Bearer ", 7)) return false;
  return same(h + 7, strlen(h + 7), apiSecret);
}

const char *authToken() { return apiSecret; }

bool authHookTokenIs(const char *value, size_t len) { return same(value, len, hookSecret); }

// A refusal worth a serial line: the first one, then once every 10 s at most
static bool logDue(bool &logged, uint32_t &at) {
  if (logged && millis() - at < 10000) return false;
  logged = true;
  at = millis();
  return true;
}

// Dotted IPv4, as IPAddress::toString() writes it: 4 groups of 1 to 3 digits
static bool isIpv4(const char *ip) {
  if (!ip) return false;
  int groups = 0, digits = 0;
  for (const char *c = ip;; c++) {
    if (*c >= '0' && *c <= '9') {
      if (++digits > 3) return false;
    } else if ((*c == '.' || !*c) && digits) {
      groups++;
      digits = 0;
      if (!*c) return groups == 4;
    } else {
      return false;
    }
  }
}

// A proof costs an HMAC, and any host on the LAN may ask for them in a loop:
// 4 at once at most, then one every 250 ms. Shared by every client, so a
// flood delays the hook's proof by a few seconds (it retries), nothing more.
static bool proofDue() {
  const uint32_t steps = (millis() - proofsAt) / 250;
  if (steps >= 4) {
    proofsLeft = 4;
    proofsAt = millis();
  } else if (steps) {
    proofsLeft = proofsLeft + steps > 4 ? 4 : proofsLeft + steps;
    proofsAt += steps * 250;
  }
  if (!proofsLeft) return false;
  proofsLeft--;
  return true;
}

bool authHookProof(const char *nonce, const char *ip, char out[33]) {
  out[0] = '\0';
  const size_t n = nonce ? strlen(nonce) : 0;
  if (n < 16 || n > 32 || strspn(nonce, "0123456789abcdefABCDEF") != n) return false;
  if (!isIpv4(ip) || !proofDue()) return false;
  char message[32 + 1 + 15 + 1];
  const int len = snprintf(message, sizeof(message), "%s:%s", nonce, ip);
  uint8_t mac[32];
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const uint8_t *)hookSecret, 32,
                      (const uint8_t *)message, len, mac))
    return false;
  for (int i = 0; i < 16; i++) snprintf(out + 2 * i, 3, "%02x", mac[i]);
  return true;
}

bool authHookAllowed(const String &header) {
  if (hookOpen || authHookTokenIs(header.c_str(), header.length())) return true;
  hookRefusals++;
  // An old hook refused on every event would otherwise fill the serial log.
  // The kind only, never the value.
  if (logDue(hookLogged, hookLoggedAt))
    Serial.printf("[HOOK] refused: %s\n", header.length() ? "wrong token" : "no token");
  return false;
}

void authLogInstallRefused() {
  if (logDue(installLogged, installLoggedAt)) Serial.printf("[HOOK] install refused: wrong link\n");
}

uint32_t authHookRefusals() { return hookRefusals; }

String authSetCookie() {
  return String("carousel=") + secret +
         "; Path=/; Max-Age=315360000; HttpOnly; SameSite=Strict";
}

static bool codeExpired() { return !code || millis() - codeSince > PAIRING_MS; }

void pairingRequest() {
  if (!codeExpired()) return;
  if (droppedAt && millis() - droppedAt < PAIRING_COOLDOWN_MS) return;
  codeSince = millis();
  wrong     = 0;
  code      = 100000 + esp_random() % 900000;     // always 6 digits
}

bool pairingCheck(const String &typed) {
  if (codeExpired()) return false;
  if ((uint32_t)typed.toInt() == code && typed.length() == 6) {
    code = 0;
    return true;
  }
  if (++wrong >= PAIRING_ATTEMPTS) {
    code      = 0;
    droppedAt = millis();
  }
  return false;
}

uint32_t pairingCodeShown() { return codeExpired() ? 0 : code; }

float pairingTimeLeft() {
  return codeExpired() ? 0.0f : 1.0f - (float)(millis() - codeSince) / PAIRING_MS;
}
