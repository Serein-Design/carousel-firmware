// SPDX-License-Identifier: AGPL-3.0-only
#include "web.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <uri/UriBraces.h>

#include <climits>
#include <memory>
#include <new>
#include <type_traits>

#include "agent_install.h"
#include "agent_scripts.h"
#include "auth.h"
#include "agent_bridge.h"
#include "content.h"
#include "display.h"
#include "gallery.h"
#include "i18n.h"
#include "json_lite.h"
#include "mqtt.h"
#include "network.h"
#include "panel_name.h"
#include "places.h"
#include "posters.h"
#include "preview_logic.h"
#include "region_logic.h"
#include "request.h"
#include "settings.h"
#include "settings_logic.h"
#include "updater.h"
#include "weather.h"
#include "social.h"
#include "web_access.h"
#include "web_assets.h"
#include "web_forms.h"
#include "web_images.h"
#include "web_panel.h"
#include "web_screens.h"
#include "web_shell.h"
#include "web_stage.h"

// What WebServer does not offer:
//  - the current client itself, not a copy, to read a hook's body as it
//    streams in (rawAgentBody);
//  - a chunked answer cut short: client() is a copy, whose stop() closes
//    nothing, and the handler's return sends the last chunk, so the browser
//    would keep a truncated file as a whole one;
//  - a backlog of 8 connections instead of 4: a burst of tools run together
//    exceeded it, and the extra hooks were refused;
//  - TCP_NODELAY: without it, a response's header and body leave in two
//    packets, and the second waits for Windows' delayed ACK, up to 200 ms per
//    response.
class PanelServer : public WebServer {
public:
  explicit PanelServer(int port) : WebServer(port) {
    _server = WiFiServer(port, 8);   // before begin(): no socket open yet
  }
  WiFiClient &currentClient() { return _currentClient; }
  // The socket closed with no last chunk: the browser sees the body incomplete
  void abortChunked() {
    _chunked = false;
    _currentClient.stop();
  }
  // After begin(), which resets it to false
  void disableNagle() { _server.setNoDelay(true); }
};
static PanelServer server(80);

static DNSServer dns;

// ===========================================================================
// PAGES: sent in pieces, each built by its module
// ===========================================================================

// An empty chunk would end the response: a chunk whose String could not
// grow is skipped instead.
static void sendChunk(const String &s) {
  if (s.length()) server.sendContent(s);
}

// A page's flush: sent, then emptied, keeping its reserve
static void sendAndClear(String &s) {
  sendChunk(s);
  s = "";
}

// A signed-in page's head, before its pieces: never stored by the browser,
// so the back button or a shared computer shows no stale copy (Panel holds
// the API token)
static void startPage(int code) {
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(code, "text/html; charset=utf-8", "");
}

// A browser route's refusal or failure, as a page (SIGN-IN section below)
static void sendError(int code, Tab back = TAB_STAGE);

// Panel (web_panel.cpp builds it), sent in pieces. values: what the form
// shows, the stored settings or, after a refused or failed save, what was
// posted; the secrets' marks come from what is stored, and no secret is
// sent back either way. Low memory answers the request's own code. feedback:
// its own page (/panel/feedback), which needs the values and the result only.
static void sendPanelPage(int code, SaveResult result, const Settings &values, ListCheck refusal,
                          bool feedback = false) {
  PanelView v = {};
  v.values = &values;
  v.stored = &settings;
  v.result = result;
  v.refusal = refusal;
  // Written by the HTTPS task: a word torn mid-update shows for one load
  v.update = updateStatus.state;
  v.latest = updateStatus.latest;
  v.updateError = updateStatus.error;
  v.ssid = wifiSsid;
  v.connected = WiFi.status() == WL_CONNECTED;
  v.rssi = v.connected ? (int)WiFi.RSSI() : 0;
  v.mqtt = !settings.mqttHost[0] ? MQTT_OFF : mqttConnected() ? MQTT_UP : MQTT_DOWN;
  v.sessionCount = activeSessionCount();
  const String ip = WiFi.localIP().toString();
  v.ip = ip.c_str();
  v.token = authToken();
  v.hookToken = authHookToken();
  v.hookOpen = authHookOpen();
  v.hookRefusals = authHookRefusals();
  v.freeKb = ESP.getFreeHeap() / 1024;
  v.minFreeKb = ESP.getMinFreeHeap() / 1024;
  v.resetReason = resetReason();
  v.upSec = millis() / 1000;
  const Shell sh = {&tr(), nullptr, panelHostName(), TAB_PANEL, true, FIRMWARE_VERSION};
  String s;
  if (!s.reserve(PANEL_PAGE_RESERVE)) {
    server.send(code, "text/html; charset=utf-8", tr().webOutOfMemory);
    return;
  }
  startPage(code);
  if (feedback) feedbackPage(s, sh, v, sendAndClear);
  else          panelPage(s, sh, v, sendAndClear);
  sendChunk(s);
  server.sendContent("");
}


static int imageList(std::unique_ptr<ImageInfo[]> &list);

// Running order, or one screen's page (screen >= 0), web_screens.cpp
// builds both, sent in pieces. values: as in sendPanelPage; refusal: the
// list's, its banner in the side column. The image list is read for its
// count only, on the heap and freed before the page is built.
static void sendOrderPage(int code, SaveResult result, const Settings &values,
                          ListCheck refusal, int screen = -1) {
  const bool storage = contentAvailable();
  int images = 0;
  if (storage) {
    std::unique_ptr<ImageInfo[]> list;
    // Unreadable (memory, the lock): the page says no count rather than
    // losing a save's answer
    images = imageList(list);
    if (images < 0) images = 0;
  }
  char choice[IMAGE_NAME_MAX + 1];
  contentChoice(choice, sizeof(choice));
  OrderView v = {};
  v.screens = {&values, &settings, refusal, locateFailed, {storage, images, choice}};
  const int current = screenFind(galleryCurrentScreen());
  v.current = current < 0 ? 0 : (uint8_t)current;
  v.portrait = portrait;
  // Written by the HTTPS task: a count torn mid-update shows for one load
  const SocialStats st = socialStats;
  v.social = {st.ytOk, st.ghOk, (long)st.ytSubscribers, (long)st.ghFollowers,
              st.updatedAt ? (millis() - st.updatedAt) / 60000UL : 0};
  v.result = result;
  const Shell sh = {&tr(), nullptr, panelHostName(), TAB_ORDER, true, FIRMWARE_VERSION};
  String s;
  if (!s.reserve(ORDER_PAGE_RESERVE)) {   // a stored save still answers its own code
    server.send(code, "text/html; charset=utf-8", tr().webOutOfMemory);
    return;
  }
  startPage(code);
  if (screen >= 0) screenPage(s, sh, v, (uint8_t)screen, sendAndClear);
  else             orderPage(s, sh, v, sendAndClear);
  sendChunk(s);
  server.sendContent("");
}

// A save's answer, on the page its form named (pg)
static void sendSaveAnswer(int code, SaveResult result, const Settings &values,
                           ListCheck refusal) {
  uint8_t screen = 0;
  switch (saveAnswerPage(server.arg("pg").c_str(), &screen)) {
    case SAVE_TO_ORDER:  sendOrderPage(code, result, values, refusal); break;
    case SAVE_TO_SCREEN: sendOrderPage(code, result, values, refusal, screen); break;
    case SAVE_TO_PANEL:  sendPanelPage(code, result, values, refusal); break;
    case SAVE_TO_FEEDBACK: sendPanelPage(code, result, values, refusal, true); break;
  }
}

static void copyArg(const char *name, char *dest, size_t size) {
  if (!server.hasArg(name)) return;
  strncpy(dest, server.arg(name).c_str(), size - 1);
  dest[size - 1] = '\0';
}

static void sendWifiPage(const char *message, const char *name = nullptr,
                         bool restarting = false, bool scan = false);

// The settings form's fields, today's rules: each one posted is read into
// s, held to its bounds; moved: the city changed (the weather looks it up),
// chosen: the suggestion picked for it (-1 none); mqttChanged: the broker's
// settings did (MQTT reconnects).
static void applyForm(Settings &s, bool *moved, int32_t *chosen, bool *mqttChanged) {
  // The page sends these as typed, unbounded (web_screens.cpp): read and held
  // here; text that is not a number keeps what is stored
  if (server.hasArg("sal")) s.salary    = formDecimal(server.arg("sal").c_str(), s.salary);
  if (server.hasArg("hd"))  s.workStart = formNumber(server.arg("hd").c_str(), 0, 23, s.workStart);
  if (server.hasArg("hf"))  s.workEnd   = formNumber(server.arg("hf").c_str(), 1, 24, s.workEnd);
  if (server.hasArg("jm"))  s.workDays  = formNumber(server.arg("jm").c_str(), 1, 31, s.workDays);
  if (server.hasArg("ig"))
    s.instaFollowers = formNumber(server.arg("ig").c_str(), 0, LONG_MAX, s.instaFollowers);
  if (server.hasArg("xa"))
    s.xFollowers = formNumber(server.arg("xa").c_str(), 0, LONG_MAX, s.xFollowers);
  if (server.hasArg("nde")) s.nightStart = formNumber(server.arg("nde").c_str(), 0, 23, s.nightStart);
  if (server.hasArg("nto")) s.nightEnd   = formNumber(server.arg("nto").c_str(), 0, 23, s.nightEnd);
  if (server.hasArg("nlv")) s.nightBrightness = constrain(server.arg("nlv").toInt(), 0, 255);
  if (server.hasArg("lang")) {
    const long l = server.arg("lang").toInt();
    s.lang = (l >= 0 && l < LANG_COUNT) ? (uint8_t)l : (uint8_t)LANG_EN;
  }
  if (server.arg("ytk").length()) copyArg("ytk", s.ytKey, sizeof(s.ytKey));
  copyArg("ytc", s.ytChannel, sizeof(s.ytChannel));
  copyArg("gh",  s.ghUser,    sizeof(s.ghUser));
  if (server.hasArg("auh")) s.autoUpdate = server.hasArg("au");
  // Only when edited: MQTT or the API may have set another one since the
  // page loaded, which an untouched field would put back
  if (server.hasArg("msg") &&
      formEdited(server.arg("msg").c_str(),
                 server.hasArg("msg0") ? server.arg("msg0").c_str() : nullptr)) {
    // The font only draws ASCII: accents are folded now, so the page shows
    // what the panel will. Stage's Send stores it the same way.
    panelMessage(server.arg("msg").c_str(), s.message, sizeof(s.message));
  }
  // MQTT: reconnect only when something changed
  if (server.hasArg("mqh") && server.arg("mqh") != s.mqttHost) {
    copyArg("mqh", s.mqttHost, sizeof(s.mqttHost));
    *mqttChanged = true;
  }
  if (server.hasArg("mqp")) {
    const long port = server.arg("mqp").toInt();
    if (port >= 1 && port <= 65535 && port != s.mqttPort) {
      s.mqttPort = (uint16_t)port;
      *mqttChanged = true;
    }
  }
  if (server.hasArg("mqu") && server.arg("mqu") != s.mqttUser) {
    copyArg("mqu", s.mqttUser, sizeof(s.mqttUser));
    *mqttChanged = true;
  }
  if (server.arg("mqw").length()) {                  // empty: keep the saved one
    copyArg("mqw", s.mqttPass, sizeof(s.mqttPass));
    *mqttChanged = true;
  }
  // cid: the suggestion chosen under the field, its number in the city
  // table, which wins over the name (the HTTPS task names the city after
  // it); city: the name in the field, the suggestion's or, without scripts,
  // the one typed, whose best match is taken. Stored at once either way, so
  // the page answering this save shows it. A suggestion is a change even
  // when its name is the one stored: another Springfield.
  const String cid = server.arg("cid");
  const long id = formId(cid.c_str(), 0xFFFF);   // anything else: the name typed
  if (id >= 0) {
    *chosen = (int32_t)id;
    s.place[0] = '\0';             // the HTTPS task looks it up
    *moved = true;
  }
  if (server.hasArg("city")) {
    String city = server.arg("city");
    city.trim();
    if (city != s.city) {
      strncpy(s.city, city.c_str(), sizeof(s.city) - 1);
      s.city[sizeof(s.city) - 1] = '\0';
      s.place[0] = '\0';
      *moved = true;
    }
  }

  if (s.workEnd <= s.workStart) s.workEnd = s.workStart + 1;   // guard
}

