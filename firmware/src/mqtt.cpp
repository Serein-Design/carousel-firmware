// SPDX-License-Identifier: AGPL-3.0-only
#include "mqtt.h"

#include <WiFi.h>
#include <mqtt_client.h>

#include <new>

#include "content.h"
#include "display.h"
#include "gallery.h"
#include "i18n.h"
#include "request.h"
#include "json_lite.h"
#include "mqtt_logic.h"
#include "settings.h"

static esp_mqtt_client_handle_t client = nullptr;
static volatile bool connected = false;
static volatile bool restart   = true;     // (re)start at the next mqttLoop()
static volatile bool resend    = false;    // discovery and state again (HA restarted)
static char id[8], label[8], base[24];
static char topic[96], body[MQTT_DISCOVERY_MAX];   // one discovery message at a time

// --- Discovery ----------------------------------------------------------------

static void publishEntity(MqttEntity e, const MqttDevice &d) {
  mqttDiscoveryTopic(topic, sizeof(topic), e, id);
  if (mqttDiscoveryConfig(body, sizeof(body), e, d))
    esp_mqtt_client_enqueue(client, topic, body, 0, 1, 1, true);
  else
    Serial.printf("[MQTT] discovery of %s does not fit\n", topic);
}

// From mqttLoop(), once connected and whenever Home Assistant comes back online:
// every entity but the image picker, which follows the images (below). With
// screensOnly, the screen picker alone, after the owner changed the list. The
// picker offers the screens that are on, in the owner's order.
static void publishDiscovery(bool screensOnly) {
  static const char *names[48];
  const int n = galleryOnNames(names, 48);
  const MqttDevice d = {id, label, FIRMWARE_VERSION, names, n, galleryBrightnessLevels(), nullptr, 0};
  for (int e = 0; e < ENTITY_COUNT; e++)
    if (e != ENTITY_IMAGE && (!screensOnly || e == ENTITY_SCREEN)) publishEntity((MqttEntity)e, d);
}

// The image picker, with the names of the moment. The list (~1.3 KB) lives on
// the heap only while the message is built. False: busy or out of memory, try
// again at the next check.
static bool publishImages() {
  struct List { ImageInfo items[MAX_IMAGES]; const char *names[MAX_IMAGES]; };
  static bool told = false;                           // one log, not one every 500 ms
  List *l = new (std::nothrow) List;
  if (!l) {
    if (!told) Serial.println("[MQTT] image picker: out of memory, trying again");
    told = true;
    return false;
  }
  told = false;
  const int n = contentList(l->items, MAX_IMAGES);
  if (n >= 0) {
    for (int i = 0; i < n; i++) l->names[i] = l->items[i].name;
    const MqttDevice d = {id, label, FIRMWARE_VERSION, nullptr, 0, 0, l->names, n};
    publishEntity(ENTITY_IMAGE, d);
  }
  delete l;
  return n >= 0;
}

// --- Commands -----------------------------------------------------------------

static void onMessage(const char *topic, const char *payload) {
  const size_t b = strlen(base);
  if (!strcmp(topic, "homeassistant/status")) {
    if (!strcmp(payload, "online")) resend = true;
    return;
  }
  if (strncmp(topic, base, b) || topic[b] != '/') return;
  const char *what = topic + b + 1;
  if (!strcmp(what, "screen/set")) {
    if (galleryShowScreen(payload)) requestShown(REQUEST_COMMAND, SOURCE_HOME_ASSISTANT, payload);
  } else if (!strcmp(what, "brightness/set")) {
    gallerySetBrightness(atoi(payload));
    requestShown(REQUEST_COMMAND, SOURCE_HOME_ASSISTANT, payload);
  } else if (!strcmp(what, "text/set")) {
    toMessageText(payload, settings.message, sizeof(settings.message));
    saveSettings();
  } else if (!strcmp(what, "image/set")) {
    // A stored name or all in turn; anything else is ignored. MQTT never
    // adds, replaces or deletes an image.
    if (contentChoose(strcmp(payload, MQTT_ALL_IN_TURN) ? payload : "")) {
      galleryShowScreen("IMAGE");
      requestShown(REQUEST_IMAGE, SOURCE_HOME_ASSISTANT, payload);
    }
  } else if (!strcmp(what, "notify")) {
    // Plain text, or {"text":..,"color":..,"seconds":..}
    char text[160], color[12] = "", seconds[8] = "";
    if (payload[0] != '{') {
      strncpy(text, payload, sizeof(text) - 1);
      text[sizeof(text) - 1] = '\0';
    } else if (!jsonValue(payload, "text", text, sizeof(text))) {
      return;
    }
    if (payload[0] == '{') {
      jsonValue(payload, "color", color, sizeof(color));
      jsonValue(payload, "seconds", seconds, sizeof(seconds));
    }
    galleryNotify(text, parseColor(color), seconds[0] ? atoi(seconds) : 10, SOURCE_HOME_ASSISTANT);
  }
}

