// SPDX-License-Identifier: AGPL-3.0-only
// Home Assistant discovery and state messages: right topics, fields and
// escaping, and nothing written past the buffer.
//
//   pio test -e native
#include <unity.h>

#include "../../src/json_lite.cpp"
#include "../../src/mqtt_logic.cpp"

void setUp() {}
void tearDown() {}

static const char *const SCREENS[] = {"CLOCK", "WEATHER", "ISS DEMO", "IMAGE"};
static const char *const IMAGES[]  = {"cat.gif", "rain.gif"};
static const MqttDevice DEV = {"695c", "695C", "v0.2.0", SCREENS, 4, 6, IMAGES, 2};

// Brackets and braces balance outside strings, and strings close
static bool balanced(const char *j) {
  int depth = 0;
  bool inString = false;
  for (const char *p = j; *p; p++) {
    if (inString) { if (*p == '\\') p++; else if (*p == '"') inString = false; continue; }
    if (*p == '"') inString = true;
    else if (*p == '{' || *p == '[') depth++;
    else if (*p == '}' || *p == ']') { if (--depth < 0) return false; }
  }
  return depth == 0 && !inString;
}

static void test_discovery_topics() {
  char t[96];
  mqttDiscoveryTopic(t, sizeof(t), ENTITY_SCREEN, "695c");
  TEST_ASSERT_EQUAL_STRING("homeassistant/select/carousel_695c/screen/config", t);
  mqttDiscoveryTopic(t, sizeof(t), ENTITY_NOTIFY, "695c");
  TEST_ASSERT_EQUAL_STRING("homeassistant/notify/carousel_695c/notify/config", t);
}

