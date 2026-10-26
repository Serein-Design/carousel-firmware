// SPDX-License-Identifier: AGPL-3.0-only
// Information screens on the system grid and the palette's roles: DATE,
// SYSTEM, MONEY and SOCIAL, and the sign-in code (WEATHER has its own file;
// the Wi-Fi setup screen is in reach.cpp). The label line says what the
// number is, the main block holds it in Pierre (or a smaller face when it is
// too wide), the foot gives the detail; row 31 stays free for the status
// strip. What they say and in which face is decided in info_logic.cpp.
#include <WiFi.h>
#include <esp_timer.h>

#include "auth.h"
#include "clock_logic.h"
#include "display.h"
#include "gallery.h"
#include "grid.h"
#include "i18n.h"
#include "info_logic.h"
#include "network.h"
#include "palette.h"
#include "screens/row_clip.h"
#include "screens/screens.h"
#include "settings.h"
#include "social.h"
#include "type.h"

// Sign-in code, over everything while someone is asking for it (auth.h)
bool drawPairingCode() {
  const uint32_t code = pairingCodeShown();
  if (!code) return false;
  char digits[7];
  signInDigits(code, digits);
  drawSignInCode(digits, pairingTimeLeft());
  return true;
}

namespace {

// The main block in landscape (Pierre's 13 rows), and the portrait column's
// number rows, as CLOCK's
const int16_t MAIN_BOT = GRID_MAIN_TOP + 12;
const int16_t TALL_MAIN_TOP = 10, TALL_MAIN_BOT = 22;

// No time yet: dim dashes where the number goes, as CLOCK does
void drawNoTime(const Palette &p) {
  if (portrait)
    drawText(*display, "--", W() / 2, TALL_MAIN_TOP, Face::PIERRE, p.dim, Align::CENTER);
  else
    drawText(*display, "--", GRID_MARGIN, GRID_MAIN_TOP, Face::PIERRE, p.dim);
}

// The top row of a label-face line whose foot meets that of a line in face
// f drawn from row y: a unit beside its number
int16_t besideFoot(Face f, int16_t y) {
  return y + FONT_FACES[(int)f].cap - FONT_FACES[(int)Face::LABEL].cap;
}

int isoWeek(const struct tm &t) {
  char w[4];
  strftime(w, sizeof(w), "%V", &t);
  return atoi(w);
}

// A thin line that fills as k (0..1) goes: a ghost track, the accent over it
void drawProgress(int16_t x, int16_t y, int16_t w, float k, const Palette &p) {
  display->drawFastHLine(x, y, w, p.ghost);
  const int16_t done = (int16_t)lroundf(k * w);
  if (done > 0) display->drawFastHLine(x, y, done, p.accent);
}

}  // namespace

// ===========================================================================
// DATE: the weekday, the day in Pierre with the month over the year beside
// it, the week and the day of the year
// ===========================================================================
void screenDate(uint32_t f, bool init) {
  (void)f;
  (void)init;
  const Palette &p = palette();
  display->clearScreen();
  struct tm t;
  if (!timeValid || !getLocalTime(&t, 0)) {
    drawNoTime(p);
    return;
  }
  const Strings &s = tr();
  char day[12], year[12], b[24];
  snprintf(day, sizeof(day), "%d", t.tm_mday);
  snprintf(year, sizeof(year), "%d", t.tm_year + 1900);

  if (!portrait) {
    drawText(*display, s.days[t.tm_wday % 7], GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.label);
    drawText(*display, day, GRID_MARGIN, GRID_MAIN_TOP, Face::PIERRE, p.text);
    const int16_t x = GRID_MARGIN + textWidth(day, Face::PIERRE) + INFO_GAP;
    drawText(*display, s.months[t.tm_mon % 12], x, GRID_MAIN_TOP, Face::LABEL, p.text);
    drawText(*display, year, x, besideFoot(Face::PIERRE, GRID_MAIN_TOP), Face::LABEL, p.label);
    snprintf(b, sizeof(b), s.dateWeek, (unsigned)isoWeek(t));
    drawText(*display, b, GRID_MARGIN, GRID_FOOT_TOP, Face::LABEL, p.label);
    dateDayLine(b, sizeof(b), s, t.tm_yday + 1, false);
    drawText(*display, b, W() - GRID_MARGIN, GRID_FOOT_TOP, Face::LABEL, p.label, Align::RIGHT);
    return;
  }
  const int16_t mid = W() / 2;
  drawText(*display, s.clkDays[t.tm_wday % 7], mid, GRID_PORTRAIT_LABEL_TOP, Face::LABEL, p.label,
           Align::CENTER);
  drawText(*display, day, mid, TALL_MAIN_TOP, Face::PIERRE, p.text, Align::CENTER);
  drawText(*display, s.clkMonths[t.tm_mon % 12], mid, 27, Face::LABEL, p.text, Align::CENTER);
  drawText(*display, year, mid, 34, Face::LABEL, p.label, Align::CENTER);
  clockWeekLine(b, sizeof(b), s, isoWeek(t), true);
  drawText(*display, b, mid, 46, Face::LABEL, p.label, Align::CENTER);
  dateDayLine(b, sizeof(b), s, t.tm_yday + 1, true);
  drawText(*display, b, mid, 53, Face::LABEL, p.label, Align::CENTER);
}

