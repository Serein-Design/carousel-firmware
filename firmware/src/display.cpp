// SPDX-License-Identifier: AGPL-3.0-only
#include "display.h"

#include <driver/gpio.h>
#include <driver/rtc_cntl.h>
#include <esp_rom_gpio.h>
#include <esp_system.h>
#include <soc/gpio_sig_map.h>
#include <soc/gpio_struct.h>
#include <soc/rtc_cntl_reg.h>

#include "auth.h"
#include "calibration.h"
#include "frame_logic.h"
#include "panel_blank_logic.h"
#include "updater.h"

#ifdef SCREENS_TEST_CONSOLE
#include <Preferences.h>
#include <nvs.h>

#include "screen_list.h"
#include "settings.h"
#include "settings_logic.h"
#endif

static MatrixPanel_I2S_DMA *panel = nullptr;

static Frame     offscreen;
Frame           *display = &offscreen;
static uint16_t *shown   = nullptr;   // what the panel shows, in the frame's layout

bool Frame::allocate() {
  buffer       = (uint16_t *)calloc(PIXEL_COUNT, sizeof(uint16_t));
  buffer_owned = true;
  return buffer != nullptr;
}

void Frame::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (buffer) GFXcanvas16::drawFastVLine(x, y, h, color);
}

void Frame::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (buffer) GFXcanvas16::drawFastHLine(x, y, w, color);
}

static void writePanelPixel(int16_t x, int16_t y, uint16_t color) {
  panel->drawPixel(x, y, color);
}

void displayPush() {
  if (!shown || !display->getBuffer()) return;
  framePush(display->getBuffer(), shown, PANEL_RES_X, PANEL_RES_Y, writePanelPixel);
}

const uint16_t *displayShown() { return shown; }

uint8_t displayRotation() { return display->getRotation(); }

// In DRAM, as everything latchDark() reads: the brownout handler runs it with
// the flash already off.
static DRAM_ATTR const PanelBlankPins BLANK_PINS = {
  OE_PIN, LAT_PIN, CLK_PIN, {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN}
};

// Registers and ROM calls only (no driver code, which lives in flash)
struct PanelGpio {
  inline __attribute__((always_inline)) void level(int8_t pin, bool high) const {
    const uint32_t bit = 1u << (pin & 31);
    if (pin < 32) {
      if (high) GPIO.out_w1ts = bit;
      else      GPIO.out_w1tc = bit;
    } else if (high) {
      GPIO.out1_w1ts.val = bit;
    } else {
      GPIO.out1_w1tc.val = bit;
    }
  }
  inline __attribute__((always_inline)) void take(int8_t pin) const {
    esp_rom_gpio_pad_select_gpio(pin);
    esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false);
    const uint32_t bit = 1u << (pin & 31);
    if (pin < 32) GPIO.enable_w1ts = bit;
    else          GPIO.enable1_w1ts.val = bit;
  }
};

// Takes the panel's pins (from the DMA when it runs), OE off, and latches
// zeros into every column: dark until the DMA drives the pins again, and
// after that whatever OE does while nothing drives it (panel_blank_logic.h).
static void IRAM_ATTR latchDark() {
  panelLatchBlank(BLANK_PINS, PANEL_RES_X * PANEL_CHAIN, PanelGpio());
}

void displayHoldDark() { latchDark(); }

// The ESP32 handles a brownout in an RTC interrupt handler that restarts the
// chip (this revision has no brownout reset), and rtc_isr_register() puts the
// last handler registered first (SLIST_INSERT_HEAD in the framework's
// rtc_module.c): this one runs before the restart, while the panel still has
// power to latch the zeros.
static void IRAM_ATTR blankOnBrownout(void *) { latchDark(); }

void displayBrightness(uint8_t level) {
  if (panel) panel->setBrightness8(level);
}

uint16_t BLACK, WHITE;

uint8_t sinTab[256];

uint16_t colorWheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85)  return display->color565(255 - pos * 3, 0, pos * 3);
  if (pos < 170) { pos -= 85;  return display->color565(0, pos * 3, 255 - pos * 3); }
  pos -= 170;    return display->color565(pos * 3, 255 - pos * 3, 0);
}