static void test_every_config_is_valid() {
  char j[1024], v[64];
  for (int e = 0; e < ENTITY_COUNT; e++) {
    TEST_ASSERT_TRUE(mqttDiscoveryConfig(j, sizeof(j), (MqttEntity)e, DEV));
    TEST_ASSERT_TRUE_MESSAGE(balanced(j), j);
    TEST_ASSERT_TRUE(jsonValue(j, "unique_id", v, sizeof(v)));
    TEST_ASSERT_TRUE(jsonValue(j, "availability_topic", v, sizeof(v)));
    TEST_ASSERT_EQUAL_STRING("carousel/695c/status", v);
  }
  TEST_ASSERT_TRUE(mqttDiscoveryConfig(j, sizeof(j), ENTITY_SCREEN, DEV));
  TEST_ASSERT_NOT_NULL(strstr(j, "\"options\":[\"CLOCK\",\"WEATHER\",\"ISS DEMO\",\"IMAGE\"]"));
  TEST_ASSERT_TRUE(jsonValue(j, "command_topic", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("carousel/695c/screen/set", v);
  TEST_ASSERT_NOT_NULL(strstr(j, "\"name\":\"Carousel 695C\""));
  TEST_ASSERT_TRUE(mqttDiscoveryConfig(j, sizeof(j), ENTITY_BRIGHTNESS, DEV));
  TEST_ASSERT_TRUE(jsonValue(j, "max", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("5", v);
}

static void test_state_escapes_text() {
  char j[200], v[64];
  TEST_ASSERT_TRUE(mqttState(j, sizeof(j), "CLOCK", 3, "say \"hi\" \\ ok", "v0.2.0", ""));
  TEST_ASSERT_TRUE(balanced(j));
  TEST_ASSERT_TRUE(jsonValue(j, "text", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("say \"hi\" \\ ok", v);
  TEST_ASSERT_TRUE(jsonValue(j, "brightness", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("3", v);
}

static void test_too_small_buffer_fails_cleanly() {
  char j[40];
  memset(j, 'x', sizeof(j));
  TEST_ASSERT_FALSE(mqttDiscoveryConfig(j, 32, ENTITY_SCREEN, DEV));
  TEST_ASSERT_EQUAL_CHAR('x', j[32]);                 // nothing past the size given
  TEST_ASSERT_TRUE(memchr(j, '\0', 32) != nullptr);   // and still terminated
}

static void test_image_entity() {
  char t[96], j[MQTT_DISCOVERY_MAX], v[64];
  mqttDiscoveryTopic(t, sizeof(t), ENTITY_IMAGE, "695c");
  TEST_ASSERT_EQUAL_STRING("homeassistant/select/carousel_695c/image/config", t);
  TEST_ASSERT_TRUE(mqttDiscoveryConfig(j, sizeof(j), ENTITY_IMAGE, DEV));
  TEST_ASSERT_TRUE_MESSAGE(balanced(j), j);
  TEST_ASSERT_TRUE(jsonValue(j, "name", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("Image", v);
  TEST_ASSERT_TRUE(jsonValue(j, "unique_id", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("carousel_695c_image", v);
  TEST_ASSERT_TRUE(jsonValue(j, "command_topic", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("carousel/695c/image/set", v);
  TEST_ASSERT_TRUE(jsonValue(j, "state_topic", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("carousel/695c/state", v);
  TEST_ASSERT_TRUE(jsonValue(j, "value_template", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("{{ value_json.image }}", v);
  TEST_ASSERT_NOT_NULL(strstr(j, "\"options\":[\"All in turn\",\"cat.gif\",\"rain.gif\"]"));
  // No images yet: the picker still offers all in turn
  MqttDevice none = DEV;
  none.images = nullptr;
  none.imageCount = 0;
  TEST_ASSERT_TRUE(mqttDiscoveryConfig(j, sizeof(j), ENTITY_IMAGE, none));
  TEST_ASSERT_NOT_NULL(strstr(j, "\"options\":[\"All in turn\"]"));
}

static void test_discovery_fits_24_names() {
  // 24 names of 40 characters, the longest the sanitizer lets through
  static char names[24][41];
  const char *list[24];
  for (int i = 0; i < 24; i++) {
    snprintf(names[i], sizeof(names[i]), "%02d%s", i, "-abcdefghijklmnopqrstuvwxyz0123456.gif");
    TEST_ASSERT_EQUAL(40, strlen(names[i]));
    list[i] = names[i];
  }
  MqttDevice d = DEV;
  d.id = "ffff";
  d.label = "FFFF";
  d.version = "v10.20.30-rc.1";
  d.images = list;
  d.imageCount = 24;
  char t[96], j[MQTT_DISCOVERY_MAX];
  mqttDiscoveryTopic(t, sizeof(t), ENTITY_IMAGE, d.id);
  TEST_ASSERT_TRUE(mqttDiscoveryConfig(j, sizeof(j), ENTITY_IMAGE, d));
  TEST_ASSERT_TRUE(balanced(j));
  TEST_ASSERT_NOT_NULL(strstr(j, names[23]));
  // The MQTT client's buffer (2 048 B) holds topic and body in one packet
  TEST_ASSERT_LESS_THAN(2048, strlen(t) + strlen(j));
}

static void test_state_has_image() {
  char j[MQTT_STATE_MAX], v[64];
  TEST_ASSERT_TRUE(mqttState(j, sizeof(j), "IMAGE", 3, "hi", "v0.2.0", "cat.gif"));
  TEST_ASSERT_TRUE(jsonValue(j, "image", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("cat.gif", v);
  // "" is all in turn, under the name the picker offers
  TEST_ASSERT_TRUE(mqttState(j, sizeof(j), "IMAGE", 3, "hi", "v0.2.0", ""));
  TEST_ASSERT_TRUE(jsonValue(j, "image", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING(MQTT_ALL_IN_TURN, v);
  // Escaped like the text
  TEST_ASSERT_TRUE(mqttState(j, sizeof(j), "IMAGE", 3, "hi", "v0.2.0", "a\"b.gif"));
  TEST_ASSERT_TRUE(balanced(j));
  TEST_ASSERT_TRUE(jsonValue(j, "image", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("a\"b.gif", v);
  // The worst case fits: 95 quotes (each escaped) and a 40-character name
  char text[96], name[41];
  memset(text, '"', 95);
  text[95] = '\0';
  memset(name, 'n', 36);
  strcpy(name + 36, ".gif");
  TEST_ASSERT_TRUE(mqttState(j, sizeof(j), "WEATHER", 5, text, "v10.20.30-rc.1", name));
  TEST_ASSERT_TRUE(balanced(j));
  TEST_ASSERT_TRUE(jsonValue(j, "image", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING(name, v);
}

static void test_screen_options_include_image() {
  // The IMAGE screen is offered to the Home Assistant screen picker
  char j[MQTT_DISCOVERY_MAX];
  TEST_ASSERT_TRUE(mqttDiscoveryConfig(j, sizeof(j), ENTITY_SCREEN, DEV));
  TEST_ASSERT_NOT_NULL(strstr(j, "\"IMAGE\""));
}

// The screen picker offers what the caller passes, in that order and nothing
// else: the panel passes the screens that are on, in the owner's order.
static void test_screen_picker_follows_the_list() {
  static const char *const ON[] = {"IMAGE", "CLOCK", "WEATHER"};
  MqttDevice d = DEV;
  d.screens = ON;
  d.screenCount = 3;
  char j[MQTT_DISCOVERY_MAX];
  TEST_ASSERT_TRUE(mqttDiscoveryConfig(j, sizeof(j), ENTITY_SCREEN, d));
  TEST_ASSERT_TRUE_MESSAGE(balanced(j), j);
  TEST_ASSERT_NOT_NULL_MESSAGE(strstr(j, "\"options\":[\"IMAGE\",\"CLOCK\",\"WEATHER\"]"), j);
  TEST_ASSERT_NULL(strstr(j, "ISS DEMO"));
}

// Each screen gives its own retained state, so a change is sent
static void test_state_follows_the_screen() {
  char a[MQTT_STATE_MAX], b[MQTT_STATE_MAX], v[32];
  TEST_ASSERT_TRUE(mqttState(a, sizeof(a), "CLOCK", 3, "hi", "v0.2.0", ""));
  TEST_ASSERT_TRUE(mqttState(b, sizeof(b), "WEATHER", 3, "hi", "v0.2.0", ""));
  TEST_ASSERT_TRUE(strcmp(a, b) != 0);
  TEST_ASSERT_TRUE(jsonValue(a, "screen", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("CLOCK", v);
  TEST_ASSERT_TRUE(jsonValue(b, "screen", v, sizeof(v)));
  TEST_ASSERT_EQUAL_STRING("WEATHER", v);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_discovery_topics);
  RUN_TEST(test_every_config_is_valid);
  RUN_TEST(test_state_escapes_text);
  RUN_TEST(test_too_small_buffer_fails_cleanly);
  RUN_TEST(test_image_entity);
  RUN_TEST(test_discovery_fits_24_names);
  RUN_TEST(test_state_has_image);
  RUN_TEST(test_screen_options_include_image);
  RUN_TEST(test_screen_picker_follows_the_list);
  RUN_TEST(test_state_follows_the_screen);
  return UNITY_END();
}