// ===========================================================================
// SYSTEM, for makers: the free RAM in Pierre with its share, the Wi-Fi
// signal in bars and dBm, the frame rate and how long the panel has run
// ===========================================================================

// Bars 1, 3, 5 and 7 px high standing on row bottom, lit as far as bars
static void drawSignal(int16_t x, int16_t bottom, uint8_t bars, const Palette &p) {
  for (uint8_t i = 0; i < 4; i++) {
    const int16_t h = 1 + 2 * i;
    display->fillRect(x + 3 * i, bottom - h + 1, 2, h, i < bars ? p.text : p.dim);
  }
}

void screenSystem(uint32_t f, bool init) {
  // Frames per second are only measured while this screen is shown
  static uint32_t refTime = 0, refFrame = 0;
  static float fps = 0;
  if (init) { refTime = millis(); refFrame = f; }
  if (millis() - refTime >= 1000) {
    fps  = (f - refFrame) * 1000.0f / (millis() - refTime);
    refTime = millis();
    refFrame = f;
  }

  const Palette &p = palette();
  const Strings &s = tr();
  display->clearScreen();

  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint8_t  pct      = ramPercent(freeHeap, ESP.getHeapSize());
  const bool     connected = WiFi.status() == WL_CONNECTED;
  const int      rssi     = connected ? WiFi.RSSI() : 0;
  const uint8_t  bars     = signalBars(connected, rssi);
  // esp_timer counts in 64 bits: millis() would wrap after 49 days
  const uint32_t uptime   = (uint32_t)(esp_timer_get_time() / 1000000LL);

  char kb[12], share[8], dbm[12], rate[16], dur[24], b[40];
  snprintf(kb, sizeof(kb), "%lu", (unsigned long)(freeHeap / 1024));
  snprintf(share, sizeof(share), "%u%%", pct);
  if (connected) snprintf(dbm, sizeof(dbm), "%d", signalDbm(rssi));
  else snprintf(dbm, sizeof(dbm), "--");
  snprintf(rate, sizeof(rate), s.sysFps, (unsigned)lroundf(fps < 99 ? fps : 99));
  uptimeText(dur, sizeof(dur), s, uptime);
  // A low share is in the alert colour, its number saying it too
  const uint16_t shareColor = pct < RAM_LOW_PERCENT ? p.alert : p.label;

  if (!portrait) {
    drawText(*display, s.sysRam, GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.label);
    drawText(*display, rate, W() - GRID_MARGIN, GRID_LABEL_TOP, Face::LABEL, p.label, Align::RIGHT);
    drawText(*display, kb, GRID_MARGIN, GRID_MAIN_TOP, Face::PIERRE, p.text);
    const int16_t x = GRID_MARGIN + textWidth(kb, Face::PIERRE) + INFO_GAP;
    drawText(*display, s.sysKb, x, GRID_MAIN_TOP, Face::LABEL, p.label);
    drawText(*display, share, x, besideFoot(Face::PIERRE, GRID_MAIN_TOP), Face::LABEL, shareColor);
    drawSignal(W() - GRID_MARGIN - SIGNAL_BARS_W, GRID_MAIN_TOP + 6, bars, p);
    drawText(*display, dbm, W() - GRID_MARGIN, besideFoot(Face::PIERRE, GRID_MAIN_TOP), Face::LABEL,
             connected ? p.label : p.dim, Align::RIGHT);
    snprintf(b, sizeof(b), s.sysUp, dur);
    drawText(*display, b, GRID_MARGIN, GRID_FOOT_TOP, Face::LABEL, p.label);
    return;
  }
  const int16_t mid = W() / 2;
  drawText(*display, s.sysRamTall, mid, GRID_PORTRAIT_LABEL_TOP, Face::LABEL, p.label,
           Align::CENTER);
  drawText(*display, kb, mid, TALL_MAIN_TOP, Face::PIERRE, p.text, Align::CENTER);
  snprintf(b, sizeof(b), "%s %s", s.sysKb, share);
  drawText(*display, b, mid, 26, Face::LABEL, shareColor, Align::CENTER);
  drawText(*display, s.sysUpTall, mid, 35, Face::LABEL, p.label, Align::CENTER);
  drawText(*display, dur, mid, 42, Face::LABEL, p.text, Align::CENTER);
  drawSignal(GRID_MARGIN, 55, bars, p);
  drawText(*display, dbm, W() - GRID_MARGIN, 51, Face::LABEL, connected ? p.label : p.dim,
           Align::RIGHT);
  drawText(*display, rate, mid, 58, Face::LABEL, p.label, Align::CENTER);
}