// ---------------------------------------------------------------------------
// Panel timing settings, changed from the keyboard over the serial port.
//
// They live in RTC memory, which survives a software reset: change a value,
// the board reboots by itself, and the panel restarts with the new setting.
// No BOOT+EN, no reflashing between two tries.
// ---------------------------------------------------------------------------
// The classic ESP32 gets two clocks only, whatever is asked: 80 MHz / 4 / 2
// = 10 MHz up to 8 MHz asked, 80 / 2 / 2 = 20 MHz above (the library's
// esp32_i2s_parallel_dma.cpp, clkm_div_num). "16 MHz" was 20 MHz as well.
// The library computes its refresh rate from the clock asked, not the real one.
static const uint8_t SPEED_COUNT = 2;
static const HUB75_I2S_CFG::clk_speed SPEEDS[SPEED_COUNT] = {
  HUB75_I2S_CFG::HZ_8M, HUB75_I2S_CFG::HZ_20M
};
static const uint8_t SPEED_REAL_MHZ[SPEED_COUNT] = { 10, 20 };

struct PanelTuning {
  uint32_t magic;      // equals MAGIC when the content is valid
  uint8_t  speed;      // index into SPEEDS[]
  uint8_t  blanking;   // latch_blanking, 1 to 4
  bool     clkphase;
};
static const uint32_t MAGIC = 0x48423735;   // "HB75"

static RTC_NOINIT_ATTR PanelTuning tuning;

static void loadTuning() {
  if (tuning.magic != MAGIC || tuning.speed >= SPEED_COUNT || tuning.blanking < 1 ||
      tuning.blanking > 4) {
    tuning.magic    = MAGIC;
    tuning.speed    = 0;       // 8 MHz asked, 10 MHz real: safest for signal integrity
    tuning.blanking = 1;       // library default
    tuning.clkphase = false;
  }
}

void printPanelTuning() {
  Serial.println();
  Serial.println("--- Panel settings ---");
  Serial.printf("  v : clock speed      = %u MHz\n", SPEED_REAL_MHZ[tuning.speed]);
  Serial.print("  p : clkphase         = "); Serial.println(tuning.clkphase ? "true" : "false");
  Serial.print("  b : latch_blanking   = "); Serial.println(tuning.blanking);
  Serial.print("      refresh rate     = ");
  Serial.print(panel ? panel->calculated_refresh_rate * SPEED_REAL_MHZ[tuning.speed] /
                         (SPEEDS[tuning.speed] / 1000000) : 0);
  Serial.println(" Hz");
  Serial.println("  r : back to defaults");
  Serial.println("  x : reboot, nothing changed");
  Serial.println("  u : check for a signed update and install it");
  Serial.println("  t : print the local API token");
#ifdef SCREENS_TEST_CONSOLE
  Serial.println("  n : NVS statistics (test build)");
  Serial.println("  l : store a crafted screen list (test build)");
  Serial.println("  o : store a list without IMAGE, with an unknown name (test build)");
  Serial.println("  i : store an interval the page does not offer (test build)");
  Serial.println("  w : refuse every later interval write (test build)");
  Serial.println("  k : lock as an unfinished reset, nothing erased (test build)");
  Serial.println("  j : lock as a knob reset, nothing erased (test build)");
  Serial.println("  c : show or hide the calibration screen (test build)");
  Serial.println("  a : Chaud or Ambre on the calibration screen (test build)");
#endif
  Serial.println("  Type the letter, then Enter. v, p, b, r change the setting and reboot.");
  Serial.println("----------------------");
}

#ifdef SCREENS_TEST_CONSOLE
// Development builds only (-DSCREENS_TEST_CONSOLE): crafted stored values,
// NVS statistics, a refused write and a locked start, so the screen list's
// start-up repairs, the settings form's put-back and the locked pages can be
// checked on a panel without opening the case. Each write lands as the next
// start reads it: restart with "x".
static bool hasName(uint8_t i, const char *const Strings::*name) {
  return SCREEN_INFO[i].name == name;
}

static void storeTestText(const char *key, const char *text) {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  const bool ok = p.putString(key, text) == strlen(text);
  p.end();
  Serial.printf("[TEST] %s %s: %s\n", key, ok ? "written" : "NOT written", text);
}

