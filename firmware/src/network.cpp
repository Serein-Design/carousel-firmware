// SPDX-License-Identifier: AGPL-3.0-only
#include "network.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <lwip/tcpip.h>
#include <mdns.h>
#include <sys/time.h>
#include <time.h>

#include "ota.h"
#include "places_logic.h"
#include "settings.h"
#include "time_logic.h"
#include "wifi_logic.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"   // placeholders, see the file header
#endif

NetworkState networkState = NET_CONNECTING;
bool         timeValid    = false;

// ---------------------------------------------------------------------------
// WI-FI CREDENTIALS, changeable without reflashing
//
// Kept in NVS. Until something is saved, those of secrets.h are used. To
// change them:
//  - from the current network: the panel's web page, "Change Wi-Fi" link;
//  - without a network (new panel, new router, moving house): the panel opens
//    its own "CAROUSEL-XXXX" access point, right away when nothing is saved,
//    after 45 s when the saved network does not answer, or at boot if the
//    encoder button is held. Connect a phone to it: the setup page opens by
//    itself. When to open and close it is decided in wifi_logic.cpp.
// ---------------------------------------------------------------------------
char wifiSsid[33] = "";
char wifiPass[65] = "";
char setupApName[16] = SETUP_AP_NAME;

volatile bool        wifiSetupMode = false;
volatile uint8_t     apClientCount = 0;
volatile uint32_t    restartAt     = 0;
volatile WifiFailure wifiFailure   = FAIL_NONE;

static volatile uint8_t authFailures = 0;
static uint32_t lastAttemptAt  = 0;  // last WiFi.begin()
static uint32_t offlineSince   = 0;  // when the Wi-Fi was lost, 0 = connected
static uint32_t apActiveAt     = 0;  // last moment someone was on the access point
static bool     everConnected  = false;
static bool     setupForced    = false;
static bool     servicesUp     = false;
// A network saved from the setup page, not joined yet: once it is, the reach
// card shows where the pages now are (gallery.cpp). In NVS, as the network
// is joined after the restart that follows its saving.
static const char *KEY_WIFI_NEW = "wNew";
static bool     joinedNew      = false;

// Locked (an unfinished factory reset): neither the saved network nor the
// one compiled in is joined; only the setup access point opens.
static void loadWifiCredentials() {
  if (panelLocked()) return;
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  if (p.isKey("wSsid")) {
    p.getString("wSsid", wifiSsid, sizeof(wifiSsid));
    p.getString("wMdp",  wifiPass, sizeof(wifiPass));
  }
  p.end();
  if (!wifiSsid[0]) {
    strncpy(wifiSsid, WIFI_SSID, sizeof(wifiSsid) - 1);
    strncpy(wifiPass, WIFI_PASS, sizeof(wifiPass) - 1);
  }
}

// Called from the web server: local Preferences instance.
void saveWifiCredentials(const char *ssid, const char *pass) {
  if (panelLocked()) {
    Serial.println("[WIFI] panel locked: network not saved");
    return;
  }
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putString("wSsid", ssid);
  p.putString("wMdp",  pass);
  p.putBool(KEY_WIFI_NEW, true);
  p.end();
  Serial.printf("[WIFI] new network saved: %s\n", ssid);
}

// The driver reads its saved network from NVS when it starts, and NVS is
// about to close: started now, RAM storage, then its station config wiped,
// so no saved network is joined and nothing is written back.
void networkForgetSaved() {
  WiFi.persistent(false);
  wifi_config_t blank = {};
  if (!WiFi.mode(WIFI_STA) || esp_wifi_set_config(WIFI_IF_STA, &blank) != ESP_OK)
    Serial.println("[WIFI] could not clear the saved network");
}

// Runs in the Wi-Fi event task: only records why the router said no.
static void onDisconnect(WiFiEvent_t, WiFiEventInfo_t info) {
  const WifiFailure f = classifyDisconnect(info.wifi_sta_disconnected.reason);
  if (f == FAIL_NONE) return;
  wifiFailure = f;
  if (f == FAIL_WRONG_PASSWORD && authFailures < 255) authFailures++;
  Serial.printf("[WIFI] refused, reason %u\n", info.wifi_sta_disconnected.reason);
}

static void onConnected(WiFiEvent_t, WiFiEventInfo_t) {
  wifiFailure  = FAIL_NONE;
  authFailures = 0;
}

