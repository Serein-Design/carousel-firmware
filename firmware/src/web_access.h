// SPDX-License-Identifier: AGPL-3.0-only
// The pages a visitor reaches before the others: the Wi-Fi page, served at
// /wifi and as the setup access point's captive page; the sign-in page; the
// locked page. Built without the web server, from a view web.cpp fills, so
// test/test_web_access checks them and test/test_web_pages writes them.
#pragma once

#include <Arduino.h>

#include "web_shell.h"

struct WifiNet { char ssid[33]; int16_t rssi; bool open; };
// Rows the page lists; a crowded street sees more, the weakest are left out
static const int WIFI_ROWS_MAX = 30;
// web.cpp reserves WIFI_PAGE_BASE + WIFI_ROW_BYTES per row: the page
// without rows, the longest message and a last try in every language, plus
// a row of a 32-byte name (test_page_fits_24k). Names that escape to more
// grow the string; even then the worst page stays under 24 KB.
static const size_t WIFI_PAGE_BASE = 4096;
static const size_t WIFI_ROW_BYTES = 256;

// Hidden networks dropped, one row per name with its strongest signal,
// strongest first, at most WIFI_ROWS_MAX, in place. Returns the count.
int wifiRows(WifiNet *nets, int n);
// The form's name: typed when not empty, else the chosen row
const char *wifiChosenName(const char *typed, const char *row);
// The bars a row shows for its signal, 1 (weak) to 4 (excellent), the
// thresholds a phone's Wi-Fi settings use: -55, -67 and -75 dBm
int wifiBars(int16_t rssi);

// WiFi.scanComplete()'s "still running" (WIFI_SCAN_RUNNING) and "none done,
// or the last one failed" (WIFI_SCAN_FAILED), without WiFi.h
static const int WIFI_SCAN_BUSY = -1;
static const int WIFI_SCAN_NONE = -2;
// 80 pauses of 50 ms: a scan of every channel takes about 3 s
static const int WIFI_SCAN_WAIT_PAUSES = 80;
// Asks poll() until the scan answers, with pause() between two asks, at
// most `pauses` times: the scan's count (or failure), else WIFI_SCAN_BUSY.
// /wifi waits so in the web task instead of a page that reloads itself.
int wifiAwaitScan(int (*poll)(), void (*pause)(), int pauses);

// What a Wi-Fi answer does with the scan: start a new one or not, and how
// many pauses it waits for one. complete: WiFi.scanComplete(); asked: the
// Refresh link; restarting: the answer to a save or a retry, which a restart
// follows 2 s on. That one neither starts a scan nor waits (0 pauses: the
// list as it is), or the restart would cut the confirmation off.
struct WifiScanPlan { bool start; int pauses; };
WifiScanPlan wifiScanPlan(int complete, bool asked, bool restarting);

enum WifiLink : uint8_t { LINK_SETUP, LINK_CONNECTED, LINK_OFFLINE };
enum WifiTry  : uint8_t { TRY_NONE, TRY_REFUSED, TRY_NOT_FOUND };
// saved: the stored network ("" none); ip: the address when connected;
// rows: count networks from wifiRows(), none while a scan is not done
// (the page says so); message: a wifiMsg* format or nullptr, with
// messageName for its %s (escaped here).
struct WifiPageView { const char *saved; WifiLink link; const char *ip; WifiTry lastTry;
                      const WifiNet *rows; int count;
                      const char *message; const char *messageName; };

// The whole page. In setup mode it has no tabs and the setup brand (the
// other pages cannot help without a network); on the home network the tabs,
// none current. The title is the page's own.
void wifiPage(String &s, const Shell &sh, const WifiPageView &v);

// --- Sign-in and locked -------------------------------------------------------

// web.cpp reserves this for either page: the longest of them, in every
// language, fits it (test_page_fits_24k)
static const size_t ACCESS_PAGE_RESERVE = 4096;

// Where sign-in sends the visitor back: one of the six pages (Feedback's
// included), or a screen's page (/screens/<slug>, asked itself, held as long as asked is),
// exactly as written; anything else (another site's address included) is
// home, so the redirect can never leave the panel. What it returns holds
// only letters, '/' and '-': nothing to escape.
const char *returnPath(const char *asked);

// wrong: the code typed did not match (or expired); next: the page asked
// for, kept through returnPath() whatever it holds
struct SignInView { bool wrong; const char *next; };
// The stage block draws the panel's sign-in frame with its digits hidden:
// the page never carries the code. No tabs, before any session.
void signInPage(String &s, const Shell &sh, const SignInView &v);
// An unfinished factory reset: RESET FAILED as the panel shows it, the text
// in English and in French (the language setting cannot be read, so sh.t is
// ignored), the reinstall link unless knob (a knob reset that wrote no
// marker is never resumed: its text asks for the knob instead). No tabs, no
// form: nothing can be changed until the reset ends.
void lockedPage(String &s, const Shell &sh, bool knob);

// --- Error pages ----------------------------------------------------------------

// What a browser route's refusal or failure says, from its status: 400 and
// 415 share theirs (the form did not arrive as sent), 401 is a refusal,
// anything not listed is the panel short of memory. The API, the hooks,
// /preview, /id, the assets and the image files keep their plain answers.
enum ErrorKind : uint8_t { ERR_NOT_FOUND, ERR_REFUSED, ERR_TOO_LARGE, ERR_BAD_FORM, ERR_BUSY };
ErrorKind errorKind(int code);
// web.cpp reserves this: every kind in every language fits it, small
// enough to go out when a page could not be built
static const size_t ERROR_PAGE_RESERVE = 2048;
// The shell (tabs only when sh.tabs: signed in), a stage block with the
// kind's title and the status, its sentence, and a link to back's page
void errorPage(String &s, const Shell &sh, int code, Tab back);