// The copies of the settings below hold the YouTube key and the MQTT
// password: wiped with wipeSecret once used (a default Settings, assigned, built
// some 400 bytes more code), so no stale secret stays in RAM
static_assert(std::is_trivially_copyable<Settings>::value, "Settings is wiped bytewise");

// saveSettings() puts it back if the flash refuses a save: never shown and
// played as if applied, then lost or half-kept at the next restart. One
// copy for every browser save (the settings form, Stage's Send): static,
// ~600 bytes kept off the web task's 8 KB stack; only that task uses it.
// Wiped once used: it holds the old YouTube key and MQTT password.
static Settings before;

// The form applied to posted, a copy of the settings, then checked, stored
// and applied, and the page answered
static void saveForm(Settings &posted) {
  bool moved = false, mqttChanged = false;
  int32_t chosen = -1;
  applyForm(posted, &moved, &chosen, &mqttChanged);
  // The Screens controls and the styles, by name, in the order the browser
  // posts them
  ScreensForm form(posted.screens, posted.rotateSec);
  StylesForm styles;
  FeedbackForm feedback;
  RegionForm region;
  for (int i = 0; i < server.args(); i++) {
    form.arg(server.argName(i).c_str(), server.arg(i).c_str());
    styles.arg(server.argName(i).c_str(), server.arg(i).c_str());
    feedback.arg(server.argName(i).c_str(), server.arg(i).c_str());
    region.arg(server.argName(i).c_str(), server.arg(i).c_str());
  }
  styles.apply(posted);
  feedback.apply(posted);
  region.apply(posted);
  if (form.hasList()) {
    posted.screens = form.list();
    posted.screensStored = true;
  }
  if (form.hasRotate()) {
    posted.rotateSec = form.rotateSec();
    posted.screensStored = true;
  }
  // Checked before anything is stored or applied: an orientation without a
  // screen would leave the panel nothing to draw
  const ListCheck check = screenListCheck(posted.screens);
  if (check != LIST_OK) {
    Serial.printf("[SCREENS] list refused: %s\n", check == LIST_EMPTY ? "no screen on"
                  : check == LIST_NO_PORTRAIT ? "no portrait screen" : "no landscape screen");
    sendSaveAnswer(400, SAVE_REFUSED, posted, check);
    return;
  }

  keepLocation(posted, settings, moved);     // what the HTTPS task found meanwhile
  before = settings;
  settings = posted;
  const bool saved = saveSettings(&before);   // refused: settings back to before
  wipeSecret(&before, sizeof(before));
  if (!saved) {                                // no change, no side effect
    sendSaveAnswer(200, SAVE_FAILED, posted, LIST_OK);
    return;
  }
  // saveSettings() also switched the language
  if (moved) {
    locateFailed = false;
    placesChosen = chosen;
    weatherRefresh();
  }
  if (mqttChanged) mqttApplySettings();
  // Not taken (logged there): stored all the same, the panel loads it at
  // boot. A form without the list and the interval (Panel's) applies none,
  // so the rotation's count is not restarted.
  const bool applied = !(form.hasList() || form.hasRotate()) ||
                       galleryApplyList(settings.screens, settings.rotateSec);
  sendSaveAnswer(200, applied ? SAVE_DONE : SAVE_LATER, settings, LIST_OK);
}

static void handleSave() {
  // The form is applied to a copy, so a refused list leaves the running
  // settings untouched, and the page shows what was posted. On the heap
  // (~640 bytes, for this request only), off the web task's 8 KB stack.
  Settings *posted = new (std::nothrow) Settings(settings);
  if (!posted) { server.send(500, "text/html; charset=utf-8", tr().webOutOfMemory); return; }
  saveForm(*posted);
  // It holds the YouTube key and the MQTT password: cleared before its
  // memory goes back to the heap, as before is
  wipeSecret(posted, sizeof(*posted));
  delete posted;
}

// ===========================================================================
// WI-FI PAGE: change network without reflashing
//
// Served at /wifi, and instead of the home page while the setup access point
// is open.
// ===========================================================================
// A crowded street can show more access points than this; the scan's first
// ones are read (the page lists at most WIFI_ROWS_MAX names anyway)
static const int WIFI_SCAN_READ_MAX = 64;
static_assert(WIFI_SCAN_BUSY == WIFI_SCAN_RUNNING && WIFI_SCAN_NONE == WIFI_SCAN_FAILED,
              "web_access names WiFi.scanComplete()'s answers");
// name: the network a message names (wifiMsgSaved's %s); restarting: the
// answer to a save or a retry, sent before the scheduled restart; scan: the
// visitor asked for a new list (a GET's ?scan: a post's answer never reads
// the address, which the library leaves unparsed)
static String buildWifiPage(const char *message, const char *name, bool restarting, bool scan) {
  // The list comes from a background scan: started when the access point
  // opens, or on request. While it runs (~3 s), this request waits for it,
  // here in the web task (loop() and the panel go on), so the page comes
  // with the list instead of reloading itself. A scan not done after about
  // 4 s answers an empty list and the Refresh link. An answer a restart
  // follows takes the list as it is (wifiScanPlan).
  const WifiScanPlan plan =
      wifiScanPlan(WiFi.scanComplete(), scan, restarting);
  if (plan.start) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
  }
  const int n = wifiAwaitScan([] { return (int)WiFi.scanComplete(); },
                              [] {
                                // The captive portal's DNS keeps answering meanwhile
                                if (wifiSetupMode) dns.processNextRequest();
                                delay(50);
                              },
                              plan.pauses);
  // On the heap for this request only (36 bytes a network); without it the
  // page still offers the typed name
  const int seen = n > 0 ? min(n, WIFI_SCAN_READ_MAX) : 0;
  std::unique_ptr<WifiNet[]> nets(seen ? new (std::nothrow) WifiNet[seen] : nullptr);
  int rows = 0;
  if (nets) {
    for (int i = 0; i < seen; i++) {
      WifiNet &w = nets[i];
      strncpy(w.ssid, WiFi.SSID(i).c_str(), sizeof(w.ssid) - 1);
      w.ssid[sizeof(w.ssid) - 1] = '\0';
      w.rssi = WiFi.RSSI(i);
      w.open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    }
    rows = wifiRows(nets.get(), seen);
  }
  const String ip = WiFi.localIP().toString();
  const WifiLink link = wifiSetupMode                    ? LINK_SETUP
                      : WiFi.status() == WL_CONNECTED    ? LINK_CONNECTED
                                                         : LINK_OFFLINE;
  const WifiTry last = wifiFailure == FAIL_WRONG_PASSWORD ? TRY_REFUSED
                     : wifiFailure == FAIL_NOT_FOUND      ? TRY_NOT_FOUND
                                                          : TRY_NONE;
  const WifiPageView v = {wifiSsid, link, ip.c_str(), last, nets.get(), rows,
                      message, name};
  const Shell sh = {&tr(), nullptr, panelHostName(), TAB_NONE, true, FIRMWARE_VERSION};
  String s;
  s.reserve(WIFI_PAGE_BASE + rows * WIFI_ROW_BYTES);
  wifiPage(s, sh, v);
  return s;
}

static void sendWifiPage(const char *message, const char *name, bool restarting, bool scan) {
  server.sendHeader("Cache-Control", "no-store");   // as startPage(): a signed-in page
  server.send(200, "text/html; charset=utf-8", buildWifiPage(message, name, restarting, scan));
}

// The phone's zone, "America/Argentina/Buenos_Aires": its POSIX TZ from
// tz_names, applied at the restart, and its city ("Buenos Aires"), which the
// HTTPS task looks up in the city table. Only when no city was chosen:
// then the zone is the city's.
static void zoneFromPhone(const char *tz) {
  char city[sizeof(settings.city)], zone[sizeof(settings.zone)];
  if (settings.city[0] || !phoneZone(tz, city, sizeof(city), zone, sizeof(zone))) return;
  memcpy(settings.zone, zone, sizeof(zone));
  if (city[0]) {
    memcpy(settings.city, city, sizeof(city));
    settings.place[0] = '\0';
  }
  saveSettings();
}