static void tryConnect() {
  WiFi.disconnect();
  WiFi.begin(wifiSsid, wifiPass);
  lastAttemptAt = millis();
  Serial.printf("[WIFI] connecting to %s\n", wifiSsid);
}

// While the access point is open, attempts are made only when nobody is on
// it (wifi_logic.cpp): looking for the router sweeps every channel and would
// kick the phone off.
static void openWifiSetup() {
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  // Open, no password: you have to be standing in front of the panel.
  WiFi.softAP(setupApName);
  WiFi.scanNetworks(true);           // the list will be ready when the page opens
  apActiveAt = lastAttemptAt = millis();
  networkState  = NET_CONNECTING;
  wifiSetupMode = true;
  Serial.printf("[WIFI] access point \"%s\" open, page http://%s\n",
                setupApName, WiFi.softAPIP().toString().c_str());
}

// Called once the saved network works: the station stays connected.
static void closeWifiSetup() {
  wifiSetupMode = false;             // the web server stops its DNS
  setupForced   = false;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  Serial.println("[WIFI] access point closed");
}

// ---------------------------------------------------------------------------
// TIME
//
// SNTP starts once the IP is up, not at boot: a request sent before fails,
// and lwIP then waits 15 s to try again (the clock showed no time for 20 s).
// Until the first answer, one server is asked per window, the same one in
// every lwIP slot: lwIP asks one slot at a time and this core's headers give
// no way to tell which one answered; this way the log can. The windows (time_logic.h) restart SNTP
// on the next server after 2 s, then every 5 s. Each start waits a random
// 0-5 s before asking (lwIP's startup delay, built into this core), so the
// 2 s window is often lost; the 5 s ones rarely. The router's own server
// comes from the DHCP lease, which writes lwIP's slot 0 and empties the
// others. After the first answer every server goes into the slots, and lwIP
// resyncs every 3 h on its own, moving to the next slot when one is silent.
// ---------------------------------------------------------------------------
static const time_t TIME_BUILT = buildDay(__DATE__);
static const char *const PUBLIC_NTP[2] = {NTP_SERVEUR_1, NTP_SERVEUR_2};

static ip_addr_t routerNtp;
static bool      haveRouter  = false;
static bool      sntpStarted = false;
static bool      ntpAnswered = false;   // SNTP runs on its own from then on
static bool      allPending  = false;   // every server still to go into the slots
static bool      netTime     = false;   // a time from the network this boot: NET_SYNCED
static uint32_t  ntpWindow   = 0;
static uint32_t  ntpWindowAt = 0;
// Written in lwIP's thread: the server in the slots, and what answered
static const uint8_t NTP_EVERY = 0xFF;
static volatile uint8_t  ntpAsked    = NTP_EVERY;
static volatile uint8_t  ntpFrom     = NTP_EVERY;
static volatile uint32_t ntpAnswers  = 0;
static volatile bool     httpDateSet = false;   // by the HTTPS task

static void logTime(TimeSource source) {
  struct tm t;
  char buf[32] = "";
  if (getLocalTime(&t, 0)) strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &t);
  Serial.printf("[NTP] time received (%s): %s\n", timeSourceName(source), buf);
}

// lwIP's thread, after each answer: the time is already set
static void onNtpTime(struct timeval *) {
  ntpFrom = ntpAsked;
  ntpAnswers = ntpAnswers + 1;
}

// lwIP's thread: one server in every slot, then a fresh start
static void askOne(void *server) {
  const uint8_t s = (uint8_t)(uintptr_t)server;
  sntp_stop();
  for (u8_t i = 0; i < SNTP_MAX_SERVERS; i++) {
    if (s == NTP_ROUTER) sntp_setserver(i, &routerNtp);
    else sntp_setservername(i, PUBLIC_NTP[s - NTP_PUBLIC_1]);
  }
  ntpAsked = s;
  sntp_init();
}

// lwIP's thread: every server, the router's first; no restart, the answer
// that just came stays
static void askEvery(void *) {
  u8_t i = 0;
  if (haveRouter) sntp_setserver(i++, &routerNtp);
  for (const char *name : PUBLIC_NTP)
    if (i < SNTP_MAX_SERVERS) sntp_setservername(i++, name);
  while (i < SNTP_MAX_SERVERS) sntp_setserver(i++, nullptr);
  ntpAsked = NTP_EVERY;
}