// CODE, SLEEP, RUN and TEXT on, every other screen off: no portrait screen.
static void storeLandscapeOnly() {
  const char *const Strings::*first[] = {&Strings::scrCode, &Strings::scrSleep,
                                         &Strings::scrRun, &Strings::scrText};
  char text[SCREEN_LIST_TEXT_MAX] = "";
  for (auto name : first) {
    for (uint8_t i = 0; i < SCREEN_COUNT; i++)
      if (hasName(i, name)) snprintf(text + strlen(text), sizeof(text) - strlen(text),
                                     "%s%s", *text ? "," : "", screenNameEn(i));
  }
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    bool listed = false;
    for (auto name : first) listed |= hasName(i, name);
    if (!listed) snprintf(text + strlen(text), sizeof(text) - strlen(text), ",-%s", screenNameEn(i));
  }
  storeTestText("scrList", text);
}

// The firmware's order without IMAGE (added by a later update, as it were)
// and a name no firmware has after the mascot.
static void storeUnknownName() {
  char text[SCREEN_LIST_TEXT_MAX] = "";
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    if (hasName(i, &Strings::scrImage)) continue;
    snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s%s", *text ? "," : "",
             screenNameEn(i));
    if (hasName(i, &Strings::scrMascot))
      snprintf(text + strlen(text), sizeof(text) - strlen(text), ",NOPE");
  }
  storeTestText("scrList", text);
}

static bool runTestCommand(char c) {
  switch (c) {
    // The calibration screen: shown at once, nothing stored, no restart
    case 'c':
      calibrationToggle();
      Serial.printf("[TEST] calibration %s\n", calibrationShown() ? "shown" : "hidden");
      return true;
    case 'a':
      calibrationFlipPalette();
      Serial.printf("[TEST] calibration palette %s\n",
                    &calibrationPalette(false) == &paletteFor(true) ? "Ambre" : "Chaud");
      return true;
    case 'n': {
      nvs_stats_t st;
      if (nvs_get_stats(nullptr, &st) == ESP_OK)
        Serial.printf("[TEST] NVS entries used %u free %u total %u\n", (unsigned)st.used_entries,
                      (unsigned)st.free_entries, (unsigned)st.total_entries);
      else
        Serial.println("[TEST] NVS statistics unreadable");
      break;
    }
    case 'l': storeLandscapeOnly(); break;
    case 'o': storeUnknownName();   break;
    case 'i': {
      Preferences p;
      p.begin(NVS_NAMESPACE, false);
      const bool ok = p.putUShort("rotateSec", 45) != 0;
      p.end();
      Serial.printf("[TEST] rotateSec %s: 45\n", ok ? "written" : "NOT written");
      break;
    }
    case 'w':
      settingsRefuseRotateWrites();
      Serial.println("[TEST] every rotateSec write refused until a restart");
      break;
    case 'k':
    case 'j':
      settingsTestLock(c == 'j');
      Serial.printf("[TEST] panel locked (%s), nothing erased, until a restart\n",
                    c == 'j' ? "knob reset" : "reset marker");
      break;
    default: return false;
  }
  Serial.println("[TEST] send x to restart");
  return true;
}
#endif

static void runCommand(char c) {
#ifdef SCREENS_TEST_CONSOLE
  if (runTestCommand(c)) return;
#endif
  bool changed = true;
  switch (c) {
    case 'v': tuning.speed    = (tuning.speed + 1) % SPEED_COUNT;          break;
    case 'p': tuning.clkphase = !tuning.clkphase;                          break;
    case 'b': tuning.blanking = tuning.blanking >= 4 ? 1 : tuning.blanking + 1; break;
    case 'r': tuning.magic    = 0; loadTuning();                           break;
    case 'x':                                                              break;   // reboot only
    case 'u': updateInstallNow(); Serial.println("Checking for an update..."); return;
    case 't': Serial.printf("API token: %s\n", authToken());              return;   // physical access
    case '?': printPanelTuning();                                          // fallthrough
    default:  changed = false;                                             break;
  }
  if (!changed) return;
  Serial.println(c == 'x' ? "Rebooting..." : "New setting saved, rebooting...");
  Serial.flush();
  delay(50);
  ESP.restart();
}