static void onEvent(void *, esp_event_base_t, int32_t eventId, void *data) {
  const esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)eventId) {
    case MQTT_EVENT_CONNECTED: {
      connected = true;
      char t[48];
      snprintf(t, sizeof(t), "%s/status", base);
      esp_mqtt_client_enqueue(client, t, "online", 0, 1, 1, true);
      snprintf(t, sizeof(t), "%s/+/set", base);
      esp_mqtt_client_subscribe(client, t, 1);
      snprintf(t, sizeof(t), "%s/notify", base);
      esp_mqtt_client_subscribe(client, t, 1);
      esp_mqtt_client_subscribe(client, "homeassistant/status", 1);
      resend = true;
      Serial.println("[MQTT] connected");
      break;
    }
    case MQTT_EVENT_DISCONNECTED:
      if (connected) Serial.println("[MQTT] disconnected");
      connected = false;
      break;
    case MQTT_EVENT_DATA: {
      // Commands are short: a message split over several events is not one
      if (ev->total_data_len != ev->data_len || ev->topic_len <= 0) break;
      char topic[64], payload[320];
      if (ev->topic_len >= (int)sizeof(topic) || ev->data_len >= (int)sizeof(payload)) break;
      memcpy(topic, ev->topic, ev->topic_len);
      topic[ev->topic_len] = '\0';
      memcpy(payload, ev->data, ev->data_len);
      payload[ev->data_len] = '\0';
      onMessage(topic, payload);
      break;
    }
    default:
      break;
  }
}

// --- Life cycle ---------------------------------------------------------------

static void start() {
  if (client) {
    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    client = nullptr;
    connected = false;
  }
  if (!settings.mqttHost[0]) return;                  // MQTT off

  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(id, sizeof(id), "%02x%02x", mac[4], mac[5]);
  snprintf(label, sizeof(label), "%02X%02X", mac[4], mac[5]);
  snprintf(base, sizeof(base), "carousel/%s", id);
  char clientId[24], will[40];
  snprintf(clientId, sizeof(clientId), "carousel_%s", id);
  snprintf(will, sizeof(will), "%s/status", base);

  // The client copies these strings
  esp_mqtt_client_config_t cfg = {};
  cfg.host        = settings.mqttHost;
  cfg.port        = settings.mqttPort;
  cfg.client_id   = clientId;
  cfg.username    = settings.mqttUser[0] ? settings.mqttUser : nullptr;
  cfg.password    = settings.mqttPass[0] ? settings.mqttPass : nullptr;
  cfg.lwt_topic   = will;
  cfg.lwt_msg     = "offline";
  cfg.lwt_qos     = 1;
  cfg.lwt_retain  = 1;
  cfg.keepalive   = 60;
  cfg.buffer_size = 2048;                             // discovery messages are ~1 KB
  client = esp_mqtt_client_init(&cfg);
  if (!client) { Serial.println("[MQTT] cannot start"); return; }
  esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, onEvent, nullptr);
  esp_mqtt_client_start(client);
  Serial.printf("[MQTT] connecting to %s:%u as %s\n", settings.mqttHost, settings.mqttPort, clientId);
}

void mqttLoop() {
  if (restart && WiFi.status() == WL_CONNECTED) {
    restart = false;
    start();
  }
  if (!connected) return;

  static uint32_t checkedAt = 0, sentAt = 0, imagesAt = 0, listAt = 0;
  static bool imagesDue = false;
  static char last[MQTT_STATE_MAX];
  if (resend) {
    resend = false;
    listAt = galleryListGeneration();                 // read before the names: a save in between sends again
    publishDiscovery(false);
    imagesDue = true;
    last[0] = '\0';                                   // and the state again
  }
  if (millis() - checkedAt < 500) return;
  checkedAt = millis();
  // The screen picker follows every save of the order or of a screen on or off
  const uint32_t list = galleryListGeneration();
  if (list != listAt) {
    listAt = list;
    publishDiscovery(true);
  }
  // The picker follows every add, replace and delete within 5 s
  const uint32_t generation = contentGeneration();
  if (imagesDue || generation != imagesAt) {
    if (publishImages()) {
      imagesDue = false;
      imagesAt = generation;
    }
  }
  char state[MQTT_STATE_MAX], image[IMAGE_NAME_MAX + 1];
  contentChoice(image, sizeof(image));
  if (!mqttState(state, sizeof(state), galleryCurrentScreen(), galleryBrightness(), settings.message,
                 FIRMWARE_VERSION, image))
    return;
  // On every change, and every 5 min in case a retained message got lost
  if (!strcmp(state, last) && millis() - sentAt < 300000) return;
  char t[40];
  snprintf(t, sizeof(t), "%s/state", base);
  esp_mqtt_client_enqueue(client, t, state, 0, 1, 1, true);
  strcpy(last, state);
  sentAt = millis();
}

void mqttApplySettings() { restart = true; }
bool mqttConnected() { return connected; }