// ===========================================================================
// MONEY: what you earn, second by second, in settings.currency. The day's
// whole units in Pierre, the symbol written before them and the cents beside
// them in the label face, the working day as a line, the month's total.
// ===========================================================================
void screenMoney(uint32_t f, bool init) {
  (void)f;
  (void)init;
  const Palette &p = palette();
  display->clearScreen();
  struct tm t;
  if (!timeValid || !getLocalTime(&t, 0)) {
    drawNoTime(p);
    return;
  }
  const Strings &s = tr();
  const MoneyDay d = moneyDay({settings.salary, settings.workDays, settings.workStart,
                               settings.workEnd},
                              t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec, t.tm_mday);
  MoneyText m;
  char total[32];
  moneyAmount(m, s, settings.currency, d.today);
  // The count climbs in the text colour while the day is on, rests in the label's after
  const uint16_t amount = d.working ? p.text : p.label;
  const int16_t beforeW = m.before[0] ? textWidth(m.before, Face::LABEL) + INFO_GAP : 0;

  if (!portrait) {
    moneyTotal(total, sizeof(total), s, settings.currency, d.month,
               GRID_LINE_W - INFO_GAP - textWidth(s.month, Face::LABEL));
    drawText(*display, d.working ? s.working : s.offHours, GRID_MARGIN, GRID_LABEL_TOP,
             Face::LABEL, p.label);
    const Face face = numberFace(m.units, moneyRoom(m, GRID_LINE_W, false));
    const int16_t y = faceTop(face, GRID_MAIN_TOP, MAIN_BOT);
    const int16_t x = GRID_MARGIN + beforeW;
    if (beforeW) drawText(*display, m.before, GRID_MARGIN, besideFoot(face, y), Face::LABEL, p.label);
    drawText(*display, m.units, x, y, face, amount);
    drawText(*display, m.after, x + textWidth(m.units, face) + INFO_GAP, besideFoot(face, y),
             Face::LABEL, p.label);
    drawProgress(GRID_MARGIN, GRID_MAIN_BOT + 1, GRID_LINE_W, d.progress, p);
    drawText(*display, s.month, GRID_MARGIN, GRID_FOOT_TOP, Face::LABEL, p.label);
    drawText(*display, total, W() - GRID_MARGIN, GRID_FOOT_TOP, Face::LABEL, p.text, Align::RIGHT);
    return;
  }
  const int16_t mid = W() / 2;
  moneyTotal(total, sizeof(total), s, settings.currency, d.month, GRID_PORTRAIT_LINE_W);
  drawText(*display, d.working ? s.workingShort : s.pausedShort, mid, GRID_PORTRAIT_LABEL_TOP,
           Face::LABEL, p.label, Align::CENTER);
  // The symbol before and the units, centred together
  const Face face = numberFace(m.units, moneyRoom(m, GRID_PORTRAIT_LINE_W, true));
  const int16_t y = faceTop(face, TALL_MAIN_TOP, TALL_MAIN_BOT);
  const int16_t x = mid - ((beforeW + textWidth(m.units, face)) >> 1);
  if (beforeW) drawText(*display, m.before, x, besideFoot(face, y), Face::LABEL, p.label);
  drawText(*display, m.units, x + beforeW, y, face, amount);
  drawText(*display, m.after, mid, 26, Face::LABEL, p.label, Align::CENTER);
  drawProgress(GRID_MARGIN, 33, GRID_PORTRAIT_LINE_W, d.progress, p);
  drawText(*display, s.month, mid, 40, Face::LABEL, p.label, Align::CENTER);
  drawText(*display, total, mid, 47, Face::LABEL, p.text, Align::CENTER);
}

