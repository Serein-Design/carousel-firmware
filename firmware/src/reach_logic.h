// SPDX-License-Identifier: AGPL-3.0-only
// Reaching the panel's pages: the knob's long press, the reach card's time
// and pages, the Wi-Fi setup screen's pages, and the texts their QR codes
// and lines carry. Hardware-free: gallery.cpp and screens/reach.cpp draw
// what it decides; test/test_reach checks it on the PC.
#pragma once

#include <stddef.h>
#include <stdint.h>

// A press held this long, then released, is a long press: past the
// brightness step's quick tap, well short of the boot-time setup hold (3 s)
#define LONG_PRESS_MS  800UL
#define REACH_SHOW_MS  20000UL   // the reach card, unless the knob moves first
#define REACH_PAGE_MS  4000UL    // its two pages, and the setup screen's three, take turns
#define SETUP_PAGES    3

enum class Press : uint8_t { NONE, SHORT, LONG };

// Tells a press once released, by how long it was held. A contact already
// closed at the first look (the knob held through the boot, for setup or a
// reset) is not a press: it counts from the first time it is seen open.
class PressTimer {
 public:
  Press update(bool closed, uint32_t now);
  // How long the press under way has been held; 0 when none is
  uint32_t heldMs(uint32_t now) const { return down_ ? now - since_ : 0; }

 private:
  bool     opened_ = false;
  bool     down_   = false;
  uint32_t since_  = 0;
};

// The reach card's 20 s and its pages: 0 the address, 1 the QR code
class ReachCard {
 public:
  void    show(uint32_t now) { shownAt_ = now; on_ = true; }
  void    hide() { on_ = false; }
  bool    shown(uint32_t now);          // false once its time ran out
  uint8_t page(uint32_t now) const { return (now - shownAt_) / REACH_PAGE_MS % 2; }

 private:
  bool     on_      = false;
  uint32_t shownAt_ = 0;
};

// The setup screen's page after ms on show: 0 join, 1 open the address, 2 QR
uint8_t setupPage(uint32_t ms);

// A QR code's text; false if it does not fit n. Both stay within 32 bytes,
// what a version 2 code holds at the low correction level.
// "WIFI:S:<ssid>;T:nopass;;": a phone camera offers to join the open access
// point. The panel's own name has none of the characters it would escape.
bool apQrText(char *out, size_t n, const char *ssid);
// "http://<ip>/": an IP always works, where a .local name may not
bool pageQrText(char *out, size_t n, const uint8_t ip[4]);

// The pages' address on one label line when it fits maxPx, else on two, cut
// before its first dot ("carousel-1a2b" / ".local"); b is empty for one line
void addressLines(const char *host, uint16_t maxPx, char *a, char *b, size_t n);
// An IP on two portrait lines: "192.168." and "1.42"
void ipLines(const uint8_t ip[4], char *a, char *b, size_t n);
// The access point's name down a portrait column: "CARO-", "USEL-", "695C"
void apNameTall(const char *ap, char lines[3][8]);
