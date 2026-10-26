// SPDX-License-Identifier: AGPL-3.0-only
// The boot sequence: the se/de square fades in, holds and fades out to
// black (skipped after a software restart, which starts at the word); after a moment of black the word "Carousel" fades in over a thin
// line that fills while the Wi-Fi is awaited (Simple), plus the IP under it
// once joined (Detailed); then the line widens to the panel's full width as
// the word fades, and the first screen rises from it. Beats, fades, fill, layout and the opening come from
// boot_logic; this file only draws, every colour scaled through bootFaded. A
// failed Wi-Fi stops the line in the alert colour; the setup screen then
// opens as before (wifi_logic.cpp).
#include <WiFi.h>
#include <esp_system.h>
#include <stdlib.h>
#include <string.h>

#include "boot_logic.h"
#include "display.h"
#include "i18n.h"
#include "mark.h"
#include "network.h"
#include "palette.h"
#include "screens/screens.h"
#include "settings.h"
#include "type.h"
#include "wordmark.h"

#ifdef SCREENS_TEST_CONSOLE
static const bool TEST_CONSOLE = true;
#else
static const bool TEST_CONSOLE = false;
#endif

static BootProgress progress;
static bool gone = false;   // the screen came in: never again until a restart

// The first screen as drawn, kept while the opening runs (4 KB, allocated only
// then): the screen rises from the line, each frame writes it over the word
// beat, and the screen gets its own frame back before it draws again, as
// screens may draw on their last frame
static uint16_t *stage  = nullptr;
static bool      staged = false;
static bool      noRoom = false;   // the copy could not be allocated
static const size_t STAGE_BYTES = (size_t)PANEL_RES_X * PANEL_RES_Y * sizeof(uint16_t);

static void releaseStage() {
  free(stage);
  stage  = nullptr;
  staged = false;
  noRoom = false;
}

void bootStageBack(uint16_t *frame) {
  if (staged && frame) memcpy(frame, stage, STAGE_BYTES);
}