// The Wi-Fi form's values, one byte more than each may hold: a longer one
// is told by the length value() returns. On the heap for the request, the
// password wiped before it goes back.
struct WifiForm { char typed[33], row[33], pass[65], tz[64]; };

// Saving, from the capped body. The restart is scheduled, not immediate: the
// confirmation page must go out first. networkUpdate() does the restart.
static void handleWifi(const FormBody &body) {
  std::unique_ptr<WifiForm> f(new (std::nothrow) WifiForm);
  if (!f) { sendError(503); return; }
  // A typed name wins over the chosen row: the field is never filled in
  // advance, so whatever it holds was meant
  const int typedLen = body.value("other", f->typed, sizeof(f->typed));
  const int rowLen = body.value("ssid", f->row, sizeof(f->row));
  const char *ssid = wifiChosenName(f->typed, f->row);
  int passLen = body.value("mdp", f->pass, sizeof(f->pass));
  const WifiValues check = wifiValues(ssid == f->typed ? typedLen : rowLen, passLen);
  // The known network and no password: nothing to save, the panel restarts
  // and retries it with the one it stores (the page's default choice)
  if (check == WIFI_VALUES_OK && wifiRetrySaved(ssid, passLen, wifiSsid)) {
    restartAt = millis() + 2000;
    sendWifiPage(tr().wifiMsgRetry, nullptr, true);
    return;
  }
  if (check == WIFI_VALUES_OK) {
    saveWifiCredentials(ssid, f->pass);
    // The phone's units, under Panel's names and through its parser. Before
    // the city's save below: that one writes every key, these three too, and
    // the preset fills only keys still absent
    RegionForm phone;
    char digit[3];   // a second character is kept, so "10" is refused, not read as "1"
    for (const char *name : {"tu", "cu", "ck"})
      if (body.value(name, digit, sizeof(digit)) >= 0) phone.arg(name, digit);
    savePhoneRegion(phone.choice());
    body.value("tz", f->tz, sizeof(f->tz));
    zoneFromPhone(f->tz);
    restartAt = millis() + 2500;
  }
  wipeSecret(f->pass, sizeof(f->pass));
  if (check == WIFI_BAD_SSID)          sendWifiPage(tr().wifiMsgBadSsid);
  else if (check == WIFI_BAD_PASSWORD) sendWifiPage(tr().wifiMsgBadPassword);
  else                                 sendWifiPage(tr().wifiMsgSaved, ssid, true);
}

// ===========================================================================
// CODING AGENT HOOK ENDPOINT
// ===========================================================================

// Route called by the hooks. It runs in webTask, on core 0: it does not
// touch the table, it posts an event to loop().
static bool crossSite();             // SIGN-IN section below
static bool hostAllowed();           // idem

// A hook call's verdict: 0 to go on, else the refusal's status. A browser
// page of another site first (403), then the hook token (401; one refusal
// counted).
static int16_t hookVerdict() {
  if (crossSite()) return 403;
  if (!authHookAllowed(server.header("X-Hook-Token"))) return 401;
  return 0;
}

// A POST's verdict, taken at RAW_START, before the body (-1: none taken).
// The handler uses it rather than asking again, so a refusal counts once.
static int16_t postVerdict = -1;

// Start of the hook's POST body. WebServer used to load the whole body in
// memory: PostToolUse puts the tool's complete output in it (a file read in
// full), PreToolUse the whole file of a Write. Tens of KB to allocate on a
// heap already eaten into by DMA: when the allocation failed, the identity
// was lost and a "default" session appeared.
//
// With a "raw" handler, rawAgentBody receives the body: it keeps the start,
// where session_id and cwd are, and lets the rest go by without allocating.
static char   hookBody[1536];
static size_t hookBodyLen = 0;

static void rawAgentBody() {
  HTTPRaw &r = server.raw();
  if (r.status == RAW_START) {
    hookBodyLen = 0;
    hookBody[0] = '\0';
    // Refused before any byte of the body is read: a device without the
    // token gets no buffer filled and keeps no one waiting. Marked as read
    // in full, WebServer's loop reads nothing more, and the handler closes
    // the connection with what is left unread.
    postVerdict = hookVerdict();
    if (postVerdict) {
      r.totalSize = server.clientContentLength();
      return;
    }
    // The whole body is read here, from the start. WebServer's loop reads it
    // in 1436-byte blocks with Stream::readBytes, which waits for each block
    // to be full: the last, shorter one cost the whole read timeout (1 s) on
    // every hook, and with a short timeout a large body was dropped at the
    // first Wi-Fi pause (measured: 300 KB cut after 4 s). Here we take what
    // has arrived, yielding between two packets, and keep only the start.
    WiFiClient &c = server.currentClient();
    const size_t total = server.clientContentLength();
    size_t read = 0;
    uint32_t lastPacketAt = millis();
    uint8_t buffer[512];
    // 1 s without a byte, or client gone (Claude Code drops a hook after its
    // timeout): let go, otherwise the next hooks wait.
    while (read < total && millis() - lastPacketAt < 1000) {
      const int avail = c.available();
      const int n = (avail > 0) ? c.read(buffer, min(sizeof(buffer), min((size_t)avail, total - read))) : 0;
      if (n <= 0) {
        if (!c.connected()) break;
        vTaskDelay(1);
        continue;
      }
      const size_t keep = min((size_t)n, sizeof(hookBody) - 1 - hookBodyLen);
      memcpy(hookBody + hookBodyLen, buffer, keep);
      hookBodyLen += keep;
      read += n;
      lastPacketAt = millis();
    }
    hookBody[hookBodyLen] = '\0';
    // WebServer's loop runs until totalSize reaches the announced length:
    // body read in full, it has nothing left to do. Otherwise it takes over,
    // receives nothing more and gives up cleanly.
    r.totalSize = read;
    return;
  }
  // Client gone midway: handleAgent will not be called, drop this partial
  // body and its verdict so that no following request picks them up.
  if (r.status == RAW_ABORTED) {
    hookBodyLen = 0;
    hookBody[0] = '\0';
    postVerdict = -1;
    return;
  }
  if (r.status != RAW_WRITE || postVerdict) return;
  const size_t n = min(sizeof(hookBody) - 1 - hookBodyLen, r.currentSize);
  memcpy(hookBody + hookBodyLen, r.buf, n);
  hookBodyLen += n;
  hookBody[hookBodyLen] = '\0';
}

static void handleAgent() {
  // In POST, the body goes through rawAgentBody and WebServer does not
  // parse the URL: server.arg() would then return the arguments of the
  // PREVIOUS request. After a test curl with ?s=test, every hook would have
  // gone to the "test" session. In raw mode, only the body is read.
  const bool raw = server.method() == HTTP_POST;
  // A POST that skipped the raw reader (a multipart form) has no verdict yet
  const int16_t refusal = raw && postVerdict >= 0 ? postVerdict : hookVerdict();
  postVerdict = -1;
  if (refusal) {
    hookBodyLen = 0;
    hookBody[0] = '\0';
    server.sendHeader("Connection", "close");
    server.send(refusal, "text/plain", refusal == 401 ? "token" : "refused");
    server.currentClient().stop();   // and the unread body with it
    return;
  }
  const String e = server.pathArg(0);
  AgentEvent ev = {};
  ev.state = parseAgentState(e.c_str());

  // The http hook posts its JSON payload: the session identity and the
  // project name are read from it.
  char cwd[160] = "";
  if (raw) {
    extractJsonString(hookBody, "\"session_id\"", ev.id, sizeof(ev.id));
    if (extractJsonString(hookBody, "\"cwd\"", cwd, sizeof(cwd))) projectName(cwd, ev.name, sizeof(ev.name));
  } else {
    // Hook script format (tools/agent-bridge/hook.sh): ?s=<id>&n=<name>&t=<us>
    // To test from the PC:
    //   curl -H 'X-Hook-Token: <token>' "http://<ip>/agent/attention?s=test&n=essai"
    if (server.hasArg("t")) ev.t = strtoull(server.arg("t").c_str(), nullptr, 10);
    if (server.hasArg("s")) {
      strncpy(ev.id, server.arg("s").c_str(), sizeof(ev.id) - 1);
      ev.id[sizeof(ev.id) - 1] = '\0';
    }
    if (server.hasArg("n")) {
      strncpy(ev.name, server.arg("n").c_str(), sizeof(ev.name) - 1);
      ev.name[sizeof(ev.name) - 1] = '\0';
    }
  }
  hookBodyLen = 0;
  hookBody[0] = '\0';

  // "Connection: close" is essential: without it the client keeps the
  // connection open, and the ESP32 only has a handful of sockets. A burst of
  // hooks used them up, and the following requests timed out without the
  // panel crashing - it simply had no room left to answer.
  server.sendHeader("Connection", "close");
  if (ev.state == EV_UNKNOWN) { server.send(400, "text/plain", "unknown state"); return; }
  // Without an identity nothing is done: these requests used to open a
  // "default" session, saved in NVS, that stayed in the bar for 15 min.
  if (!ev.id[0]) { server.send(400, "text/plain", "missing session_id: add ?s=test"); return; }

  // Empty body: Claude Code requires JSON or nothing. With "ok", every
  // successful http hook showed up as an error in the conversation.
  server.send(200, "text/plain", "");
  server.currentClient().stop();   // client() is a copy: its stop() closes nothing

  // Queue full: lose the event rather than make the hook wait
  if (!postAgentEvent(ev))
    Serial.println("[AGENT] queue full, event lost");
}

// ===========================================================================

