// SPDX-License-Identifier: AGPL-3.0-only
#include "panel_name.h"

#include <stdio.h>
#include <strings.h>

static char mdnsName[16] = HOSTNAME;            // "carousel-695c"
static char unique[24]   = HOSTNAME ".local";   // "carousel-695c.local"
static bool shared       = false;               // answering at carousel.local instead

void panelNameFromMac(const uint8_t mac[6]) {
  snprintf(mdnsName, sizeof(mdnsName), HOSTNAME "-%02x%02x", mac[4], mac[5]);
  snprintf(unique, sizeof(unique), "%s.local", mdnsName);
}

void panelUseSharedName() { shared = true; }

const char *panelMdnsName() { return mdnsName; }
const char *panelHostName() { return shared ? HOSTNAME ".local" : unique; }

bool panelNameIsOurs(const char *host) {
  return strcasecmp(host, HOSTNAME ".local") == 0 || strcasecmp(host, unique) == 0;
}

int panelInfoHead(char *out, size_t size, const char *version) {
  return snprintf(out, size, "{\"device\":\"carousel\",\"name\":\"%s\",\"version\":\"%s\"",
                  mdnsName, version);
}

int panelIdJson(char *out, size_t size, const char *version, const char *mac, const char *reset,
                unsigned long uptimeSec, const char *proof) {
  const int head = panelInfoHead(out, size, version);
  if (head < 0 || (size_t)head >= size) return head;
  const int tail = snprintf(out + head, size - head, ",\"mac\":\"%s\",\"reset\":\"%s\",\"uptime\":%lu%s%s%s}",
                            mac, reset, uptimeSec, proof ? ",\"proof\":\"" : "", proof ? proof : "",
                            proof ? "\"" : "");
  return tail < 0 ? tail : head + tail;
}
