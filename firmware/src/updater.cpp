// SPDX-License-Identifier: AGPL-3.0-only
#include "updater.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <time.h>

#include "certs.h"
#include "display.h"
#include "https_task.h"
#include "json_lite.h"
#include "network.h"
#include "ota.h"
#include "places.h"
#include "settings.h"
#include "update_logic.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef UPDATE_MANIFEST_URL     // secrets.h may point a development board elsewhere
#define UPDATE_MANIFEST_URL \
  "https://github.com/Serein-Design/carousel-firmware/releases/latest/download/manifest.json"
#endif

// Only images signed with the matching private key install. That key lives
// in the release workflow's secrets and offline with the maintainers.
static const char PUBLIC_KEY[] =
  "-----BEGIN PUBLIC KEY-----\n"
  "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAETbm+jc0Y9VUnZdc3WEy1zMkLItrc\n"
  "QkLE93loTSxK607cwM0yzcfUPLJslgLwudcJiQzpZxxlBv3T2/qgNQUSIw==\n"
  "-----END PUBLIC KEY-----\n";

UpdateStatus updateStatus;
static volatile bool installRequested = false;
static bool onTrial = false;       // running the version just installed, not confirmed yet

struct Manifest {
  char     version[24];
  char     url[220];
  uint32_t size;
  uint8_t  sig[80];
  size_t   sigLen;
};
static Manifest manifest;

static void fail(const char *why) {
  strncpy(updateStatus.error, why, sizeof(updateStatus.error) - 1);
  updateStatus.state    = UPD_FAILED;
  updateStatus.progress = -1;
  Serial.printf("[UPDATE] %s\n", why);
}

// Versions that were installed and then rolled back are never tried again
static String badVersion() {
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  const String v = p.getString("updBad", "");
  p.end();
  return v;
}

// Integrity comes from the signature, which a man in the middle cannot forge;
// checking the server certificate as well keeps it from serving junk or
// watching which version is fetched.
static bool get(HTTPClient &http, WiFiClientSecure &client, const char *url) {
  placesTidy(true);                      // the city table's 13 KB would starve the handshake
  trustRootCertificates(client);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);   // GitHub redirects to its CDN
  http.setTimeout(15000);
  if (!http.begin(client, url)) return false;
  const int code = http.GET();
  if (code == 200) return true;
  Serial.printf("[UPDATE] HTTP %d for %s\n", code, url);
  return false;
}

static bool check() {
  updateStatus.state = UPD_CHECKING;
  WiFiClientSecure client;
  HTTPClient http;
  if (!get(http, client, UPDATE_MANIFEST_URL)) { http.end(); fail("update server unreachable"); return false; }
  const String body = http.getString();
  http.end();

  Manifest m = {};
  char sigHex[200] = "";
  float size = 0;
  if (!extractJsonString(body.c_str(), "\"version\"", m.version, sizeof(m.version)) ||
      !extractJsonString(body.c_str(), "\"url\"", m.url, sizeof(m.url)) ||
      !extractJsonString(body.c_str(), "\"sig\"", sigHex, sizeof(sigHex)) ||
      !extractJsonNumber(body, "\"size\"", size) || size <= 0 ||
      !hexToBytes(sigHex, m.sig, sizeof(m.sig), m.sigLen)) {
    fail("unreadable manifest");
    return false;
  }
  m.size = (uint32_t)size;
  manifest = m;
  strncpy(updateStatus.latest, m.version, sizeof(updateStatus.latest) - 1);
  const bool newer = isNewer(m.version, FIRMWARE_VERSION) && badVersion() != m.version;
  updateStatus.state = newer ? UPD_AVAILABLE : UPD_UP_TO_DATE;
  Serial.printf("[UPDATE] latest %s, running %s: %s\n", m.version, FIRMWARE_VERSION,
                newer ? "update available" : "up to date");
  return newer;
}

static bool signatureValid(const uint8_t sha[32]) {
  char msg[120];
  signedMessage(msg, sizeof(msg), manifest.version, sha);
  uint8_t hash[32];
  mbedtls_sha256_ret((const uint8_t *)msg, strlen(msg), hash, 0);
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  int r = mbedtls_pk_parse_public_key(&pk, (const uint8_t *)PUBLIC_KEY, sizeof(PUBLIC_KEY));
  if (r == 0) r = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash),
                                    manifest.sig, manifest.sigLen);
  mbedtls_pk_free(&pk);
  return r == 0;
}

