// SPDX-License-Identifier: AGPL-3.0-only
// What the panel says on MQTT, without the network: Home Assistant discovery
// messages and the panel's state. Pure, checked on the PC by test/test_mqtt.
//
// Topics, with <id> the end of the MAC ("695c"):
//   carousel/<id>/status          "online" / "offline" (last will), retained
//   carousel/<id>/state           {"screen":..,"brightness":..,"text":..,"version":..,"image":..},
//                                 retained
//   carousel/<id>/screen/set      a screen name
//   carousel/<id>/brightness/set  a level
//   carousel/<id>/text/set        the TEXT screen's message
//   carousel/<id>/image/set       an image name, or "All in turn"
//   carousel/<id>/notify          plain text, or {"text":..,"color":..,"seconds":..}
// Discovery: homeassistant/<component>/carousel_<id>/<object>/config, retained.
#pragma once

#include <stddef.h>

enum MqttEntity { ENTITY_SCREEN, ENTITY_BRIGHTNESS, ENTITY_TEXT, ENTITY_NOTIFY, ENTITY_VERSION,
                  ENTITY_IMAGE, ENTITY_COUNT };

// The image picker's first option, and the state's image when none is chosen.
// English, like the screen names.
static const char *const MQTT_ALL_IN_TURN = "All in turn";
// Buffers: the image picker lists every name in one discovery message (24 x 40
// characters, ~1.5 KB); with its topic it must stay under the client's 2 048 B.
// More images need a larger client buffer too.
static const size_t MQTT_DISCOVERY_MAX = 1792;
// The state with a 95-character text all escaped and a 40-character image name
static const size_t MQTT_STATE_MAX = 384;

struct MqttDevice {
  const char *id;            // "695c", in topics
  const char *label;         // "695C", in the device name shown by Home Assistant
  const char *version;       // firmware version
  const char *const *screens;
  int         screenCount;
  int         brightnessLevels;
  const char *const *images; // the image picker's names, after "All in turn"
  int         imageCount;
};

// Topic of an entity's discovery message
void mqttDiscoveryTopic(char *out, size_t size, MqttEntity e, const char *id);
// Its JSON; false if it does not fit
bool mqttDiscoveryConfig(char *out, size_t size, MqttEntity e, const MqttDevice &d);
// The retained state message; image "" is all in turn. False if it does not fit
bool mqttState(char *out, size_t size, const char *screen, int brightness, const char *text,
               const char *version, const char *image);