// The station's address once it has one; false before. No String: nothing
// allocated per frame.
static bool ipText(char *out, size_t size) {
  if (networkState == NET_CONNECTING) return false;
  const IPAddress ip = WiFi.localIP();
  snprintf(out, size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return true;
}

// The palette with the roles the word beat draws in, at level/255 of their light
static Palette faded(const Palette &p, uint8_t level) {
  Palette f = p;
  f.label  = bootFaded(p.label, level);
  f.accent = bootFaded(p.accent, level);
  f.alert  = bootFaded(p.alert, level);
  f.dim    = bootFaded(p.dim, level);
  return f;
}

// The thin line under the word: a dim track, lit as far as the wait went
// (in the alert colour once the Wi-Fi failed)
static void drawLine(Adafruit_GFX &g, const Palette &p, int16_t x, int16_t y, int16_t w,
                     uint32_t now) {
  g.drawFastHLine(x, y, w, p.dim);
  const int lit = progress.fill(w, now);
  if (lit) g.drawFastHLine(x, y, lit, progress.stopped() ? p.alert : p.accent);
}

// Detailed's one line, centred, its cap top at y: the IP once the Wi-Fi is
// joined, fading in (two lines in portrait, the second 7 rows down); nothing
// before. p is already at the word's light.
static void drawIp(Adafruit_GFX &g, const Palette &p, int16_t y, uint32_t now) {
  const uint8_t level = progress.ipLevel(now);
  char ip[16];
  if (!level || !ipText(ip, sizeof(ip))) return;
  const uint16_t c = bootFaded(p.label, level);
  const int16_t cx = g.width() / 2;
  if (g.width() >= 60) {
    drawText(g, ip, cx, y, Face::LABEL, c, Align::CENTER);
    return;
  }
  char first[16], second[16];
  bootSplitIp(ip, first, second, sizeof(first));
  drawText(g, first, cx, y, Face::LABEL, c, Align::CENTER);
  drawText(g, second, cx, y + 7, Face::LABEL, c, Align::CENTER);
}

// Where the word beat sits on g, and the word's ink (its box, whatever its
// face) drawn with its top at row 0
static BootLayout wordLayout(const Adafruit_GFX &g, BootInk &ink) {
  drawBootWord(ink, g.width() / 2, 0, 255);
  const bool detailed = bootDetailedFor(settings.bootDetail, FIRMWARE_VERSION, TEST_CONSOLE);
  return bootLayout(g.height(), ink.bottom - ink.top + 1, detailed);
}

// The word beat on g at the word's light times light/255: the word, the line
// under it as wide as its ink, and in Detailed the IP. Drawn on the panel
// while the word shows, then fading out through the opening, so the word
// leaves with it instead of being erased.
static void drawWordBeat(Adafruit_GFX &g, uint32_t now, uint8_t light) {
  const uint8_t level = progress.wordLevel(now) * light / 255;
  BootInk ink(g.width(), g.height());
  const BootLayout l = wordLayout(g, ink);
  if (!level || !ink.any()) return;
  const Palette lit = faded(palette(), level);
  drawBootWord(g, g.width() / 2, l.wordY - ink.top, level);
  drawLine(g, lit, ink.left, l.lineY, ink.right - ink.left + 1, now);
  if (l.ipY >= 0) drawIp(g, lit, l.ipY, now);
}

// The square, centred, at the light the sequence gives it: rising, held,
// then falling to black before the word comes in
static void drawMark(const Palette &p, uint32_t now) {
  if (!progress.markLevel(now)) return;
  const uint16_t c = bootFaded(p.text, progress.markLevel(now));
  display->drawBitmap((W() - MARK_W) / 2, (H() - MARK_H) / 2, MARK_BITS, MARK_W, MARK_H, c);
}

bool drawBootScreen(uint32_t now) {
  if (gone) return false;
  const BootInputs in = {networkState != NET_CONNECTING, wifiSetupMode, wifiFailure != FAIL_NONE};
  static bool first = true;
  if (first) {
    // millis() counts from the app's start: the bootloader's few hundred ms
    // before it are not in N
    first = false;
    // An update or a restart from the page comes back to the word; a power-on
    // (or a crash, a reset button) keeps the mark
    if (esp_reset_reason() == ESP_RST_SW) progress.shorten();
    Serial.printf("[BOOT] mark at %lu ms\n", (unsigned long)now);
  }
  progress.update(in, now);
  const uint8_t beat = progress.beat(now);
  if (beat >= BEAT_OPEN) {
    gone = true;
    Serial.printf("[BOOT] ready in %lu ms\n", (unsigned long)now);
    return false;
  }
  const Palette &p = palette();
  display->clearScreen();
  drawMark(p, now);   // dark by the time the word lights
  drawWordBeat(*display, now, 255);
  return true;
}

// The first screen coming in, over everything drawn this frame: kept whole
// in stage, the frame cleared, the word beat drawn at the light the opening
// leaves it, the line widened to the panel's full width and then lifted by the
// screen, which is written back from the line down.
void drawBootOpening(uint32_t now) {
  if (progress.beat(now) != BEAT_OPEN) {
    if (stage) releaseStage();
    return;
  }
  uint16_t *frame = display->getBuffer();
  if (!frame) return;
  if (!stage && !(stage = (uint16_t *)malloc(STAGE_BYTES))) {
    // No room for the copy: the screen as drawn, a cut (said once)
    if (!noRoom) Serial.println("[BOOT] no memory for the opening: the first screen at once");
    noRoom = true;
    return;
  }
  memcpy(stage, frame, STAGE_BYTES);
  staged = true;

  BootInk ink(W(), H());
  const BootLayout l = wordLayout(*display, ink);
  const BootOpeningFrame f =
      bootOpeningAt(progress.opened(now), W(), H(), l.lineY, ink.left, ink.right);
  display->clearScreen();
  drawWordBeat(*display, now, f.wordLight);
  if (f.lineRow >= 0)
    display->drawFastHLine(f.lineLeft, f.lineRow, f.lineRight - f.lineLeft + 1, palette().accent);
  bootOpeningStage(f, stage, frame, PANEL_RES_X, PANEL_RES_Y, display->getRotation());
}