// The web server has its own task, on core 0. In loop(), the animation froze
// while each request was read, and hooks waited for the current frame to end
// before being served.
static void webTask(void *) {
  bool dnsActive = false;
  for (;;) {
    // The captive portal's DNS lives here, with the server: started and
    // stopped by the same task that serves it, there is no concurrent access.
    if (wifiSetupMode != dnsActive) {
      if (wifiSetupMode) dns.start(53, "*", WiFi.softAPIP());
      else               dns.stop();
      dnsActive = wifiSetupMode;
    }
    if (dnsActive) dns.processNextRequest();
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// The installer, for the command on the Panel page:
//   curl -sS --compressed --noproxy '*' "http://<ip>/install?k=<token>" | sh
// The token rides in the address because "curl | sh" has nowhere else to
// carry it. sh runs whatever comes back, so every refusal is a script that
// says why and exits 1 (agent_install.h). The same checks as a page, in the
// same order, then the token in constant time. The script exists in flash
// only compressed (6 KB, not 16): a curl that did not ask for gzip, an
// older command, would pipe the bytes into sh as they are, so it gets the
// refusal instead.
static void handleInstall() {
  static const char TYPE[] = "text/plain; charset=utf-8";
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("Connection", "close");
  if (!hostAllowed() || crossSite()) {
    server.send(403, TYPE, installRefusalScript(INSTALL_BAD_LINK));
    return;
  }
  if (panelLocked()) {
    server.send(503, TYPE, installRefusalScript(INSTALL_LOCKED));
    return;
  }
  const String k = server.arg("k");
  if (!authHookTokenIs(k.c_str(), k.length())) {
    authLogInstallRefused();
    server.send(403, TYPE, installRefusalScript(INSTALL_BAD_LINK));
    return;
  }
  if (server.header("Accept-Encoding").indexOf("gzip") < 0) {
    server.send(406, TYPE, installRefusalScript(INSTALL_BAD_LINK));
    return;
  }
  // The gzip head, then the prelude in a stored block, in one buffer
  uint8_t head[INSTALL_GZIP_HEAD_SIZE + INSTALL_PRELUDE_SIZE];
  char *prelude = (char *)head + INSTALL_GZIP_HEAD_SIZE;
  const int n = installPrelude(prelude, INSTALL_PRELUDE_SIZE, authHookToken(),
                               WiFi.localIP().toString().c_str(), panelMdnsName());
  if (n < 0) {   // no address yet, or a name the prelude cannot carry
    server.send(503, TYPE, installRefusalScript(INSTALL_BAD_LINK));
    return;
  }
  installGzipHead(head, n);
  uint8_t tail[INSTALL_GZIP_TAIL_SIZE];
  installGzipTail(tail, prelude, n, INSTALL_SH_CRC, INSTALL_SH_LEN);
  // The script stays in flash: sent from there in blocks, never copied
  server.sendHeader("Content-Encoding", "gzip");
  server.setContentLength(INSTALL_GZIP_HEAD_SIZE + n + INSTALL_SH_GZ_LEN + INSTALL_GZIP_TAIL_SIZE);
  server.send(200, TYPE, "");
  server.sendContent((const char *)head, INSTALL_GZIP_HEAD_SIZE + n);
  for (size_t at = 0; at < INSTALL_SH_GZ_LEN; at += 2048)
    server.sendContent_P((const char *)INSTALL_SH_GZ + at, min((size_t)2048, INSTALL_SH_GZ_LEN - at));
  server.sendContent((const char *)tail, sizeof(tail));
}

// Identity, for discovery: clients that cannot use mDNS (multicast filtered
// by the router) find the panel by asking every address of the LAN for /id.
// With ?c=<nonce>, the hook asks it to prove it holds the hook token, bound
// to the address the request reached (the station's, or the access point's).
static void handleId() {
  char body[ID_JSON_SIZE];
  char proof[33];
  const bool proved = authHookProof(server.arg("c").c_str(),
                                    server.client().localIP().toString().c_str(), proof);
  panelIdJson(body, sizeof(body), FIRMWARE_VERSION, WiFi.macAddress().c_str(), resetReason(),
              (unsigned long)(millis() / 1000), proved ? proof : nullptr);
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", body);
}

// ===========================================================================
// SIGN-IN: the settings are for whoever can read the panel (auth.h)
// ===========================================================================

// DNS rebinding: a page whose own name is later pointed at the panel's IP
// would talk to it as to its own site. Only the panel's real names are served.
static bool hostAllowed() {
  String h = server.hostHeader();
  const int colon = h.indexOf(':');
  if (colon >= 0) h = h.substring(0, colon);
  return h == WiFi.localIP().toString() || h == WiFi.softAPIP().toString() ||
         panelNameIsOurs(h.c_str());
}

// A page on another site can make the visitor's browser send requests to
// the panel: posting wrong codes until sign-in locks, or popping codes up on
// the panel. Browsers say where a request comes from (Origin on POST,
// Sec-Fetch-Site on everything); curl and the hook script say nothing and
// stay welcome.
static bool crossSite() {
  if (server.header("Sec-Fetch-Site") == "cross-site") return true;
  const String origin = server.header("Origin");
  if (!origin.length()) return false;
  return origin != "http://" + server.hostHeader();
}

// wrong: the answer to a wrong code. The page goes back to the page asked
// for after sign-in: the address of this request, or the form's next.
static void sendSignIn(bool wrong) {
  if (!crossSite()) pairingRequest();           // the panel shows the code
  const String next = wrong ? server.arg("next") : server.uri();
  // On the setup access point the brand row says so, as on the Wi-Fi page
  const Shell sh = {&tr(), nullptr, wifiSetupMode ? nullptr : panelHostName(), TAB_NONE, false,
                    FIRMWARE_VERSION};
  String s;
  s.reserve(ACCESS_PAGE_RESERVE);
  signInPage(s, sh, SignInView{wrong, next.c_str()});
  server.sendHeader("Connection", "close");
  server.send(200, "text/html; charset=utf-8", s);
}

// An unfinished factory reset: every page and browser route answers this,
// and nothing is read or changed. 503: the panel is not serving its pages.
static void sendLocked() {
  // On the setup access point the brand row says so, as on sign-in
  const Shell sh = {&tr(), nullptr, wifiSetupMode ? nullptr : panelHostName(), TAB_NONE, false,
                    FIRMWARE_VERSION};
  String s;
  s.reserve(ACCESS_PAGE_RESERVE);
  lockedPage(s, sh, panelLock() == LOCK_KNOB);
  server.send(503, "text/html; charset=utf-8", s);
}

// The shell's tabs only for a visitor signed in; the brand row as on
// sign-in. Too little memory even for this page: the low-memory line.
static void sendError(int code, Tab back) {
  const bool in = hostAllowed() && authCookieValid(server.header("Cookie")) && !panelLocked();
  const Shell sh = {&tr(), nullptr, wifiSetupMode ? nullptr : panelHostName(), TAB_NONE, in,
                    FIRMWARE_VERSION};
  String s;
  if (!s.reserve(ERROR_PAGE_RESERVE)) {
    server.send(code, "text/html; charset=utf-8", tr().webOutOfMemory);
    return;
  }
  errorPage(s, sh, code, back);
  server.send(code, "text/html; charset=utf-8", s);
}

// Every page that shows or changes settings starts with this
static bool signedIn() {
  if (!hostAllowed() || (server.method() == HTTP_POST && crossSite())) {
    sendError(403);
    return false;
  }
  if (authCookieValid(server.header("Cookie"))) return true;
  sendSignIn(false);
  return false;
}

// Every page and browser route that shows or changes settings starts with
// this: the visitor first (host, site, session), then the lock
static bool pageGuard() {
  if (!signedIn()) return false;
  if (!panelLocked()) return true;
  sendLocked();
  return false;
}

static void handleLogin() {
  if (!hostAllowed() || crossSite()) { sendError(403); return; }
  if (!pairingCheck(server.arg("code"))) { sendSignIn(true); return; }
  server.sendHeader("Set-Cookie", authSetCookie());
  server.sendHeader("Location", returnPath(server.arg("next").c_str()));
  server.send(303, "text/plain", "");
}

// ===========================================================================
// LOCAL API: docs/api.md. JSON in (or form / query parameters), JSON out.
// Signed in by "Authorization: Bearer <token>" (token on Panel,
// or "t" on the serial console), or by the browser's sign-in cookie.
// ===========================================================================

static void apiReply(int code, const String &json) {
  server.sendHeader("Connection", "close");
  server.send(code, "application/json", json);
}

static void apiError(int code, const char *message) {
  apiReply(code, String("{\"error\":\"") + message + "\"}");
}

static bool apiAllowed() {
  if (!hostAllowed() || crossSite()) { apiError(403, "refused"); return false; }
  if (authBearerValid(server.header("Authorization")) || authCookieValid(server.header("Cookie")))
    return true;
  apiError(401, "missing or wrong token: Authorization: Bearer <token>");
  return false;
}

// A parameter from the query string or a form, else from a JSON body
static bool apiArg(const char *name, char *out, size_t size) {
  if (server.hasArg(name)) {
    strncpy(out, server.arg(name).c_str(), size - 1);
    out[size - 1] = '\0';
    return true;
  }
  return server.hasArg("plain") && jsonValue(server.arg("plain").c_str(), name, out, size);
}


static void apiInfo() {
  if (!apiAllowed()) return;
  String j;
  j.reserve(2300);   // 24 screens with every field, the order, the timing and the region
  char head[INFO_HEAD_SIZE];
  panelInfoHead(head, sizeof(head), FIRMWARE_VERSION);
  j += head;
  j += F(",\"screen\":\"");
  j += galleryCurrentScreen();
  j += F("\",\"orientation\":\"");
  j += portrait ? F("portrait") : F("landscape");
  j += F("\",\"brightness\":");
  j += galleryBrightness();
  j += F(",\"brightness_levels\":");
  j += galleryBrightnessLevels();
  regionInfoJson(j, settings.tempUnit, settings.currency, settings.clock12);
  ScreenList l;
  uint16_t rotateSec;
  galleryList(&l, &rotateSec);
  screensInfoJson(j, l, rotateSec, galleryRotateNextIn(), portrait);
  j += '}';
  apiReply(200, j);
}

static void apiScreen() {
  if (!apiAllowed()) return;
  char name[24];
  if (!apiArg("screen", name, sizeof(name))) { apiError(400, "screen missing"); return; }
  ScreenList l;
  uint16_t rotateSec;
  galleryList(&l, &rotateSec);
  // The loop checks again when it applies it: a save can land in between
  const int status = screenRequestStatus(l, name, portrait);
  if (status == 409) { apiError(409, "screen is off, see /api/info"); return; }
  if (status != 200 || !galleryShowScreen(name)) {
    apiError(404, "no such screen in this orientation, see /api/info");
    return;
  }
  requestShown(REQUEST_COMMAND, SOURCE_API, name);
  apiReply(200, F("{\"ok\":true}"));
}

static void apiBrightness() {
  if (!apiAllowed()) return;
  char level[8];
  if (!apiArg("level", level, sizeof(level))) { apiError(400, "level missing"); return; }
  gallerySetBrightness(atoi(level));
  requestShown(REQUEST_COMMAND, SOURCE_API, level);
  apiReply(200, F("{\"ok\":true}"));
}

static void apiNotify() {
  if (!apiAllowed()) return;
  char text[160], color[12] = "", seconds[8] = "";
  if (!apiArg("text", text, sizeof(text))) { apiError(400, "text missing"); return; }
  apiArg("color", color, sizeof(color));
  apiArg("seconds", seconds, sizeof(seconds));
  galleryNotify(text, parseColor(color), seconds[0] ? atoi(seconds) : 10);
  apiReply(200, F("{\"ok\":true}"));
}

static void apiText() {
  if (!apiAllowed()) return;
  char text[160];
  if (!apiArg("text", text, sizeof(text))) { apiError(400, "text missing"); return; }
  if (panelLocked()) { apiError(503, "panel locked: factory reset unfinished"); return; }
  toMessageText(text, settings.message, sizeof(settings.message));
  if (!saveSettings()) { apiError(500, "message not saved"); return; }
  apiReply(200, F("{\"ok\":true}"));
}

// ===========================================================================
// IMAGES: upload, delete, show (content.h). The upload is a RequestHandler
// rather than server.on(): WebServer calls it per part as the body streams
// in, so the sign-in and the announced size are checked before the first
// byte reaches the flash, and the body is never held in RAM.
// ===========================================================================

// A refusal goes out with "Connection: close" (WebServer sends it on every
// answer), what the client is still sending is read and dropped for at most
// this long, then the socket is closed: curl and browsers read the status
// instead of a connection reset. Bounded so a refused client cannot hold the
// web task.
static const uint32_t LINGER_MS = 500;

static void lingeringClose() {
  WiFiClient &c = server.currentClient();
  uint8_t sink[256];
  const uint32_t from = millis();
  while (c.connected() && millis() - from < LINGER_MS) {
    if (c.available() > 0) c.read(sink, sizeof(sink));
    else vTaskDelay(1);
  }
  c.stop();   // the parser's next read fails: the request ends there
}

static ImagesNotice bareNotice(UploadResult r) {
  return {r, nullptr, 0, 0, {KIND_OTHER, 0, 0}, nullptr, false};
}

// The Images page (web_images.cpp builds it), sent in pieces. The generation
// is read after the list: a change in between gives the old list new
// addresses, never a replaced file an address the browser has kept. When
// it cannot be built, an upload still answers with its own status and
// result, never "busy" after a stored file.
static void sendImagesPage(int code, const ImagesNotice *notice) {
  const bool storage = contentAvailable();
  std::unique_ptr<ImageInfo[]> list;
  const int n = storage ? imageList(list) : 0;
  String s;
  if (n < 0 || !s.reserve(4096)) {
    if (notice) server.send(code, "text/plain", uploadText(notice->result));
    else        sendError(503, TAB_IMAGES);
    return;
  }
  char choice[IMAGE_NAME_MAX + 1];
  contentChoice(choice, sizeof(choice));
  const ImagesPageView v = {storage, list.get(), n, contentSpace(), choice, notice,
                            contentGeneration()};
  const Shell sh = {&tr(), nullptr, panelHostName(), TAB_IMAGES, true, FIRMWARE_VERSION};
  startPage(code);
  imagesPage(s, sh, v, sendAndClear);
  sendChunk(s);
  // The last chunk now, not when the handler returns: an upload refused
  // midway answers from inside the body's parser, and the connection is
  // closed right after.
  server.sendContent("");
}

// The page's answer to an upload: the Images page saying what happened,
// with the upload's status. A locked panel answers the locked page, as
// every other page and browser route does.
static void pageUploadDone(const ImagesNotice &n) {
  if (panelLocked()) { sendLocked(); return; }
  sendImagesPage(uploadStatus(n.result), &n);
}

class ImageUploadHandler : public RequestHandler {
public:
  // api: /api/images (token or cookie, JSON answers) rather than the page
  // (cookie, the page). done answers once the body is in.
  ImageUploadHandler(const char *uri, bool api, void (*done)(const ImagesNotice &))
      : uri_(uri), api_(api), done_(done) {}

  bool canHandle(HTTPMethod method, String uri) override {
    if (method != HTTP_POST || uri != uri_) return false;
    // Called once per request, before its body: a new request starts here
    mode_ = FRESH;
    parts_.reset();
    seen_ = 0;
    started_ = false;
    result_ = UP_PENDING;
    imageOff_ = false;
    return true;
  }
  bool canUpload(String uri) override { return uri == uri_; }
  bool canRaw(String uri) override { return uri == uri_; }

  void upload(WebServer &, String, HTTPUpload &u) override {
    if (mode_ == ANSWERED) return;
    switch (u.status) {
      case UPLOAD_FILE_START:
        parts_.next();
        if (!parts_.first()) return;         // only the first file part is stored
        gateFirstPart(u.filename.c_str());
        return;
      case UPLOAD_FILE_WRITE:
        // The parser reads up to the part's boundary whatever the request
        // announced: a body longer than it said is cut here.
        seen_ += u.currentSize;
        if (seen_ > (mode_ == DRAINING ? PAGE_DRAIN_MAX : MAX_IMAGE_BYTES + MULTIPART_SLACK)) {
          if (mode_ == STORING) contentUploadAbort();
          result_ = UP_TOO_BIG;
          started_ = false;                  // nothing of it to name
          answerAndClose();
          return;
        }
        if (parts_.first() && mode_ == STORING) contentUploadWrite(u.buf, u.currentSize);
        return;
      case UPLOAD_FILE_END:
        if (parts_.first() && mode_ == STORING) result_ = contentUploadFinish();
        return;
      case UPLOAD_FILE_ABORTED:              // client gone: handle() will not run
        if (mode_ == STORING) contentUploadAbort();
        mode_ = ANSWERED;
        return;
    }
  }

  // Not multipart: refused unread. server.upload() is a null pointer on
  // this path, so nothing here touches it.
  void raw(WebServer &, String, HTTPRaw &r) override {
    if (r.status != RAW_START || mode_ != FRESH) return;
    GateVerdict v = gate();
    // The format is told only to the owner (host, site and sign-in first);
    // without a page to show, an oversized raw body is simply closed.
    if (v == GATE_ACCEPT) v = GATE_CLOSE_400;
    if (v == GATE_DRAIN_413) v = GATE_CLOSE_413;
    answerGate(v);
    lingeringClose();
    // WebServer reads the body until totalSize reaches the announced
    // length: there is nothing left to read.
    r.totalSize = SIZE_MAX;
    mode_ = ANSWERED;
  }

  bool handle(WebServer &, HTTPMethod, String) override {
    if (mode_ == ANSWERED) return true;
    if (mode_ == FRESH) {                    // a multipart body without a file part
      const GateVerdict v = gate();
      if (v != GATE_ACCEPT && v != GATE_DRAIN_413) { answerGate(v); return true; }
      result_ = v == GATE_DRAIN_413 ? UP_TOO_BIG : UP_NO_FILE;
    }
    if (result_ == UP_PENDING) {             // the part never ended: nothing kept
      contentUploadAbort();
      result_ = UP_FAILED;
    }
    const ImageUpload &up = contentUpload();
    // A page upload, or an API one asking for it, drawn for this
    // orientation, is shown at once and first, unless IMAGE is off in the
    // owner's list: then it is stored and never shown, and the page says so
    // (the API's answer does not change)
    const bool imageOn = galleryScreenOn("IMAGE");
    const bool stored = result_ == UP_ADDED || result_ == UP_REPLACED;
    if (stored &&
        showsAfterUpload(!api_, api_ && server.arg("show") == "1", up.portrait(), portrait,
                         imageOn)) {
      contentShowFirst(up.name());
      galleryShowScreen("IMAGE");
      requestShown(REQUEST_IMAGE, api_ ? SOURCE_API : SOURCE_WEB, up.name());
    }
    imageOff_ = stored && !api_ && !imageOn;
    done_(notice());
    return true;
  }

private:
  enum Mode : uint8_t { FRESH, STORING, DRAINING, ANSWERED };

  GateVerdict gate() const {
    const int announced = server.clientContentLength();
    GateInput in;
    in.api = api_;
    in.hostOk = hostAllowed();
    in.crossSite = crossSite();
    in.bearerOk = api_ && authBearerValid(server.header("Authorization"));
    in.cookieOk = authCookieValid(server.header("Cookie"));
    in.announced = announced > 0 ? (uint32_t)announced : 0;
    return uploadGate(in);
  }

  // At the first file part, before contentUploadStart touches the flash
  void gateFirstPart(const char *filename) {
    const GateVerdict v = gate();
    if (v == GATE_ACCEPT && panelLocked()) {
      // The owner is told the panel is locked; storage is never touched
      result_ = UP_NO_STORAGE;
      answerAndClose();
    } else if (v == GATE_ACCEPT) {
      const int announced = server.clientContentLength();
      result_ = contentUploadStart(filename, announced > 0 ? (uint32_t)announced : 0);
      started_ = true;
      mode_ = STORING;
    } else if (v == GATE_DRAIN_413) {
      // The page reads the body and drops it, so the browser shows the
      // refusal instead of a connection error
      result_ = UP_TOO_BIG;
      mode_ = DRAINING;
    } else {
      answerGate(v);
      lingeringClose();
      mode_ = ANSWERED;
    }
  }

  ImagesNotice notice() const {
    if (!started_) return bareNotice(result_);
    const ImageUpload &up = contentUpload();
    return {result_, up.sent(), up.received(), up.left(), up.header(), up.name(), imageOff_};
  }

  // Answers from inside the body's parser: the previous request's
  // Content-Length setting is still there (WebServer resets it only before
  // handle()), and a chunked one would garble a plain answer.
  void answerAndClose() {
    server.setContentLength(CONTENT_LENGTH_NOT_SET);
    done_(notice());
    lingeringClose();
    mode_ = ANSWERED;
  }

  void answerGate(GateVerdict v) {
    server.setContentLength(CONTENT_LENGTH_NOT_SET);
    Serial.printf("[WEB] image upload refused before its body (%d)\n", (int)v);
    switch (v) {
      case GATE_SIGN_IN:   sendSignIn(false); break;
      case GATE_CLOSE_401: apiError(401, "missing or wrong token: Authorization: Bearer <token>"); break;
      case GATE_CLOSE_413: result_ = UP_TOO_BIG; started_ = false; done_(notice()); break;
      case GATE_CLOSE_400:
        if (api_) apiError(400, "send one GIF as multipart/form-data: curl -F file=@image.gif");
        else      sendError(400, TAB_IMAGES);
        break;
      default:   // GATE_CLOSE_403
        if (api_) apiError(403, "refused");
        else      sendError(403, TAB_IMAGES);
        break;
    }
  }

  const char *const uri_;
  const bool api_;
  void (*const done_)(const ImagesNotice &);
  Mode mode_ = FRESH;
  FileParts parts_;
  uint32_t seen_ = 0;       // file bytes of every part so far
  bool started_ = false;    // contentUploadStart ran: contentUpload() is this request's
  UploadResult result_ = UP_PENDING;
  bool imageOff_ = false;   // a page upload stored while IMAGE is off: the notice says it
};

static ImageUploadHandler pageUpload("/images", false, pageUploadDone);

// --- The same, for scripts: JSON in docs/api.md ------------------------------

static const char LOCKED[] = "panel locked: factory reset unfinished";

// /api/text's answer on a locked panel: storage is never touched
static bool apiUnlocked() {
  if (!panelLocked()) return true;
  apiError(503, LOCKED);
  return false;
}

static const char *kindName(GifKind k) {
  switch (k) {
    case KIND_PNG:  return "PNG";
    case KIND_JPEG: return "JPEG";
    case KIND_WEBP: return "WebP";
    default:        return nullptr;
  }
}

// The API's answer to an upload. Names in it are stored names: letters,
// digits, '-', '_' and '.' only, nothing to escape.
static void apiUploadDone(const ImagesNotice &n) {
  if (panelLocked()) { apiError(503, LOCKED); return; }
  const int code = uploadStatus(n.result);
  char j[160];
  switch (n.result) {
    case UP_ADDED: case UP_REPLACED:
      snprintf(j, sizeof(j),
               "{\"ok\":true,\"name\":\"%s\",\"replaced\":%s,\"orientation\":\"%s\",\"size\":%lu}",
               n.name, n.result == UP_REPLACED ? "true" : "false",
               n.header.height > n.header.width ? "portrait" : "landscape", (unsigned long)n.needed);
      break;
    case UP_NOT_GIF: {
      const char *kind = kindName(n.header.kind);
      if (kind) snprintf(j, sizeof(j), "{\"error\":\"not a GIF: %s\"}", kind);
      else      snprintf(j, sizeof(j), "{\"error\":\"not a GIF\"}");
      break;
    }
    case UP_WRONG_SIZE:
      snprintf(j, sizeof(j), "{\"error\":\"wrong size: %ux%u, the panel takes 64x32 or 32x64\"}",
               n.header.width, n.header.height);
      break;
    case UP_NO_ROOM:
      snprintf(j, sizeof(j), "{\"error\":\"no room\",\"needed\":%lu,\"left\":%lu}",
               (unsigned long)n.needed, (unsigned long)n.left);
      break;
    case UP_TOO_MANY:   apiError(code, "24 images already: delete one first"); return;
    case UP_TOO_BIG:    apiError(code, "over 96 KB"); return;
    case UP_NO_STORAGE: apiError(code, "no storage area"); return;
    case UP_NO_FILE:    apiError(code, "send one GIF as multipart/form-data: curl -F file=@image.gif"); return;
    default:            apiError(code, "upload failed: nothing kept"); return;   // write error, cut short
  }
  apiReply(code, j);
}

static ImageUploadHandler apiUpload("/api/images", true, apiUploadDone);

// The list, on the heap as for the page (~1.2 KB for 24)
static int imageList(std::unique_ptr<ImageInfo[]> &list) {
  list.reset(new (std::nothrow) ImageInfo[MAX_IMAGES]);
  if (!list) return -1;
  const int n = contentList(list.get(), MAX_IMAGES);
  return n < 0 ? 0 : n;
}

// Whether name is stored: a refused delete or choice of a stored image is the
// flash's or the NVS's failure (500), not an unknown name (404)
static bool imageStored(const char *name) {
  std::unique_ptr<ImageInfo[]> list;
  const int n = imageList(list);
  for (int i = 0; i < n; i++)
    if (strcmp(list[i].name, name) == 0) return true;
  return false;
}

static void apiImages() {
  if (!apiAllowed() || !apiUnlocked()) return;
  const bool storage = contentAvailable();
  std::unique_ptr<ImageInfo[]> list;
  const int n = storage ? imageList(list) : 0;
  if (n < 0) { apiError(500, "out of memory"); return; }
  char choice[IMAGE_NAME_MAX + 1];
  contentChoice(choice, sizeof(choice));
  String j;
  if (!j.reserve(storage ? 200 + n * (IMAGE_NAME_MAX + 60) : 40)) { apiError(500, "out of memory"); return; }
  imagesJson(j, storage, list.get(), n, contentSpace(), choice);
  apiReply(storage ? 200 : 503, j);
}

static void apiImageDelete() {
  if (!apiAllowed() || !apiUnlocked()) return;
  if (!contentAvailable()) { apiError(503, "no storage area"); return; }
  char name[IMAGE_NAME_MAX + 2];
  if (!apiArg("name", name, sizeof(name)) || !name[0]) { apiError(400, "name missing"); return; }
  if (!imageStored(name)) { apiError(404, "no such image, see /api/images"); return; }
  if (!contentDelete(name)) { apiError(500, "not deleted"); return; }
  apiReply(200, F("{\"ok\":true}"));
}

// Name, or "all" (as the name or a parameter of its own) for all in
// turn; no image is called "all", every name ends in .gif
static void apiImageShow() {
  if (!apiAllowed() || !apiUnlocked()) return;
  if (!contentAvailable()) { apiError(503, "no storage area"); return; }
  char name[IMAGE_NAME_MAX + 2] = "", all[8];
  const bool named = apiArg("name", name, sizeof(name)) && name[0];
  const bool toAll = named ? strcmp(name, "all") == 0 : apiArg("all", all, sizeof(all));
  if (!named && !toAll) { apiError(400, "name missing: an image name, or all"); return; }
  if (!toAll && !imageStored(name)) { apiError(404, "no such image, see /api/images"); return; }
  if (!contentChoose(toAll ? "" : name)) { apiError(500, "choice not saved"); return; }
  galleryShowScreen("IMAGE");
  requestShown(REQUEST_IMAGE, SOURCE_API, toAll ? "all" : name);
  apiReply(200, F("{\"ok\":true}"));
}

// An unknown name changes nothing; the list shows as it is
static void handleImageDelete() {
  if (!pageGuard()) return;
  contentDelete(server.arg("n").c_str());
  server.sendHeader("Location", "/images");
  server.send(303, "text/plain", "");
}

// The choice is saved and the panel switches to IMAGE; '' = all in turn
static void handleImageShow() {
  if (!pageGuard()) return;
  const String sh = server.arg("sh");
  if (contentChoose(sh.c_str())) {
    galleryShowScreen("IMAGE");
    requestShown(REQUEST_IMAGE, SOURCE_WEB, sh.c_str());
  }
  server.sendHeader("Location", "/images");
  server.send(303, "text/plain", "");
}

// contentFileSend's writer: the headers go with the first block, so a file
// that cannot be read still answers 404. ctx: whether they went.
static bool sendImageBlock(const uint8_t *data, size_t n, void *ctx) {
  bool &started = *static_cast<bool *>(ctx);
  if (!started) {
    started = true;
    // Never run as anything but a picture; kept a year, as its address
    // changes with every add, replace and delete, and at every start
    server.sendHeader("Content-Security-Policy", "sandbox");
    server.sendHeader("Cache-Control", "private, max-age=31536000, immutable");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "image/gif", "");
  }
  server.sendContent((const char *)data, n);
  return server.currentClient().connected();
}

// GET /images/f?n=<name>&g=<generation>: a thumbnail, the stored file
// itself. Never signedIn(): its sign-in page would put a code on the panel.
// Only a name the index holds, byte for byte (nothing after a NUL, nothing
// longer than a name); g only makes the address new.
static void handleImageFile() {
  server.sendHeader("X-Content-Type-Options", "nosniff");
  switch (readGate(hostAllowed(), crossSite(), authCookieValid(server.header("Cookie")),
                   panelLocked(), contentAvailable())) {
    case READ_FORBIDDEN:    server.send(403, "text/plain", "refused"); return;
    case READ_UNAUTHORIZED: server.send(401, "text/plain", "unauthorized"); return;
    case READ_UNAVAILABLE:  server.send(503, "text/plain", "unavailable"); return;
    case READ_OK:           break;
  }
  const String name = server.arg("n");
  bool started = false;
  const bool exact = name.length() <= IMAGE_NAME_MAX && strlen(name.c_str()) == name.length();
  const bool whole = exact && contentFileSend(name.c_str(), sendImageBlock, &started);
  if (!started) server.send(404, "text/plain", "not found");
  else if (whole) server.sendContent("");
  else server.abortChunked();   // cut short: no last chunk, so no browser keeps it
}

// ===========================================================================
// STAGE: what the panel shows, its running order and the coding sessions
// ===========================================================================

static_assert(PANEL_RES_X == PREVIEW_W && PANEL_RES_Y == PREVIEW_H, "the preview is the panel");

// The gallery's state as the API reads it, and a copy of the sessions: the
// table is read from this core while loop() writes it, at worst a row one
// frame old (as the API's info). code, notice: an action's answer; name:
// the screen a Show notice names.
// What the gallery shows and plays next, and its level: Stage's block and
// /preview's headers
static void stageState(StageView &v) {
  galleryList(&v.list, &v.rotateSec);
  const int current = screenFind(galleryCurrentScreen());
  v.current = current < 0 ? 0 : (uint8_t)current;
  v.portrait = portrait;
  v.nextIn = galleryRotateNextIn();
  // As the gallery's rotation skips it
  const int image = screenFind("IMAGE");
  v.cannotRotate = image >= 0 && !contentPlayable(portrait) ? 1u << image : 0;
  v.level = galleryBrightness();
  v.levels = galleryBrightnessLevels();
}

static void sendStage(int code = 200, StageNotice notice = STAGE_NO_NOTICE,
                      const char *name = nullptr) {
  const uint32_t now = millis();
  // 6 x 24 bytes on the heap, freed on return: the page is built a few
  // frames deeper on the web task's stack
  std::unique_ptr<SessionRow[]> rows(new (std::nothrow) SessionRow[MAX_SESSIONS]);
  if (!rows) { sendError(503); return; }
  int count = 0;
  for (int r = 0; r < MAX_SESSIONS && count < MAX_SESSIONS; r++) {
    const int i = nthActiveSession(r);
    if (i < 0) break;
    SessionRow &row = rows[count++];
    memcpy(row.name, sessions[i].name, sizeof(row.name));
    row.name[sizeof(row.name) - 1] = '\0';
    memcpy(row.tint, SESSION_TINTS[sessions[i].tint % TINT_COUNT], sizeof(row.tint));
    row.state = sessions[i].state;
    // lastSeen may be newer than now (loop() wrote it meanwhile): age 0
    const uint32_t seen = sessions[i].lastSeen;
    row.ageSec = (int32_t)(now - seen) < 0 ? 0 : (now - seen) / 1000UL;
  }
  StageView v = {};
  stageState(v);
  v.sessions = rows.get();
  v.sessionCount = count;
  v.nightStart = settings.nightStart;
  v.nightEnd = settings.nightEnd;
  v.nightBrightness = settings.nightBrightness;
  v.message = settings.message;
  v.notice = notice;
  v.noticeName = name;
  const Shell sh = {&tr(), nullptr, panelHostName(), TAB_STAGE, true, FIRMWARE_VERSION};
  String s;
  if (!s.reserve(4096)) { sendError(503); return; }
  startPage(code);
  stagePage(s, sh, v, sendAndClear);
  sendChunk(s);
  server.sendContent("");
}

// --- Stage's actions and the Wi-Fi form: one capped handler ----------------

static void redirectToStage() {
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

// Show: the screen named, as /api/screen shows it (the 30 s hold included).
// Read one byte past the longest name, so a longer one stays unknown.
static void formShow(const FormBody &body) {
  char name[25];
  body.value("s", name, sizeof(name));   // absent: empty, so unknown
  ScreenList l;
  uint16_t rotateSec;
  galleryList(&l, &rotateSec);
  ShowOutcome o = showOutcome(l, name, portrait);
  // The loop checks again when it applies it: a save can land in between
  if (o == SHOW_OK && galleryShowScreen(name)) {
    requestShown(REQUEST_COMMAND, SOURCE_WEB, name);
    redirectToStage();
    return;
  }
  if (o == SHOW_OK) o = SHOW_UNKNOWN;
  const int i = screenFind(name);
  const char *shown = i < 0 ? nullptr : tr().*SCREEN_INFO[i].name;
  if (o == SHOW_OFF)        sendStage(409, STAGE_SHOW_OFF, shown);
  else if (o == SHOW_OTHER) sendStage(404, STAGE_SHOW_OTHER, shown);
  else                      sendStage(404, STAGE_SHOW_UNKNOWN);
}

// Dimmer and Brighter post the step they lead to; anything else changes
// nothing. Stored 3 s later, as the API's.
static void formBrightness(const FormBody &body) {
  char text[8];
  const int n = body.value("b", text, sizeof(text));
  int level;
  if (n < 0 || n >= (int)sizeof(text) || !formLevel(text, galleryBrightnessLevels(), &level)) {
    sendStage(400);
    return;
  }
  gallerySetBrightness(level);
  requestShown(REQUEST_COMMAND, SOURCE_WEB, text);
  redirectToStage();
}

// Send: stored as the Running order field stores it, through the same
// guarded copy, then shown when TEXT is on and fits
static void formText(const FormBody &body) {
  std::unique_ptr<char[]> typed(new (std::nothrow) char[FORM_BODY_MAX + 1]);
  if (!typed) { sendError(503); return; }
  if (body.value("msg", typed.get(), FORM_BODY_MAX + 1) < 0) { sendStage(400); return; }
  before = settings;
  panelMessage(typed.get(), settings.message, sizeof(settings.message));
  const bool saved = saveSettings(&before);   // refused: the message put back
  wipeSecret(&before, sizeof(before));
  if (!saved) { sendStage(500, STAGE_TEXT_FAILED); return; }
  const int text = screenFind("TEXT");
  const StageNotice why = textNotice(galleryScreenOn("TEXT"),
                                     text >= 0 && screenFits((uint8_t)text, portrait));
  if (why == STAGE_NO_NOTICE && galleryShowScreen("TEXT")) {
    requestShown(REQUEST_COMMAND, SOURCE_WEB, "TEXT");
    redirectToStage();
    return;
  }
  sendStage(200, why == STAGE_NO_NOTICE ? STAGE_TEXT_OFF : why);
}

// POST /show, /brightness, /text and /wifi. The verdict comes before one
// byte of body is read (formGate); then at most FORM_BODY_MAX bytes are,
// into the heap, and the route answers from inside the parser. The library
// reads urlencoded bodies whole into RAM, in blocks it waits to fill: this
// handler takes them raw instead. A multipart body is parsed by the library
// before any handler runs, on every path: refused here once the gate passes.
class FormRoutes : public RequestHandler {
public:
  bool canHandle(HTTPMethod method, String uri) override {
    route_ = method == HTTP_POST ? find(uri) : -1;
    answered_ = false;   // once per request, before its body
    return route_ >= 0;
  }
  bool canUpload(String) override { return false; }
  bool canRaw(String uri) override { return find(uri) >= 0; }

  void raw(WebServer &, String, HTTPRaw &r) override {
    if (r.status != RAW_START || answered_) return;
    answered_ = true;
    // The previous request's length setting is still there (the library
    // resets it only before handle()), and a chunked one would garble a
    // plain answer
    server.setContentLength(CONTENT_LENGTH_NOT_SET);
    // Read once, signed: the gate refuses a negative length, which as a
    // size would wrap the buffer below to nothing
    const int announced = server.clientContentLength();
    if (!gate(announced)) {
      lingeringClose();
      r.totalSize = SIZE_MAX;   // nothing left for the library to read
      return;
    }
    const size_t total = (size_t)announced;   // 0 .. FORM_BODY_MAX, by the gate
    std::unique_ptr<FormBody> body(new (std::nothrow) FormBody);
    std::unique_ptr<char[]> data(new (std::nothrow) char[total + 1]);
    const bool allocated = body && data;   // data is freed below, before the answer
    const size_t read = allocated ? readBody(data.get(), total) : 0;
    if (allocated) {
      if (read == total) body->set(data.get(), read);
      // It may hold the typed Wi-Fi password: wiped before the heap gets it
      // back (FormBody wipes its own copy)
      wipeSecret(data.get(), total + 1);
      data.reset();
    }
    if (!allocated) {
      sendError(503);
    } else if (read < total) {   // stopped before its announced length
      sendError(400);
    } else {
      switch (route_) {
        case 0:  formShow(*body); break;
        case 1:  formBrightness(*body); break;
        case 2:  formText(*body); break;
        default: handleWifi(*body); break;
      }
      r.totalSize = read;
      return;
    }
    lingeringClose();
    r.totalSize = SIZE_MAX;
  }

  bool handle(WebServer &, HTTPMethod, String) override {
    if (answered_) return true;
    // Only a multipart body gets here: the library read it, not this handler
    if (gate(server.clientContentLength()))
      sendError(415);
    return true;
  }

private:
  static int find(const String &uri) {
    static const char *const ROUTES[] = {"/show", "/brightness", "/text", "/wifi"};
    for (int i = 0; i < 4; i++)
      if (uri == ROUTES[i]) return i;
    return -1;
  }

  // The settings pages' checks, in their order: false once answered
  bool gate(int announced) const {
    const FormInput in = {hostAllowed(), crossSite(), authCookieValid(server.header("Cookie")),
                          panelLocked(), (int32_t)announced};
    const FormVerdict v = formGate(in);
    if (v == FORM_ACCEPT) return true;
    Serial.printf("[WEB] form refused before its body (%d)\n", (int)v);
    switch (v) {
      case FORM_SIGN_IN:    sendSignIn(false); break;
      case FORM_BAD_LENGTH: sendError(400); break;
      case FORM_TOO_LARGE:  sendError(413); break;
      case FORM_LOCKED:     sendLocked(); break;
      default:              sendError(403); break;
    }
    return false;
  }

  // As the hook's body is read: what has arrived, yielding between packets,
  // until total or 1 s without a byte (or the client gone)
  static size_t readBody(char *out, size_t total) {
    WiFiClient &c = server.currentClient();
    size_t read = 0;
    uint32_t lastPacketAt = millis();
    while (read < total && millis() - lastPacketAt < 1000) {
      const int n = c.available() > 0 ? c.read((uint8_t *)out + read, total - read) : 0;
      if (n <= 0) {
        if (!c.connected()) break;
        vTaskDelay(1);
        continue;
      }
      read += n;
      lastPacketAt = millis();
    }
    return read;
  }

  int route_ = -1;
  bool answered_ = false;
};

static FormRoutes formRoutes;

// GET /preview: the frame the panel shows, as a BMP. Never signedIn(): its
// sign-in page would put a code on the panel. The code is checked before and
// after the copy, so a code that came up meanwhile is never sent. Served
// while locked: the panel then shows RESET FAILED, no secret. Its headers
// tell the state read with the frame (stageHeaders): Stage follows the panel
// from them, same origin, so no Access-Control-Expose-Headers.
static void handlePreview() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  switch (readGate(hostAllowed(), crossSite(), authCookieValid(server.header("Cookie")), false,
                   true)) {
    case READ_FORBIDDEN:    server.send(403, "text/plain", "refused"); return;
    case READ_UNAUTHORIZED: server.send(401, "text/plain", "unauthorized"); return;
    case READ_UNAVAILABLE:  server.send(503, "text/plain", "unavailable"); return;
    case READ_OK:           break;
  }
  const uint16_t *shown = displayShown();
  // 6 KB on the heap, freed on return: the web task's stack is 8 KB
  std::unique_ptr<uint8_t[]> bmp(new (std::nothrow) uint8_t[PREVIEW_BMP_SIZE]);
  if (!shown || !bmp) { server.send(503, "text/plain", "unavailable"); return; }
  const bool before = pairingCodeShown() != 0;
  previewBmp(shown, displayRotation(), before, bmp.get());
  if (!before && pairingCodeShown() != 0) previewBmp(nullptr, displayRotation(), true, bmp.get());
  StageView v = {};
  stageState(v);
  StageHeader h[STAGE_HEADER_MAX];
  const int n = stageHeaders(v, h);
  for (int k = 0; k < n; k++) server.sendHeader(h[k].name, h[k].value);
  server.send_P(200, "image/bmp", (const char *)bmp.get(), PREVIEW_BMP_SIZE);
}

// The stylesheet (compressed) and the poster sheet, in flash. Public: the
// sign-in, setup and locked pages need them before any session, and they
// hold no data. Any version segment gets today's file; only the running
// version's address may be kept, since a page cached from an older one must
// not pin a stale copy.
static void sendAsset(const char *type, const uint8_t *data, size_t len, bool gzip) {
  const bool current = server.pathArg(0) == FIRMWARE_VERSION;
  server.sendHeader("Cache-Control", current ? "public, max-age=31536000, immutable" : "no-cache");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  if (gzip) server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, type, (const char *)data, len);
}

