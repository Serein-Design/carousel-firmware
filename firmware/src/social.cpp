// SPDX-License-Identifier: AGPL-3.0-only
#include "social.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "certs.h"
#include "json_lite.h"
#include "ota.h"
#include "places.h"
#include "settings.h"

SocialStats socialStats;

// HTTPS request, server certificate checked against the root store (certs.h):
// the YouTube API key must not go to whoever answers in Google's place.
static bool getHttps(const String &url, String &body, const char *agent = nullptr) {
  placesTidy(true);                          // its 13 KB would starve the handshake
  WiFiClientSecure client;
  trustRootCertificates(client);
  client.setTimeout(10000);

  HTTPClient http;
  if (!http.begin(client, url)) return false;
  if (agent) http.setUserAgent(agent);
  http.setTimeout(10000);

  int code = http.GET();
  bool ok = (code == 200);
  if (ok) body = http.getString();
  else Serial.printf("[SOCIAL] HTTP %d on %s\n", code, url.substring(0, 40).c_str());
  http.end();
  return ok;
}

static void fetchSocial() {
  String body;

  if (strlen(settings.ytKey) > 10 && strlen(settings.ytChannel) > 10) {
    String url = "https://www.googleapis.com/youtube/v3/channels?part=statistics&id="
               + String(settings.ytChannel) + "&key=" + String(settings.ytKey);
    if (getHttps(url, body)) {
      float a, v, n;
      if (extractJsonNumber(body, "\"subscriberCount\"", a)) {
        socialStats.ytSubscribers = (uint32_t)a;
        if (extractJsonNumber(body, "\"viewCount\"",  v)) socialStats.ytViews  = (uint32_t)v;
        if (extractJsonNumber(body, "\"videoCount\"", n)) socialStats.ytVideos = (uint32_t)n;
        socialStats.ytOk = true;
        socialStats.updatedAt = millis();
        Serial.printf("[YT] %lu subscribers, %lu views\n",
                      (unsigned long)socialStats.ytSubscribers, (unsigned long)socialStats.ytViews);
      }
    }
  }

  if (strlen(settings.ghUser) > 0) {
    String url = "https://api.github.com/users/" + String(settings.ghUser);
    if (getHttps(url, body, "carousel-led-esp32")) {
      float a, d;
      if (extractJsonNumber(body, "\"followers\"", a)) {
        socialStats.ghFollowers = (uint32_t)a;
        if (extractJsonNumber(body, "\"public_repos\"", d)) socialStats.ghRepos = (uint32_t)d;
        socialStats.ghOk = true;
        socialStats.updatedAt = millis();
        Serial.printf("[GH] %lu followers\n", (unsigned long)socialStats.ghFollowers);
      }
    }
  }
}

uint32_t socialStep() {
  if (WiFi.status() == WL_CONNECTED && !otaInProgress) fetchSocial();
  return 10UL * 60UL * 1000UL;               // every 10 min
}
