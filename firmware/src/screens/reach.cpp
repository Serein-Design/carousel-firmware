// SPDX-License-Identifier: AGPL-3.0-only
// Reaching the panel's pages: the Wi-Fi setup screen while the access point
// is open (three pages in turn), and the reach card a long press on the
// knob shows. What they say and when is decided in reach_logic.cpp.
#include <WiFi.h>
#include <qrcode.h>

#include "display.h"
#include "grid.h"
#include "panel_name.h"
#include "i18n.h"
#include "network.h"
#include "palette.h"
#include "reach_logic.h"
#include "screens/screens.h"
#include "type.h"

// A QR code's modules, one row per word: version 2 (25 modules) is the
// largest that fits the 32 rows with its quiet zone
struct QrBits {
  uint32_t rows[32];
  uint8_t  size;   // modules per side, 0 = none
};
static const int QR_QUIET = 3;                 // modules of lit margin each side
static const int QR_SIDE  = 25 + 2 * QR_QUIET; // 31 px

// esp_qrcode hands its code to a callback without a context: the target is
// set around the call
static QrBits *qrTarget = nullptr;

static void keepQr(esp_qrcode_handle_t q) {
  const int n = esp_qrcode_get_size(q);
  if (!qrTarget || n > 32) return;
  for (int y = 0; y < n; y++) {
    qrTarget->rows[y] = 0;
    for (int x = 0; x < n; x++)
      if (esp_qrcode_get_module(q, x, y)) qrTarget->rows[y] |= 1UL << x;
  }
  qrTarget->size = n;
}

// Built once per text, never per frame (it allocates)
static bool buildQr(const char *text, QrBits &out) {
  out.size = 0;
  qrTarget = &out;
  esp_qrcode_config_t cfg = {};
  cfg.display_func       = keepQr;
  cfg.max_qrcode_version = 2;
  cfg.qrcode_ecc_level   = ESP_QRCODE_ECC_LOW;
  esp_qrcode_generate(&cfg, text);
  qrTarget = nullptr;
  return out.size;
}

// Light modules lit, dark ones off, inside a lit margin: a phone camera
// reads it like a printed code
static void drawQr(const QrBits &q, int x0, int y0) {
  const Palette &p = palette();
  display->fillRect(x0, y0, q.size + 2 * QR_QUIET, q.size + 2 * QR_QUIET, p.text);
  for (int y = 0; y < q.size; y++)
    for (int x = 0; x < q.size; x++)
      if (q.rows[y] >> x & 1) display->drawPixel(x0 + QR_QUIET + x, y0 + QR_QUIET + y, p.off);
}

// The column beside a QR code drawn at the left edge: its middle
static const int QR_COLUMN_MID = QR_SIDE + (64 - QR_SIDE) / 2;

// ===========================================================================
// Wi-Fi setup: 1 join the access point, 2 open its address, 3 a QR
// that joins it; 4 s each. Once a phone is on it, only the address.
// ===========================================================================
static QrBits apQr;
static char   apAddr[16], apAddrA[8], apAddrB[8], apTall[3][8];

