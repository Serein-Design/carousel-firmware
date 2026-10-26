// SPDX-License-Identifier: AGPL-3.0-only
// The panel's name on the network: carousel.local while no other device
// holds it, else its own carousel-xxxx.local (the end of its MAC, lowercase,
// as its access point CAROUSEL-XXXX), so two panels on one network stay
// apart (network.cpp). One name at a time: the ESP-IDF 4.4 mDNS does not
// answer a delegated second name (tried on the panel).
// Hardware-free: the MAC is handed in once, at boot.
#pragma once

#include <cstddef>
#include <cstdint>

#define HOSTNAME "carousel"   // the shared mDNS name, the unique one's prefix; also the OTA name

// The unique name from the MAC's last two bytes: "carousel-695c".
void panelNameFromMac(const uint8_t mac[6]);
// The unique mDNS host name, "carousel-695c"; "carousel" until the MAC is
// known. /id and /api/info name the panel by it, whichever name answers.
const char *panelMdnsName();
// The panel took carousel.local: no other device answered to it.
void panelUseSharedName();
// The address to type, as the panel answers now: "carousel.local", or
// "carousel-695c.local" when another device holds carousel.local. For the
// screens and the pages.
const char *panelHostName();
// A Host header naming this panel: carousel.local or its own name, any case.
bool panelNameIsOurs(const char *host);

// GET /id: {"device":"carousel","name":..,"version":..,"mac":..,"reset":..,"uptime":..}
// and, when given, "proof" last (asked with a nonce: auth.h)
constexpr size_t ID_JSON_SIZE = 264;
int panelIdJson(char *out, size_t size, const char *version, const char *mac, const char *reset,
                unsigned long uptimeSec, const char *proof = nullptr);
// The start of GET /api/info, without the closing brace: {"device":..,"name":..,"version":..
constexpr size_t INFO_HEAD_SIZE = 120;
int panelInfoHead(char *out, size_t size, const char *version);