// ===========================================================================
// SOCIAL: one counter at a time in Pierre, the next pushing it up every 4 s
// as CLOCK's readouts do: the network, its count, what it counts, and a dash
// per counter. A dot says where the number comes from: filled, an API;
// hollow, typed in on the settings page.
// ===========================================================================

// The YouTube counter had a key and a channel once and has not now: no key
static bool socialKeyMissing() {
  return socialStats.ytOk && !(strlen(settings.ytKey) > 10 && strlen(settings.ytChannel) > 10);
}

static void drawCounter(Adafruit_GFX &g, uint8_t net, uint32_t value, bool live, int16_t dy,
                        const Palette &p) {
  const Strings &s = tr();
  char b[24];
  const int16_t dotY = portrait ? 27 : GRID_LABEL_TOP + 1;
  const int16_t dotX = portrait ? W() / 2 - 1 : W() - GRID_MARGIN - 3;
  if (live) g.fillRect(dotX, dotY + dy, 3, 3, p.accent);
  else g.drawRect(dotX, dotY + dy, 3, 3, p.dim);
  if (portrait) {
    const int16_t mid = W() / 2;
    drawText(g, s.socialNamesTall[net], mid, GRID_PORTRAIT_LABEL_TOP + dy, Face::LABEL, p.label,
             Align::CENTER);
    const Face face = socialCount(b, sizeof(b), s, value, GRID_PORTRAIT_LINE_W);
    drawText(g, b, mid, faceTop(face, TALL_MAIN_TOP, TALL_MAIN_BOT) + dy, face, p.text,
             Align::CENTER);
    return;
  }
  drawText(g, s.socialNames[net], GRID_MARGIN, GRID_LABEL_TOP + dy, Face::LABEL, p.label);
  const Face face = socialCount(b, sizeof(b), s, value, GRID_LINE_W);
  drawText(g, b, GRID_MARGIN, faceTop(face, GRID_MAIN_TOP, MAIN_BOT) + dy, face, p.text);
  drawText(g, net == SOCIAL_YT ? s.socialSubscribers : s.socialFollowers, GRID_MARGIN,
           GRID_FOOT_TOP + dy, Face::LABEL, p.label);
}

void screenSocial(uint32_t f, bool init) {
  (void)f;
  (void)init;
  const Palette &p = palette();
  display->clearScreen();
  const uint32_t value[SOCIAL_COUNT] = {socialStats.ytSubscribers, settings.instaFollowers,
                                        settings.xFollowers, socialStats.ghFollowers};
  const bool live[SOCIAL_COUNT] = {socialStats.ytOk, false, false, socialStats.ghOk};
  uint8_t shown[SOCIAL_COUNT];
  const uint8_t n = socialShown(value, live, shown);

  // The card moves as a whole, within the rows above the status strip
  const int16_t bottom = portrait ? 31 : GRID_FOOT_BOT;
  const uint8_t travel = bottom + 1;
  const CycleStep c = clockCycle(millis(), n, travel);
  RowClip clip(0, bottom);
  if (c.shift) {
    const uint8_t k = shown[c.before];
    drawCounter(clip, k, value[k], live[k], c.shift - travel, p);
  }
  const uint8_t k = shown[c.now];
  drawCounter(clip, k, value[k], live[k], c.shift, p);

  // Which counter of how many: stays put while the cards move
  const int16_t pw = socialPagerWidth(n);
  const int16_t px = portrait ? (W() - pw) / 2 : W() - GRID_MARGIN - pw;
  const int16_t py = portrait ? 35 : GRID_FOOT_TOP + 2;
  for (uint8_t i = 0; pw && i < n; i++)
    display->drawFastHLine(px + 3 * i, py, 2, i == c.now ? p.accent : p.dim);

  // E1: the counters stay, dimmed, and a foot band says why they are old
  const Reason why = serviceReason({socialStats.ytOk || socialStats.ghOk, socialStats.updatedAt,
                                    WiFi.status() == WL_CONNECTED, socialKeyMissing()},
                                   millis(), SOCIAL_FRESH_MS);
  if (why != REASON_NONE) drawStale(why);
}