// The city field's suggestions on WEATHER's page, while the owner types:
// GET /places?q=<2 to 32 bytes> -> [{"id":..,"n":..,"a":..,"c":..}], five at
// most, best first; 503 {"error":"busy"} while the table cannot be reached.
// Nothing changes. The web task waits for the HTTPS task's answer, at
// most PLACES_WAIT_MS, then answers 503 and the page asks again. On the
// heap for the request: off the web task's 8 KB stack, and out of RAM
// between searches (a forecast's handshake needs it all).
static void handlePlaces() {
  if (!pageGuard()) return;
  const String q = server.arg("q");
  if (!placesQueryOk(q.c_str(), q.length())) {
    apiReply(400, "{\"error\":\"query\"}");
    return;
  }
  char *json = (char *)malloc(PLACES_JSON_SIZE);
  if (!json || !placesSearch(q.c_str(), json, PLACES_JSON_SIZE)) {
    free(json);
    apiReply(503, "{\"error\":\"busy\"}");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");   // as a page: signed in
  server.send(200, "application/json", json);
  free(json);
}

// GET /place -> {"p":"Innsbruck, Tyrol, Austria"}, {"p":""} while the HTTPS
// task is still looking the city up, {"p":"","f":1} when it was not found:
// the Found row after a save asks it. A read of two settings: no table, no
// HTTPS.
static void handlePlace() {
  if (!pageGuard()) return;
  char place[sizeof(settings.place)];   // a copy: the HTTPS task writes it
  snprintf(place, sizeof(place), "%s", settings.place);
  char json[PLACE_JSON_SIZE];
  if (!placeJson(place, locateFailed, json, sizeof(json))) {
    apiReply(503, "{\"error\":\"busy\"}");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

static void handleStylesheet() { sendAsset("text/css", STYLESHEET_GZ, STYLESHEET_GZ_LEN, true); }
static void handlePosters() { sendAsset("image/png", POSTERS_PNG, POSTERS_PNG_LEN, false); }

void webBegin() {
  server.on("/id", HTTP_GET, handleId);
  // Before any page, setup mode included (the captive page links it)
  server.on(UriBraces("/s/{}/c.css"), HTTP_GET, handleStylesheet);
  server.on(UriBraces("/s/{}/p.png"), HTTP_GET, handlePosters);
  // Setup access point open: the home page becomes the Wi-Fi page
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/api/info",       HTTP_GET,  apiInfo);
  server.on("/api/screen",     HTTP_POST, apiScreen);
  server.on("/api/brightness", HTTP_POST, apiBrightness);
  server.on("/api/notify",     HTTP_POST, apiNotify);
  server.on("/api/text",       HTTP_POST, apiText);
  server.on("/api/images",     HTTP_GET,  apiImages);
  server.addHandler(&apiUpload);
  server.on("/api/images/delete", HTTP_POST, apiImageDelete);
  server.on("/api/images/show",   HTTP_POST, apiImageShow);
  server.on("/", []() {
    if (!pageGuard()) return;
    if (wifiSetupMode) { sendWifiPage(nullptr, nullptr, false, server.hasArg("scan")); return; }
    // The old address of IMAGE's block, from a page opened before the update
    if (server.hasArg("open")) {
      server.sendHeader("Location", "/images");
      server.send(303, "text/plain", "");
      return;
    }
    sendStage();
  });
  server.on("/preview", HTTP_GET, handlePreview);
  server.on("/images/f", HTTP_GET, handleImageFile);
  server.on("/panel", HTTP_GET, []() {
    if (pageGuard()) sendPanelPage(200, SAVE_NONE, settings, LIST_OK);
  });
  server.on("/panel/feedback", HTTP_GET, []() {
    if (pageGuard()) sendPanelPage(200, SAVE_NONE, settings, LIST_OK, true);
  });
  server.on("/screens", HTTP_GET, []() {
    if (pageGuard()) sendOrderPage(200, SAVE_NONE, settings, LIST_OK);
  });
  // A screen's own settings; any other slug is the house 404
  server.on(UriBraces("/screens/{}"), HTTP_GET, []() {
    if (!pageGuard()) return;
    const int screen = screenPageFind(server.pathArg(0).c_str());
    if (screen < 0) sendError(404, TAB_ORDER);
    else            sendOrderPage(200, SAVE_NONE, settings, LIST_OK, screen);
  });
  server.on("/images", HTTP_GET, []() { if (pageGuard()) sendImagesPage(200, nullptr); });
  server.on("/wifi", HTTP_GET, []() {
    if (pageGuard()) sendWifiPage(nullptr, nullptr, false, server.hasArg("scan"));
  });
  server.addHandler(&formRoutes);   // POST /show, /brightness, /text, /wifi
  server.on("/sauver", HTTP_POST, []() { if (pageGuard()) handleSave(); });
  server.on("/places", HTTP_GET, handlePlaces);
  server.on("/place", HTTP_GET, handlePlace);
  server.addHandler(&pageUpload);
  server.on("/images/delete", HTTP_POST, handleImageDelete);
  server.on("/images/show",   HTTP_POST, handleImageShow);
  server.on("/update", HTTP_POST, []() {
    if (!pageGuard()) return;
    if (server.arg("do") == "install") updateInstallNow();
    else                               updateCheckNow();
    server.sendHeader("Location", "/panel");
    server.send(303, "text/plain", "");
  });
  // Every browser signed in so far has to type a code again, except this one
  server.on("/signout", HTTP_POST, []() {
    if (!pageGuard()) return;
    authSignOutEverywhere();
    server.sendHeader("Set-Cookie", authSetCookie());
    server.sendHeader("Location", "/panel");
    server.send(303, "text/plain", "");
  });
  // The coding agent bridge: hooks call /agent/<state>, or /claude/<state>
  // as the first ones still do; http hooks post their payload, and
  // rawAgentBody receives the body chunk by chunk (see above). The state is
  // in the path and not in ?e=: in raw mode, WebServer does not parse the
  // URL parameters.
  // A GET curl with the token header is enough to test from the PC.
  for (const char *route : {"/agent/{}", "/claude/{}"}) {
    server.on(UriBraces(route), HTTP_POST, handleAgent, rawAgentBody);
    server.on(UriBraces(route), HTTP_GET,  handleAgent);
  }

  server.on("/install", HTTP_GET, handleInstall);
  // The hooks fold's buttons (Panel page). A POST behind the page's checks
  // only: a GET, from a link or an image of another page, changes nothing.
  server.on("/hooks", HTTP_POST, []() {
    if (!pageGuard()) return;
    const String what = server.arg("do");
    if (what == "renew") {
      if (!authRenewHookToken()) {           // the old token stays: said on the page
        Serial.println("[HOOK] token not renewed: NVS refused it");
        sendPanelPage(200, SAVE_FAILED, settings, LIST_OK);
        return;
      }
    } else if (what == "accept") authSetHookOpen(true);
    else if (what == "refuse") authSetHookOpen(false);
    else { server.send(400, "text/plain", "unknown action"); return; }
    server.sendHeader("Location", "/panel#hooks");
    server.send(303, "text/plain", "");
  });

  // The purge happens in loop(), the only writer of the table. Redirect to
  // the page instead of returning it: built right away, it would still show
  // the sessions just cleared.
  server.on("/purge", HTTP_POST, []() {
    if (!pageGuard()) return;
    AgentEvent ev = {};
    ev.state = EV_PURGE;
    postAgentEvent(ev);
    server.sendHeader("Location", "/panel");
    server.send(303, "text/plain", "");
  });
  // Captive portal: the phone checks its connection by calling a Google,
  // Apple or Microsoft page. The access point's DNS sends them all here;
  // redirecting them to the Wi-Fi page makes it open by itself.
  server.onNotFound([]() {
    if (wifiSetupMode) {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/wifi");
      server.send(302, "text/plain", "");
    } else if (machinePath(server.uri().c_str())) {
      server.send(404, "text/plain", "404");   // the API and the hooks: a machine answer
    } else {
      sendError(404);
    }
  });
  static const char *HEADERS[] = {"Cookie", "Origin", "Sec-Fetch-Site", "Authorization",
                                  "X-Hook-Token", "Accept-Encoding"};
  server.collectHeaders(HEADERS, sizeof(HEADERS) / sizeof(HEADERS[0]));
  // After networkBegin(): esp_random() is a true random source only once
  // the radio runs, and the device secret comes from it.
  authBegin();
  server.begin();
  server.disableNagle();
  xTaskCreatePinnedToCore(webTask, "web", 8192, nullptr, 1, nullptr, 0);
}
