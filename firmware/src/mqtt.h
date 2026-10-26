// SPDX-License-Identifier: AGPL-3.0-only
// MQTT link, for Home Assistant and any other home automation. Set a broker
// on the settings page and Home Assistant finds the panel by itself (MQTT
// discovery): a screen picker, a brightness slider, the TEXT message, a
// notify entity and the firmware version. Topics in mqtt_logic.h.
// ESP-IDF's own client (esp-mqtt), already in the firmware: no new library.
#pragma once

#include <Arduino.h>

void mqttLoop();             // every frame: (re)starts the client, publishes state changes
void mqttApplySettings();    // broker settings changed (web task): reconnect
bool mqttConnected();