static void install() {
  updateStatus.state    = UPD_INSTALLING;
  updateStatus.progress = 0;
  WiFiClientSecure client;
  HTTPClient http;
  if (!get(http, client, manifest.url)) { http.end(); fail("download failed"); return; }
  if ((uint32_t)http.getSize() != manifest.size || !Update.begin(manifest.size)) {
    http.end(); fail("wrong size or no room"); return;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  WiFiClient *stream = http.getStreamPtr();
  static uint8_t buf[2048];
  uint32_t done = 0, lastData = millis();
  while (done < manifest.size && millis() - lastData < 15000) {
    const size_t avail = stream->available();
    if (!avail) { delay(2); continue; }
    const size_t n = stream->readBytes(buf, min(avail, sizeof(buf)));
    mbedtls_sha256_update_ret(&ctx, buf, n);
    if (Update.write(buf, n) != n) break;
    done += n;
    lastData = millis();
    updateStatus.progress = (float)done / manifest.size;
  }
  http.end();
  uint8_t sha[32];
  mbedtls_sha256_finish_ret(&ctx, sha);
  mbedtls_sha256_free(&ctx);

  if (done != manifest.size) { Update.abort(); fail("download cut short"); return; }
  if (!signatureValid(sha))  { Update.abort(); fail("bad signature, not installed"); return; }
  if (!Update.end(true))     { fail("could not finish the install"); return; }

  // Remembered so a rollback can be blamed on this version (updateBegin)
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putString("updPending", manifest.version);
  p.end();
  Serial.printf("[UPDATE] %s installed, restarting\n", manifest.version);
  Serial.flush();
  delay(300);
  ESP.restart();
}

// Spread the panels over the hour: the minute comes from the MAC
static int autoMinute = -1;
static int lastAutoDay = -1;

uint32_t updateStep(bool asked) {
  if (autoMinute < 0) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    autoMinute = mac[5] % 60;
  }
  esp_ota_img_states_t state;
  if (onTrial && esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
      state == ESP_OTA_IMG_VALID) {
    onTrial = false;                   // confirmed (ota.cpp): nothing to blame any more
    Preferences p;
    p.begin(NVS_NAMESPACE, false);
    p.remove("updPending");
    p.end();
  }
  if (WiFi.status() != WL_CONNECTED || otaInProgress) return 60000;
  if (asked) {
    if (check() && installRequested) install();
    installRequested = false;
    return 60000;
  }
  struct tm t;
  if (!settings.autoUpdate || !isReleaseVersion(FIRMWARE_VERSION) || !timeValid ||
      !getLocalTime(&t, 0) || t.tm_hour != 4 || t.tm_min < autoMinute || t.tm_yday == lastAutoDay)
    return 60000;
  lastAutoDay = t.tm_yday;
  if (check()) install();
  return 60000;
}

void updateBegin() {
  // Did the last install survive? Running it: yes. Back on the old one with
  // a failed slot: no, and that version is set aside.
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  const String pending = p.getString("updPending", "");
  if (pending == FIRMWARE_VERSION) {
    onTrial = true;                    // cleared once confirmed, in the task
  } else if (pending.length()) {
    if (esp_ota_get_last_invalid_partition()) {
      p.putString("updBad", pending);
      snprintf(updateStatus.error, sizeof(updateStatus.error), "%s failed, rolled back", pending.c_str());
      updateStatus.state = UPD_FAILED;
    }
    p.remove("updPending");
  }
  p.end();
}

void updateCheckNow() { httpsAsk(HTTPS_UPDATE); }

void updateInstallNow() {
  installRequested = true;
  updateCheckNow();
}

bool drawUpdateProgress() {
  const float p = updateStatus.progress;
  if (p < 0) return false;
  display->clearScreen();
  display->drawRect(1, H() / 2 - 3, W() - 2, 6, display->color565(60, 60, 60));
  display->fillRect(2, H() / 2 - 2, (int)((W() - 4) * p), 4, display->color565(30, 215, 96));
  return true;
}