// A full lwIP queue: tried again at the next update
static bool startWindow() {
  const NtpServer s = ntpServerFor(ntpWindow, haveRouter);
  if (tcpip_callback(askOne, (void *)(uintptr_t)s) != ERR_OK) return false;
  ntpWindowAt = millis();
  return true;
}

// At boot, before the Wi-Fi: the DHCP lease may name the router's server.
// ponytail: the clock is not kept across a software restart (tried: the RTC
// time did not survive esp_restart on the panel); NTP answers within seconds.
static void timeBegin() {
  esp_sntp_servermode_dhcp(true);
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_set_time_sync_notification_cb(onNtpTime);
}

// Each time the IP comes up, as long as NTP has not answered: a first window
// right away. The first time, the router's server is read from the lease and
// the lease stops writing the slots (a renewal would empty them again).
static void timeOnIp() {
  if (ntpAnswered) return;
  if (!sntpStarted) {
    sntpStarted = true;
    const ip_addr_t *dhcp = esp_sntp_getserver(0);
    haveRouter = dhcp && !ip_addr_isany(dhcp);
    if (haveRouter) {
      routerNtp = *dhcp;
      Serial.printf("[NTP] router's server %s\n", ipaddr_ntoa(&routerNtp));
    }
    esp_sntp_servermode_dhcp(false);
  }
  ntpWindow = 0;
  if (!startWindow()) ntpWindowAt = millis() - ntpWindowMs(0, timeValid);   // retry next update
}

// While connected: the next server when the window is over
static void timeWindows() {
  if (!sntpStarted || ntpAnswered) return;
  if (millis() - ntpWindowAt < ntpWindowMs(ntpWindow, timeValid)) return;
  ntpWindow++;
  if (!startWindow()) ntpWindow--;
}

// loop() alone writes timeValid: the answers other tasks note are taken here
static void timeUpdate() {
  static uint32_t seen = 0;
  if (ntpAnswers != seen) {
    seen = ntpAnswers;
    timeValid = netTime = true;
    if (!ntpAnswered) {
      ntpAnswered = allPending = true;
      logTime(ntpFrom == NTP_ROUTER ? TIME_ROUTER : TIME_POOL);
    }
  }
  if (allPending && tcpip_callback(askEvery, nullptr) == ERR_OK) allPending = false;
  if (httpDateSet && !timeValid) {
    timeValid = netTime = true;
    logTime(TIME_HTTP_DATE);
  }
}

void networkHttpDate(const char *value) {
  time_t t;
  if (timeValid || ntpAnswers || httpDateSet || !parseHttpDate(value, &t) ||
      !timePlausible(t, TIME_BUILT))
    return;
  const struct timeval tv = {t, 0};
  if (settimeofday(&tv, nullptr) == 0) httpDateSet = true;
}

