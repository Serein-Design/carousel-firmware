// SPDX-License-Identifier: AGPL-3.0-only
// Panel, without the web server: the object itself, its language, its
// nights, its updates, its network and, folded, what it talks to (Home
// Assistant, the coding hooks, the local API) and its memory. Checked on the
// PC by test/test_web_panel; test/test_web_pages writes it for the browser
// checks; web.cpp gathers the data and sends what it builds.
//
// Its forms come first and stay empty (sv the save, uf the updates, pf the
// session list, so the sign-out, hk the hook token's buttons): every control joins one by form=, so a button
// of Advanced never posts the settings, and Save posts nothing else.
#pragma once

#include <Arduino.h>

#include "screen_list.h"
#include "settings.h"
#include "updater.h"
#include "web_screens.h"
#include "web_shell.h"

enum MqttLink : uint8_t { MQTT_OFF, MQTT_UP, MQTT_DOWN };

struct PanelView {
  const Settings *values;  const Settings *stored;   // as stored or as posted after a failed write; secrets' marks from stored
  SaveResult result;
  ListCheck refusal;       // SAVE_REFUSED: why (a list posted without pg, by a script)
  UpdateState update; const char *latest; const char *updateError;   // escaped here
  const char *ssid; int rssi; bool connected;                        // ssid escaped here
  MqttLink mqtt; int sessionCount; const char *ip; const char *token;
  const char *hookToken;   // in the install command, escaped here
  bool hookOpen;           // hooks without a token accepted
  uint32_t hookRefusals;   // hook calls refused since start
  uint32_t freeKb, minFreeKb; const char *resetReason; uint32_t upSec;
};

enum SignalWord : uint8_t { SIGNAL_GOOD, SIGNAL_FAIR, SIGNAL_WEAK };
SignalWord signalWord(int dbm);   // >= -67 good, -68 to -75 fair, below -75 weak

// The feedback choices as the browser posts them, by name: tr the transition,
// bf the brightness change, kf the knob turn, rq0..rq3 a request kind's show
// (RequestKind order), bd the boot screen (on Panel, under Advanced), each
// one digit below its enum's count. What a post leaves out, or is not an
// option, keeps the stored value; of two values for one control the first
// counts, as a radio group posts one.
class FeedbackForm {
public:
  void arg(const char *name, const char *value);
  void apply(Settings &s) const;

private:
  int8_t transition_ = -1, brightness_ = -1, knob_ = -1, boot_ = -1;   // -1: not posted
  int8_t request_[REQUEST_KIND_COUNT] = {-1, -1, -1, -1};
};

// The region as the browser posts it, by name: tu the temperature unit, cu
// the currency (each one digit below its enum's count), ck the clock (0: 24 h,
// 1: 12 h). What a post leaves out, or is not an option, keeps the stored
// value; of two values for one control the first counts.
class RegionForm {
public:
  void arg(const char *name, const char *value);
  void apply(Settings &s) const;
  const RegionChoice &choice() const { return choice_; }

private:
  RegionChoice choice_;
};

// The page goes out in pieces: s is handed to flush (which sends and
// empties it) after the side column and after each section, so no piece
// outgrows PANEL_PAGE_RESERVE, even with every value at its longest and
// escaped (test_page_fits_24k). What is left in s is the caller's to send.
static const size_t PANEL_PAGE_RESERVE = 4096;
void panelPage(String &s, const Shell &sh, const PanelView &v, void (*flush)(String &));

// GET /panel/feedback, in pieces as panelPage: the transition, the brightness
// change and the knob turn, then each request kind as a segmented choice.
// Its save names the page (pg=panel/feedback), so the answer comes back here.
void feedbackPage(String &s, const Shell &sh, const PanelView &v, void (*flush)(String &));
