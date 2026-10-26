// SPDX-License-Identifier: AGPL-3.0-only
/*
 * Build-time defaults. Copy this file to secrets.h (git-ignored) and fill it in:
 *
 *   cp include/secrets.example.h include/secrets.h
 *
 * Without secrets.h the firmware builds with these placeholders. An empty
 * Wi-Fi SSID is fine: the panel then opens its setup access point and the
 * network can be configured from a phone, no reflash needed.
 *
 * The ESP32 only supports 2.4 GHz Wi-Fi.
 */

#pragma once

#define WIFI_SSID  ""
#define WIFI_PASS  ""

// POSIX time zone. This one is Central European Time with automatic DST
// (France, Belgium, Switzerland, Luxembourg...).
//   United Kingdom : "GMT0BST,M3.5.0/1,M10.5.0"
//   Quebec         : "EST5EDT,M3.2.0,M11.1.0"
#define FUSEAU_HORAIRE "CET-1CEST,M3.5.0,M10.5.0/3"

#define NTP_SERVEUR_1  "pool.ntp.org"
#define NTP_SERVEUR_2  "time.google.com"

// Weather location until a city is set on the web page (MET Norway forecast:
// free, no key, no account). Default: Paris.
#define METEO_LATITUDE   "48.85"
#define METEO_LONGITUDE  "2.35"

// Development builds only: where the city table (data/places.bin) is read,
// instead of main's copy in the public repository; releases always read
// their own tag's. A server that answers Range requests, e.g. the Mac
// serving the repository with `npx http-server` (python3 -m http.server
// does not). The pieces are checked against the hashes built in, so a
// table that does not match this build is refused.
// #define PLACES_URL_DEV "http://192.168.1.20:8080/data/places.bin"

// Password for firmware updates over Wi-Fi (pio run -e carousel-ota -t upload).
// Empty = OTA disabled: anyone on the network could otherwise flash the board.
#define OTA_PASSWORD ""