void networkBegin(bool forceSetup) {
  loadWifiCredentials();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // avoids micro-dropouts during DMA
  WiFi.onEvent(onDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(onConnected,  ARDUINO_EVENT_WIFI_STA_GOT_IP);
  // Every panel its own access point name: two panels in setup side by side
  // (or a neighbour's) cannot be mistaken for each other.
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(setupApName, sizeof(setupApName), SETUP_AP_NAME "-%02X%02X", mac[4], mac[5]);
  panelNameFromMac(mac);   // and its own mDNS name, carousel-695c
  setenv("TZ", FUSEAU_HORAIRE, 1);
  tzset();
  timeBegin();
  if (settings.zone[0]) {
    applyZone(settings.zone);
  } else if (settings.utcOffset != UTC_OFFSET_UNKNOWN) {
    // Until the place's zone is found (weather.cpp): the last offset an
    // earlier firmware stored, without its daylight saving
    char tz[16];
    posixZone(tz, sizeof(tz), settings.utcOffset);
    applyZone(tz);
  }
  networkState = NET_CONNECTING;
  offlineSince = millis();
  setupForced  = forceSetup;
  if (forceSetup) {
    Serial.println(panelLocked() ? "[WIFI] panel locked: Wi-Fi setup"
                                 : "[WIFI] button held at boot: Wi-Fi setup");
    openWifiSetup();
  } else if (wifiSsid[0]) {
    tryConnect();
  }
}

void applyZone(const char *tz) {
  setenv("TZ", tz, 1);
  tzset();
  Serial.printf("[TIME] zone %s\n", tz);
}

// carousel.local, the name the README and the hook start from, is taken when
// no other device answers to it: asked once, 1.5 s, without blocking the
// loop. The panel starts at its own carousel-xxxx.local and switches; with
// two panels, the first one up keeps carousel.local.
static mdns_search_once_t *sharedNameQuery = nullptr;

static void sharedNameUpdate() {
  if (!sharedNameQuery) return;
  mdns_result_t *answers = nullptr;
  if (!mdns_query_async_get_results(sharedNameQuery, 0, &answers)) return;   // still asking
  mdns_query_async_delete(sharedNameQuery);
  sharedNameQuery = nullptr;
  if (answers) {
    mdns_query_results_free(answers);
    Serial.printf("[MDNS] " HOSTNAME ".local is taken: http://%s\n", panelHostName());
  } else if (mdns_hostname_set(HOSTNAME) == ESP_OK) {
    panelUseSharedName();
    Serial.println("[MDNS] http://" HOSTNAME ".local");
  }
}

// The first join of a network saved from the setup page, told once
static void noteNewNetwork() {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  if (p.isKey(KEY_WIFI_NEW)) {
    joinedNew = true;
    p.remove(KEY_WIFI_NEW);
  }
  p.end();
}

bool networkTakeJoinedNew() {
  const bool j = joinedNew;
  joinedNew = false;
  return j;
}

// mDNS and OTA, the first time the network works
static void startServices() {
  if (servicesUp) return;
  servicesUp = true;
  noteNewNetwork();
  // Local domain name: the address no longer moves even if the router hands
  // out another IP at the next boot.
  if (MDNS.begin(panelMdnsName())) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[MDNS] http://%s\n", panelHostName());
    sharedNameQuery = mdns_query_async_new(HOSTNAME, nullptr, nullptr, MDNS_TYPE_A, 1500, 1, nullptr);
    if (!sharedNameQuery) Serial.println("[MDNS] could not ask for " HOSTNAME ".local");
  } else {
    Serial.println("[MDNS] failed, use the IP");
  }
  otaBegin();
}

void networkUpdate() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < 500) return;       // twice a second is enough
  lastCheck = millis();

  if (restartAt && (int32_t)(millis() - restartAt) >= 0) {
    Serial.println("[WIFI] restarting on the new network");
    Serial.flush();
    ESP.restart();
  }
  if (restartAt) return;                        // saved, leave the phone be
  timeUpdate();

  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;
  apClientCount = wifiSetupMode ? WiFi.softAPgetStationNum() : 0;
  if (apClientCount) apActiveAt = now;
  if (connected) offlineSince = 0;
  else if (!offlineSince) offlineSince = now;

  WifiView v;
  v.haveCredentials = wifiSsid[0] != '\0';
  v.connected       = connected;
  v.everConnected   = everConnected;
  v.setupOpen       = wifiSetupMode;
  v.setupForced     = setupForced;
  v.apClients       = apClientCount;
  v.authFailures    = authFailures;
  v.offlineMs       = connected ? 0 : now - offlineSince;
  v.apIdleMs        = now - apActiveAt;
  v.sinceAttemptMs  = now - lastAttemptAt;
  switch (wifiDecide(v)) {
    case WIFI_OPEN_SETUP:  openWifiSetup();  return;
    case WIFI_CLOSE_SETUP: closeWifiSetup(); break;
    case WIFI_RETRY:       tryConnect();     break;
    case WIFI_WAIT:                          break;
  }
  if (wifiSetupMode) return;

  if (!connected) {
    networkState = NET_CONNECTING;
    return;
  }
  everConnected = true;
  if (networkState == NET_CONNECTING) {
    networkState = NET_CONNECTED;
    // The signal tells right away whether a sluggish panel suffers from the
    // Wi-Fi: below -75 dBm, hooks arrive late or not at all.
    Serial.printf("[WIFI] connected, IP %s, signal %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    startServices();
    timeOnIp();
  }
  sharedNameUpdate();
  timeWindows();
  // A time from the network, not the kept one: the update's health check
  // (ota.cpp) waits for proof that the Internet answers
  if (netTime) networkState = NET_SYNCED;
}