// The access point's name and address are fixed while it is open: worked
// out when the screen comes back, not every frame
static void prepareSetup() {
  static bool qrTried = false;
  if (!qrTried) {
    qrTried = true;
    char text[40];
    if (apQrText(text, sizeof(text), setupApName)) buildQr(text, apQr);
  }
  const IPAddress ip = WiFi.softAPIP();
  const uint8_t b[4] = {ip[0], ip[1], ip[2], ip[3]};
  snprintf(apAddr, sizeof(apAddr), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
  ipLines(b, apAddrA, apAddrB, sizeof(apAddrA));
  apNameTall(setupApName, apTall);
}

static void drawSetupPage(uint8_t page, const Palette &p) {
  const Strings &t = tr();
  if (page == 2 && apQr.size) {
    drawQr(apQr, 0, 0);
    drawText(*display, t.setupScan[0], QR_COLUMN_MID, 6, Face::LABEL, p.text, Align::CENTER);
    drawText(*display, t.setupScan[1], QR_COLUMN_MID, 13, Face::LABEL, p.text, Align::CENTER);
    drawText(*display, "3/3", QR_COLUMN_MID, 24, Face::LABEL, p.label, Align::CENTER);
    return;
  }
  if (page == 2) page = 0;                          // no code: the name instead
  // The label line says what this is, or what went wrong, or that a phone
  // joined (then only the address page shows, without its count)
  const bool refused = wifiFailure == FAIL_WRONG_PASSWORD;
  if (apClientCount) {
    drawText(*display, t.phoneConnected, GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.accent);
  } else {
    drawText(*display, refused ? t.wrongPassword : t.setupLabel, GRID_MARGIN, GRID_LABEL_TOP,
             Face::LABEL, refused ? p.alert : p.label);
    char n[8];
    snprintf(n, sizeof(n), "%u/%u", page + 1, SETUP_PAGES);
    drawText(*display, n, W() - GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.label, Align::RIGHT);
  }
  drawText(*display, page ? t.setupOpen : t.setupJoin, GRID_MARGIN, 11, Face::BODY, p.text);
  drawText(*display, page ? apAddr : setupApName, GRID_MARGIN, GRID_FOOT_TOP, Face::LABEL,
           p.accent);
}

// Portrait: the QR on top (the address once a phone joined), the network's
// name down the column below
static void drawSetupTall(const Palette &p) {
  if (!apClientCount && apQr.size) {
    drawQr(apQr, 0, 0);
  } else {
    drawText(*display, apAddrA, GRID_MARGIN, 10, Face::LABEL, p.accent);
    drawText(*display, apAddrB, GRID_MARGIN, 17, Face::LABEL, p.accent);
  }
  const bool refused = wifiFailure == FAIL_WRONG_PASSWORD;
  drawText(*display, tr().setupTall, GRID_MARGIN, 35, Face::LABEL, refused ? p.alert : p.label);
  for (int i = 0; i < 3; i++)
    drawText(*display, apTall[i], GRID_MARGIN, 42 + i * 7, Face::LABEL, p.accent);
}

void drawWifiSetupScreen(uint32_t) {
  const Palette &p = palette();
  const bool wide = W() >= 60;
  display->clearScreen();
  if (restartAt) {                                  // saved, it is restarting
    if (wide) drawText(*display, tr().saved, GRID_MARGIN, 11, Face::BODY, p.accent);
    else      drawText(*display, "OK", GRID_MARGIN, 28, Face::LABEL, p.accent);
    return;
  }
  // Back on screen after a while: the first page again
  static uint32_t since = 0, lastDraw = 0;
  const uint32_t now = millis();
  if (!lastDraw || now - lastDraw > 1000) {
    since = now;
    prepareSetup();
  }
  lastDraw = now;
  if (!wide) {
    drawSetupTall(p);
    return;
  }
  drawSetupPage(apClientCount ? 1 : setupPage(now - since), p);
}

// ===========================================================================
// Reach card: where the pages are, the IP and a QR code to it
// ===========================================================================
static QrBits pageQr;
static char   reachIp[16], reachA[24], reachB[24], reachIpA[8], reachIpB[8];

bool reachCardBuild() {
  if (WiFi.status() != WL_CONNECTED) return false;
  const IPAddress ip = WiFi.localIP();
  const uint8_t b[4] = {ip[0], ip[1], ip[2], ip[3]};
  char text[40];
  if (!pageQrText(text, sizeof(text), b) || !buildQr(text, pageQr))
    Serial.println("[REACH] no QR code, the address only");
  snprintf(reachIp, sizeof(reachIp), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
  addressLines(panelHostName(), GRID_LINE_W, reachA, reachB, sizeof(reachA));
  ipLines(b, reachIpA, reachIpB, sizeof(reachIpA));
  return true;
}

void drawReachCard(uint8_t page) {
  const Palette &p = palette();
  const Strings &t = tr();
  display->clearScreen();
  if (W() < 60) {                                   // portrait: the code, the IP under it
    if (pageQr.size) drawQr(pageQr, 0, 0);
    drawText(*display, reachIpA, GRID_MARGIN, 35, Face::LABEL, p.text);
    drawText(*display, reachIpB, GRID_MARGIN, 42, Face::LABEL, p.text);
    return;
  }
  if (page == 1 && pageQr.size) {
    drawQr(pageQr, 0, 0);
    drawText(*display, t.reachScan[0], QR_COLUMN_MID, 6, Face::LABEL, p.text, Align::CENTER);
    drawText(*display, t.reachScan[1], QR_COLUMN_MID, 13, Face::LABEL, p.text, Align::CENTER);
    return;
  }
  drawText(*display, t.reachLabel, GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.label);
  if (reachB[0]) {
    drawText(*display, reachA, GRID_MARGIN, 10, Face::LABEL, p.accent);
    drawText(*display, reachB, GRID_MARGIN, 17, Face::LABEL, p.accent);
  } else {
    drawText(*display, reachA, GRID_MARGIN, 13, Face::LABEL, p.accent);
  }
  drawText(*display, reachIp, GRID_MARGIN, GRID_FOOT_TOP, Face::LABEL, p.text);
}