// A command is one letter alone on its line: "x" then Enter. Single bytes
// used to be enough, so anything arriving on RX ran: line noise with no
// adapter plugged in, or the board's own output looped back by a loose wire,
// which fed the "u" of "update" back in until the watchdog fired.
void serviceSerialConsole() {
  static char    letter = 0;
  static uint8_t length = 0;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (length == 1) runCommand(letter);
      length = 0;
    } else {
      letter = c;
      if (length < 255) length++;
    }
  }
}

// ---------------------------------------------------------------------------

bool displayBegin() {
  loadTuning();

  for (int i = 0; i < 256; i++)
    sinTab[i] = (uint8_t)(127.5f + 127.4f * sinf(i * 2.0f * PI / 256.0f));

  HUB75_I2S_CFG::i2s_pins pins = {
    R1_PIN, G1_PIN, B1_PIN,
    R2_PIN, G2_PIN, B2_PIN,
    A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
    LAT_PIN, OE_PIN, CLK_PIN
  };

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);
  // mxconfig.driver = HUB75_I2S_CFG::FM6126A;   // if the panel stays black

  // Timing settings driven from the serial console (see PanelTuning).
  mxconfig.i2sspeed         = SPEEDS[tuning.speed];
  mxconfig.clkphase         = tuning.clkphase;
  mxconfig.latch_blanking   = tuning.blanking;
  mxconfig.min_refresh_rate = 120;
  // Colour depth is set in platformio.ini:
  // -DPIXEL_COLOR_DEPTH_BITS=6  (8 by default; each bit less sharply raises
  // the refresh rate, at the cost of coarser shades)

  // begin() clears its DMA buffers before it starts them, and setup() latched
  // zeros first (displayHoldDark): the moment its pins change hands, OE low
  // before the first DMA word, has nothing to show. Brightness 0 before
  // begin() is not possible: the library ignores it until begin() has run.
  panel = new MatrixPanel_I2S_DMA(mxconfig);
  if (!panel->begin()) return false;

  // The library drives these pins at full strength (level 3, ~40 mA): very
  // steep edges whose harmonics fall in the Wi-Fi band, a few centimetres
  // from the antenna. Signal at -65 dBm and yet 25 to 60 % packet loss:
  // Claude Code hooks timed out en masse. At 8 MHz, ~10 mA is plenty for a
  // short ribbon cable. If the picture degrades (flickering pixels), go back
  // up to GPIO_DRIVE_CAP_2.
  for (int pin : {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
                  A_PIN, B_PIN, C_PIN, D_PIN, LAT_PIN, OE_PIN, CLK_PIN})
    gpio_set_drive_capability((gpio_num_t)pin, GPIO_DRIVE_CAP_1);

  // Unrotated for good: the frame carries the rotation (see Frame)
  panel->clearScreen();

  // After the DMA buffers, which need the larger blocks: 4 KB for the frame,
  // 4 KB for the copy of what the panel shows, black like the panel now.
  shown = (uint16_t *)calloc(PIXEL_COUNT, sizeof(uint16_t));
  if (!shown || !display->allocate()) {
    Serial.println("[PANEL] no memory for the off-screen frame");
    free(shown);
    shown = nullptr;
    return false;
  }

  BLACK = display->color565(0, 0, 0);
  WHITE = display->color565(255, 255, 255);

  // A restart (the console, an update; not a crash) and a power cut leave
  // the last row latched while the pins float: dark first.
  if (rtc_isr_register(blankOnBrownout, nullptr, RTC_CNTL_BROWN_OUT_INT_ENA_M) != ESP_OK)
    Serial.println("[PANEL] no brownout handler: the power cut may flash a few LEDs");
  esp_register_shutdown_handler(latchDark);
  return true;
}

uint16_t parseColor(const char *s) {
  if (*s == '#') s++;
  char *end;
  const unsigned long rgb = strtoul(s, &end, 16);
  if (strlen(s) != 6 || *end) return display->color565(255, 255, 255);
  return display->color565(rgb >> 16 & 0xFF, rgb >> 8 & 0xFF, rgb & 0xFF);
}
