// SPDX-License-Identifier: AGPL-3.0-only
#include "mqtt_logic.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Appends to a bounded buffer; remembers if anything was cut
struct Out {
  char  *buf;
  size_t size, len = 0;
  bool   ok = true;
  Out(char *b, size_t s) : buf(b), size(s) { if (s) buf[0] = '\0'; }
  void add(const char *fmt, ...) {
    if (!ok) return;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf + len, size - len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= size - len) { ok = false; return; }
    len += n;
  }
  // A JSON string: quotes and backslashes escaped, control characters dropped
  void str(const char *s) {
    add("\"");
    for (; *s && ok; s++) {
      if (*s == '"' || *s == '\\') add("\\%c", *s);
      else if ((unsigned char)*s >= 0x20) add("%c", *s);
    }
    add("\"");
  }
};

static const char *const COMPONENT[ENTITY_COUNT] = {"select", "number", "text", "notify", "sensor",
                                                    "select"};
static const char *const OBJECT[ENTITY_COUNT]    = {"screen", "brightness", "text", "notify", "version",
                                                    "image"};
static const char *const NAME[ENTITY_COUNT]      = {"Screen", "Brightness", "Message", "Notification",
                                                    "Firmware", "Image"};

void mqttDiscoveryTopic(char *out, size_t size, MqttEntity e, const char *id) {
  snprintf(out, size, "homeassistant/%s/carousel_%s/%s/config", COMPONENT[e], id, OBJECT[e]);
}

bool mqttDiscoveryConfig(char *out, size_t size, MqttEntity e, const MqttDevice &d) {
  Out o(out, size);
  o.add("{\"name\":\"%s\",\"unique_id\":\"carousel_%s_%s\","
        "\"availability_topic\":\"carousel/%s/status\",", NAME[e], d.id, OBJECT[e], d.id);
  switch (e) {
    case ENTITY_SCREEN:
      o.add("\"command_topic\":\"carousel/%s/screen/set\",\"state_topic\":\"carousel/%s/state\","
            "\"value_template\":\"{{ value_json.screen }}\",\"options\":[", d.id, d.id);
      for (int i = 0; i < d.screenCount; i++) {
        if (i) o.add(",");
        o.str(d.screens[i]);
      }
      o.add("],\"icon\":\"mdi:television-guide\",");
      break;
    case ENTITY_BRIGHTNESS:
      o.add("\"command_topic\":\"carousel/%s/brightness/set\",\"state_topic\":\"carousel/%s/state\","
            "\"value_template\":\"{{ value_json.brightness }}\",\"min\":0,\"max\":%d,\"step\":1,"
            "\"mode\":\"slider\",\"icon\":\"mdi:brightness-6\",", d.id, d.id, d.brightnessLevels - 1);
      break;
    case ENTITY_TEXT:
      o.add("\"command_topic\":\"carousel/%s/text/set\",\"state_topic\":\"carousel/%s/state\","
            "\"value_template\":\"{{ value_json.text }}\",\"max\":95,\"icon\":\"mdi:message-text\",",
            d.id, d.id);
      break;
    case ENTITY_NOTIFY:
      o.add("\"command_topic\":\"carousel/%s/notify\",\"icon\":\"mdi:message-alert\",", d.id);
      break;
    case ENTITY_VERSION:
      o.add("\"state_topic\":\"carousel/%s/state\",\"value_template\":\"{{ value_json.version }}\","
            "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:chip\",", d.id);
      break;
    case ENTITY_IMAGE:
      o.add("\"command_topic\":\"carousel/%s/image/set\",\"state_topic\":\"carousel/%s/state\","
            "\"value_template\":\"{{ value_json.image }}\",\"options\":[", d.id, d.id);
      o.str(MQTT_ALL_IN_TURN);
      for (int i = 0; i < d.imageCount; i++) {
        o.add(",");
        o.str(d.images[i]);
      }
      o.add("],\"icon\":\"mdi:image\",");
      break;
    case ENTITY_COUNT:
      return false;
  }
  o.add("\"device\":{\"identifiers\":[\"carousel_%s\"],\"name\":\"Carousel %s\","
        "\"manufacturer\":\"Serein Design\",\"model\":\"Carousel\",\"sw_version\":", d.id, d.label);
  o.str(d.version);
  o.add("}}");
  return o.ok;
}

bool mqttState(char *out, size_t size, const char *screen, int brightness, const char *text,
               const char *version, const char *image) {
  Out o(out, size);
  o.add("{\"screen\":");
  o.str(screen);
  o.add(",\"brightness\":%d,\"text\":", brightness);
  o.str(text);
  o.add(",\"version\":");
  o.str(version);
  o.add(",\"image\":");
  o.str(image && *image ? image : MQTT_ALL_IN_TURN);
  o.add("}");
  return o.ok;
}
