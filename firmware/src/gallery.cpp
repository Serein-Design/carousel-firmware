// SPDX-License-Identifier: AGPL-3.0-only
#include "gallery.h"

#include <Preferences.h>
#include <atomic>
#include <time.h>

#include "calibration.h"
#include "agent_bridge.h"
#include "content.h"
#include "display.h"
#include "gallery_logic.h"
#include "hold.h"
#include "input.h"
#include "network.h"
#include "overlay.h"
#include "palette.h"
#include "reach_logic.h"
#include "request.h"
#include "settings.h"
#include "status_strip.h"
#include "transition.h"
#include "updater.h"
#include "screens/mascot.h"
#include "screens/screens.h"

// What the gallery draws: screen_table.h, the draw and leave columns, by
// firmware index (SCREEN_INFO holds the names and orientations)
struct Screen {
  void (*draw)(uint32_t frame, bool init);
  // Called when another screen or an overlay takes over after this one was
  // drawn: a screen holding memory gives it back. nullptr: nothing to do.
  void (*leave)();
};
#define SCREEN_ROW(name, draw, orientation, leave, settings) {draw, leave},
static const Screen SCREENS[SCREEN_COUNT] = {SCREEN_TABLE(SCREEN_ROW)};
#undef SCREEN_ROW

static const uint8_t LEVELS[]    = {10, 25, 50, 100, 160, 255};
static const uint8_t LEVEL_COUNT = sizeof(LEVELS) / sizeof(LEVELS[0]);
// 100 out of 255: bright enough on a desk by day, and far from the current
// peak that can make a weak supply reset the board.
static uint8_t brightnessLevel   = 3;

#define NIGHT_WAKE_MS 60000UL   // touching the knob at night: day brightness for 1 min
static uint32_t touchedAt = 0;  // last turn or press, 0 = never

// A long press shows where the pages are (screens/reach.cpp); while held, the
// hold line fills toward it
static PressTimer knobPress;
static ReachCard  reach;

static void openReach() {
  if (reachCardBuild()) {
    reach.show(millis());
    Serial.println("[REACH] card shown");
  } else {
    Serial.println("[REACH] no network yet: no card");
  }
}

// Orders from other tasks, applied by loop(), the only writer of the gallery
enum CommandType : uint8_t { CMD_SCREEN, CMD_BRIGHTNESS, CMD_REQUEST, CMD_LIST };
// ScreenList's fields without its initialisers, so it can share the union
struct ListOrder {
  uint8_t  order[MAX_SCREENS];
  uint32_t on;
  uint16_t rotateSec;
};
struct Command {
  CommandType type;
  int16_t     value;            // screen index or brightness level
  union {
    RequestOrder request;       // CMD_REQUEST
    ListOrder    list;          // CMD_LIST
  };
};
static QueueHandle_t commands = nullptr;

// An image sent with Show plays once, then the screen it took over comes back
static uint8_t  screenBefore = 0;      // the screen before the last requested change
static int      returnTo     = -1;     // that screen while IMAGE plays for a request, -1: none
static uint32_t returnTurns  = 0, returnSince = 0;

bool portrait = false;

// The screen on show, the rotation, the hold and the save; the feedback
// over it after a brightness change or a turn of the knob
static ScreenCursor cursor;
static Feedback     feedback;
static bool     resetScreen = true;    // clear the screen and reset the animation
static bool     booting     = true;    // the boot screen has not given way yet
static uint32_t frame       = 0;

// The passage to the screen that comes next: its direction once the screen
// changed by the knob, a request or the rotation (+1 pushes left, -1 right),
// 0 for a cut (a tilt, an overlay that ends). Taken by drawFrame().
static int8_t   passDir     = 0;
static Passage  passage;

// What other tasks read of the gallery. loop() writes the cursor's list only
// under listMux, so galleryList() copies a whole one; the rest are atomics
// published by loop().
static portMUX_TYPE listMux = portMUX_INITIALIZER_UNLOCKED;
static std::atomic<uint8_t>  shownScreen{0};
static std::atomic<uint32_t> onMask{0};
static std::atomic<uint32_t> listGeneration{0};
static std::atomic<int32_t>  rotateNextIn{-1};
static void   (*drawnLeave)() = nullptr;   // leave hook of the screen drawn last

// Another screen or an overlay takes over: the screen drawn last lets go
static void leaveDrawn() {
  if (!drawnLeave) return;
  drawnLeave();
  drawnLeave = nullptr;
}

// Does screen i belong to the gallery currently shown?
static bool inGallery(uint8_t i, bool isPortrait) { return screenFits(i, isPortrait); }

static bool onAgentMonitor() { return SCREENS[cursor.current()].draw == screenAgentMonitor; }

// Screen and brightness survive a restart. The screen is kept by its English
// name: an index would point elsewhere once an update adds screens.
static const char *KEY_SCREEN = "screen";
static const char *KEY_LEVEL  = "brightness";

// The stored screen's firmware index; -1: none (first boot: the cursor
// starts on the clock) or a name this firmware does not have
static int loadView() {
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  brightnessLevel = min<uint8_t>(p.getUChar(KEY_LEVEL, brightnessLevel), LEVEL_COUNT - 1);
  const String name = p.getString(KEY_SCREEN, "");
  p.end();
  for (uint8_t i = 0; i < SCREEN_COUNT; i++)
    if (name == screenNameEn(i)) return i;
  return -1;
}

static void saveView() {
  const char *name = screenNameEn(cursor.current());
  Preferences p;
  // Every step checked: a full NVS or one that would not open says so, and
  // the next start then shows what was stored before
  bool saved = p.begin(NVS_NAMESPACE, false);
  if (saved) {
    saved = p.putUChar(KEY_LEVEL, brightnessLevel) != 0;
    saved &= p.putString(KEY_SCREEN, name) == strlen(name);
    p.end();
  }
  Serial.print(saved ? "[VIEW] saved " : "[VIEW] not saved "); Serial.println(name);
}

// After the cursor's list or screen changed
static void publishList() {
  onMask = cursor.list().on;
}
static void publishScreen() { shownScreen = cursor.current(); }

static bool nightNow() {
  struct tm t;
  if (!timeValid || !getLocalTime(&t, 0)) return false;
  return inHourWindow(t.tm_hour, settings.nightStart, settings.nightEnd);
}

static int appliedBrightness = 255;   // as last sent to the panel

// The night hours' palette, whether or not a touch woke the brightness; its
// low variant while the panel is that dim
const Palette &palette() { return paletteFor(nightNow(), lowLight(appliedBrightness)); }

static bool dimmedForNight() {
  return nightNow() && (!touchedAt || millis() - touchedAt > NIGHT_WAKE_MS);
}

// The level the panel is heading for: the day's, or at night the night's,
// eased in and out over the first and last minutes of the night hours. A touch
// waking the panel is at the day level at once.
static uint8_t targetBrightness() {
  const uint8_t day = LEVELS[brightnessLevel];
  struct tm t;
  if (!dimmedForNight() || !getLocalTime(&t, 0)) return day;
  return nightLevel(t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec, settings.nightStart,
                    settings.nightEnd, day, settings.nightBrightness);
}

// Moves to the target over BRIGHTNESS_RAMP_MS, from wherever the panel is, a
// step at most every BRIGHTNESS_STEP_MS: setBrightness8 rebuilds the panel's
// tables, so a step is applied only when the level changes.
static void applyBrightness() {
  static int shown = -1, from = 0, to = 0;
  static uint32_t rampAt = 0, stepAt = 0;
  const uint32_t now = millis();
  const uint8_t target = targetBrightness();
  if (shown < 0) {
    from = to = shown = target;   // the first level is not a change
    displayBrightness(target);
    appliedBrightness = target;
    return;
  }
  if (target != to) {
    from   = shown;
    to     = target;
    rampAt = now;
  }
  const uint8_t b = rampValue(from, to, now - rampAt, BRIGHTNESS_RAMP_MS);
  if (b == shown || (b != to && now - stepAt < BRIGHTNESS_STEP_MS)) return;
  displayBrightness(b);
  stepAt = now;
  shown = appliedBrightness = b;
}

// The mascot screen carries the mascot's own name when a private one is built in
static const char *shownName(uint8_t i) {
  return SCREENS[i].draw == screenMascotWalk && mascotName() ? mascotName() : tr().*SCREEN_INFO[i].name;
}

// Over the screen just drawn, in the owner's style (Panel): the brightness
// level, or where the knob is in the running order
static void drawFeedback(uint32_t now) {
  const float k = feedback.shown(now);
  if (k <= 0) return;
  if (feedback.kind() == FeedbackKind::BRIGHTNESS) {
    drawBrightnessFeedback(*display, settings.brightnessFeedback, brightnessLevel, LEVEL_COUNT, k);
    return;
  }
  const uint8_t current = cursor.current();
  const int prev = cursor.neighbour(-1), next = cursor.neighbour(1);
  const KnobView v = {shownName(current), prev >= 0 ? shownName((uint8_t)prev) : nullptr,
                      next >= 0 ? shownName((uint8_t)next) : nullptr, knobPlace(cursor.list(), current, portrait),
                      feedback.dir(), feedback.since(now)};
  drawKnobFeedback(*display, settings.knobFeedback, v, k);
}

static void applyOrientation() {
  // Landscape: 2 = 180 degrees, the panel is mounted upside down in the case.
  // Portrait: 1 = 90 degrees, already the right way up, do not touch.
  // On the frame, not the panel: the push maps the frame onto it as it is.
  display->setRotation(portrait ? 1 : 2);
  display->clearScreen();
  Serial.print("[TILT] ");
  Serial.print(portrait ? "PORTRAIT" : "LANDSCAPE");
  Serial.print("  canvas ");
  Serial.print(W()); Serial.print("x"); Serial.println(H());
}

// A stored save: list and interval at once, never stored again from here
static void applyList(const ListOrder &o) {
  ScreenList l;
  memcpy(l.order, o.order, sizeof(l.order));
  l.on = o.on;
  const ScreenList before = cursor.list();
  portENTER_CRITICAL(&listMux);
  const bool moved = cursor.apply(l, o.rotateSec, millis());
  portEXIT_CRITICAL(&listMux);
  publishList();
  // Compared with what the cursor holds now: a refused list changes nothing
  if (!screenListSameView(before, cursor.list())) listGeneration++;
  // The screen on show was turned off: the next one, without feedback or save
  if (moved) {
    resetScreen = true;
    passDir     = 1;
  }
}

// Found on first use, once the names are in place: never at static init
static int codeIndex()  { static const int i = screenFind("CODE");  return i; }
static int imageIndex() { static const int i = screenFind("IMAGE"); return i; }

// What a request adds to its change (request.h). The route posted the change
// before the request, on the same queue: it is applied by now.
static void takeRequest(const RequestOrder &o, uint32_t now) {
  RequestPlan p = o.plan;
  if (p.onStage && codeIndex() >= 0 && cursor.current() != codeIndex()) {
    if (cursor.bringOn((uint8_t)codeIndex(), now)) {
      resetScreen = true;
      passDir     = 1;                   // with the owner's transition, as any change asked for
      returnTo = -1;
    } else {
      p.chip = true;                     // CODE off, or not drawn this way: the chip says it
    }
  }
  if (p.backAfterPlay && imageIndex() >= 0 && cursor.current() == imageIndex() &&
      screenBefore != imageIndex()) {
    returnTo    = screenBefore;
    returnTurns = imageTurns();
    returnSince = now;
  }
  requestOverlayStart(o, p, now);
}

// The image a request showed has played once (its turn ended, or the longest
// turn passed without one): back to the screen before, unless something else
// moved the panel meanwhile
static void returnAfterImage(uint32_t now) {
  if (returnTo < 0) return;
  if (cursor.current() != imageIndex()) {
    returnTo = -1;
    return;
  }
  if (imageTurns() == returnTurns && now - returnSince < TURN_MAX_MS) return;
  if (cursor.request((uint8_t)returnTo, now)) {
    resetScreen = true;
    passDir     = 1;
  }
  returnTo = -1;
}

static void applyCommands() {
  Command c;
  while (commands && xQueueReceive(commands, &c, 0) == pdTRUE) {
    switch (c.type) {
      case CMD_SCREEN:
        screenBefore = cursor.current();
        returnTo = -1;                   // a newer change: no way back
        // Refused when tilted or turned off since it was asked
        if (cursor.request((uint8_t)c.value, millis())) {
          resetScreen = true;
          passDir     = 1;
        }
        break;
      case CMD_BRIGHTNESS:
        brightnessLevel = (uint8_t)c.value;
        cursor.brightnessChanged(millis());
        feedback.show(FeedbackKind::BRIGHTNESS, millis());
        break;
      case CMD_LIST:
        applyList(c.list);
        break;
      case CMD_REQUEST:
        takeRequest(c.request, millis());
        break;
    }
  }
}

void handleInputs() {
  applyCommands();
  returnAfterImage(millis());
  encoderButton.update();
  tilt.update();

  int32_t detents = readEncoderDetents();
  Press pressed = knobPress.update(encoderButton.closed(), millis());
  if (reach.shown(millis()) && (detents || pressed != Press::NONE)) {
    // The card leaves at the knob's next move, which does nothing else
    reach.hide();
    touchedAt = millis();
    detents   = 0;
    pressed   = Press::NONE;
  }
  // Where the pages are after joining a network saved from the setup page
  if (networkTakeJoinedNew()) openReach();
  if (pressed == Press::LONG) {
    touchedAt = millis();
    cursor.press(touchedAt);
    openReach();
  }

  // Encoder: changes screen, through the owner's list, the screens that are
  // on and drawn for this orientation
  if (detents != 0) {
    touchedAt = millis();
    const uint8_t before = cursor.current();
    cursor.turn(detents, touchedAt);
    feedback.show(FeedbackKind::KNOB, touchedAt, detents > 0 ? 1 : -1);
    resetScreen = true;
    if (cursor.current() != before) passDir = transitionDirection(detents);
    Serial.print("[ENC] "); Serial.print(screenNameEn(cursor.current()));
    Serial.print(" ("); Serial.print(portrait ? "portrait" : "landscape"); Serial.println(")");
  }

  // Button, a short press: contextual. On the agent monitor it cycles
  // through sessions, everywhere else it sets the brightness. Told on
  // release, as only then is it known not to be a long press.
  if (pressed == Press::SHORT) {
    const int n = activeSessionCount();
    const bool wasDimmed = dimmedForNight();
    touchedAt = millis();
    cursor.press(touchedAt);   // the screen stays a while, whatever the press did

    if (wasDimmed) {
      // At night the first press only wakes the panel up, to the level shown
      feedback.show(FeedbackKind::BRIGHTNESS, touchedAt);
    } else if (onAgentMonitor() && n > 1) {
      // Rank of the session shown, then take the next one
      int rank = 0;
      for (int r = 0; r < n; r++) if (nthActiveSession(r) == focusedSession) { rank = r; break; }
      const int next = nthActiveSession((rank + 1) % n);
      if (next >= 0) {
        focusedSession    = next;
        sessionSwitchedAt = millis();     // when the focus moved
        manualPickAt      = millis();     // auto focus respects it for a while
        // Above all no resetScreen here: it resets the frame counter, which
        // would restart the screen on every switch.
        Serial.print("[BT] session ");
        Serial.println(sessions[focusedSession].name);
      }
    } else {
      brightnessLevel = (brightnessLevel + 1) % LEVEL_COUNT;
      cursor.brightnessChanged(touchedAt);
      feedback.show(FeedbackKind::BRIGHTNESS, touchedAt);
      Serial.print("[BT] brightness ");
      Serial.println(LEVELS[brightnessLevel]);
    }
  }

  // Tilt: switches landscape / portrait, hence the gallery
  bool p = tilt.closed();
  if (p != portrait) {
    portrait = p;
    applyOrientation();
    // The current screen may not exist in the new gallery: keep it if it
    // does, otherwise take the list's first one that is on and fits.
    cursor.tilt(portrait, millis());
    feedback.show(FeedbackKind::KNOB, millis());   // where the new gallery starts
    resetScreen = true;
    passDir     = 0;   // the frame turned: a cut
  }
  publishScreen();
}

// The stored screen loadView() read, for galleryBegin(); -1: none
static int storedView = -1;

void galleryPanelBegin() {
  storedView = loadView();
  applyBrightness();

  // Start orientation: the one the board is already in
  portrait = tilt.closed();
  applyOrientation();
}

void galleryBegin() {
  commands = xQueueCreate(4, sizeof(Command));

  Serial.print("Screens: "); Serial.println(SCREEN_COUNT);
  Serial.println("Encoder = screen | button = brightness, held = address | tilt = orientation");

  // The owner's list from loadSettings(). A stored screen that is off or
  // drawn for the other orientation gives way to the list's first that fits.
  portENTER_CRITICAL(&listMux);
  cursor.begin(settings.screens, settings.rotateSec, portrait, storedView, millis());
  portEXIT_CRITICAL(&listMux);
  publishList();
  publishScreen();
  Serial.print("Start screen: ");
  Serial.println(screenNameEn(cursor.current()));
}

// An unfinished factory reset locks the panel (settings.cpp). The language
// setting cannot be read then (NVS closed), so the message comes in English,
// then in French, 3 s each, in the alert colour; then the Wi-Fi
// setup screen for 6 s, whose access point leads to the same message.
#define LOCKED_TEXT_MS 3000UL
#define LOCKED_CYCLE_MS (4 * LOCKED_TEXT_MS)
static void drawResetFailed() {
  const uint32_t t = millis() % LOCKED_CYCLE_MS;
  if (t >= 2 * LOCKED_TEXT_MS) {
    drawWifiSetupScreen(frame++);
    return;
  }
  drawLockedState(STRINGS[t < LOCKED_TEXT_MS ? LANG_EN : LANG_FR]);
}

// Screens the rotation passes over: IMAGE with nothing to play in this
// orientation (the knob still reaches it)
static uint32_t cannotRotate() {
  const int image = imageIndex();
  return image >= 0 && !contentPlayable(portrait) ? 1u << image : 0;
}

// The screen changes: the frame on show becomes the outgoing one, unless the
// passage is a cut (dir 0, the owner's choice, or no room for its copies)
static void beginPassage(int8_t dir) {
  uint16_t *shown = display->getBuffer();
  if (!dir || !shown) {
    passage.cancel();
    return;
  }
  passage.begin(settings.transition, dir, shown, PANEL_RES_X, PANEL_RES_Y,
                display->getRotation(), millis());
}

// The status strip's agent mark from the active sessions, as the bridge shows them
static AgentMark sessionsMark(uint32_t now) {
  AgentState states[MAX_SESSIONS];
  int n = 0;
  for (int r = 0; n < MAX_SESSIONS; r++) {
    const int i = nthActiveSession(r);
    if (i < 0) break;
    states[n++] = displayedState(i, now);
  }
  return agentMark(states, n);
}

void drawFrame() {
  applyBrightness();
  const uint32_t now = millis();
  // The screen chosen by hand or the brightness, once the knob settled
  if (cursor.takeSave(now)) saveView();

  bool init = false;
  if (resetScreen) {
    beginPassage(passDir);
    passDir = 0;
    display->clearScreen();
    frame       = 0;
    init        = true;
    resetScreen = false;
  }
  const uint32_t skip = cannotRotate();
  bool covered = true;   // an overlay hides the screen: the rotation waits

  // Over everything, in this order: update progress, sign-in code (a locked
  // panel's page is reached with it too). Then, on a locked panel, RESET
  // FAILED and nothing else. Then the Wi-Fi setup screen while it is open,
  // the boot sequence until the panel is ready, the reach card while it
  // shows, or else the current screen, with what a request shows on top of
  // it and the hold line while the knob is held.
  if (drawUpdateProgress() || drawPairingCode()) {
    leaveDrawn();
    resetScreen = true;
  } else if (panelLocked()) {
    leaveDrawn();
    drawResetFailed();
  } else if (wifiSetupMode || restartAt) {
    leaveDrawn();
    booting = false;                     // the setup screen is the boot's way out
    drawWifiSetupScreen(frame++);
  } else if (booting && drawBootScreen(now)) {
    leaveDrawn();
    resetScreen = true;                  // the first screen starts afresh after it
  } else if (reach.shown(now)) {
    leaveDrawn();
    drawReachCard(reach.page(now));
    resetScreen = true;                  // the screen starts afresh afterwards
#ifdef SCREENS_TEST_CONSOLE
  } else if (calibrationShown()) {
    // In place of the screen until c again; its two pages take turns
    leaveDrawn();
    display->clearScreen();
    drawCalibration(*display, calibrationPalette(lowLight(appliedBrightness)), now / 4000 % 2);
    resetScreen = true;                  // the screen starts afresh afterwards
#endif
  } else {
    covered = false;
    if (booting) {
      // The start names its screen once the boot screen gave way
      booting = false;
      feedback.show(FeedbackKind::KNOB, now);
    }
    if (cursor.tick(now, false, skip)) {
      // An automatic change: no feedback, nothing stored, pushed left
      beginPassage(1);
      display->clearScreen();
      frame = 0;
      init  = true;
      publishScreen();
      Serial.print("[ROT] "); Serial.println(screenNameEn(cursor.current()));
    }
    const uint8_t current = cursor.current();
    if (drawnLeave != SCREENS[current].leave) leaveDrawn();
    bootStageBack(display->getBuffer());
    passage.beforeDraw(display->getBuffer());
    SCREENS[current].draw(frame, init);
    // Part of the screen's frame, so a passage carries it with the screen
    drawStatusStrip(*display, palette(), current, cursor.rotationFill(now, skip), sessionsMark(now));
    passage.afterDraw(display->getBuffer(), display->getRotation(), now, palette().dim);
    drawnLeave = SCREENS[current].leave;
    frame++;
    // Overlays come after the passage: they draw above it. The hold line
    // last: the knob held is what the hand is doing now.
    drawFeedback(now);
    requestOverlayDraw(*display, palette(), now);
    drawHoldLine(*display, holdFill(knobPress.heldMs(now)));
    drawBootOpening(now);                // last: the stage opens on all of it
  }
  if (covered) {
    // An alert or an overlay took the frame: it came in with a cut
    passage.cancel();
    cursor.tick(now, true, skip);
  }
  rotateNextIn = cursor.nextInSeconds(now, skip);
  // Whatever was drawn above, the panel gets the pixels that changed, once
  displayPush();
}

// --- Remote control ---------------------------------------------------------

bool galleryShowScreen(const char *name) {
  const int i = screenFind(name);
  if (i < 0 || !commands || !inGallery((uint8_t)i, portrait) || !((onMask >> i) & 1u)) return false;
  Command c = {};
  c.type  = CMD_SCREEN;
  c.value = (int16_t)i;
  return xQueueSend(commands, &c, 0) == pdTRUE;
}

bool galleryApplyList(const ScreenList &l, uint16_t rotateSec) {
  // The loop indexes its draw functions with this list: refused here, and
  // again by the cursor in the loop
  if (!screenListValid(l) || !rotateOffered(rotateSec)) {
    Serial.println("[SCREENS] list refused: invalid");
    return false;
  }
  Command c = {};
  c.type = CMD_LIST;
  memcpy(c.list.order, l.order, sizeof(c.list.order));
  c.list.on        = l.on;
  c.list.rotateSec = rotateSec;
  // The loop drains the queue every frame: a full one for 1 s is not expected
  if (commands && xQueueSend(commands, &c, pdMS_TO_TICKS(1000)) == pdTRUE) return true;
  Serial.println("[SCREENS] list not applied");
  return false;
}

void gallerySetBrightness(int level) {
  if (!commands) return;
  Command c = {};
  c.type  = CMD_BRIGHTNESS;
  c.value = (int16_t)constrain(level, 0, LEVEL_COUNT - 1);
  xQueueSend(commands, &c, 0);
}

void galleryNotify(const char *text, uint16_t color, uint32_t seconds, RequestSource from) {
  requestShown(REQUEST_MESSAGE, from, text, color, constrain(seconds, 1UL, 600UL));
}

bool galleryRequest(const RequestOrder &o) {
  if (!commands) return false;
  Command c = {};
  c.type    = CMD_REQUEST;
  c.request = o;
  return xQueueSend(commands, &c, 0) == pdTRUE;
}

const char *galleryScreenName(int i)       { return screenNameEn((uint8_t)i); }
const char *galleryCurrentScreen()         { return galleryScreenName(shownScreen); }
int         galleryBrightness()            { return brightnessLevel; }
int         galleryBrightnessLevels()      { return LEVEL_COUNT; }

bool galleryScreenOn(const char *name) {
  const int i = screenFind(name);
  return i >= 0 && ((onMask >> i) & 1u);
}

void galleryList(ScreenList *out, uint16_t *rotateSec) {
  portENTER_CRITICAL(&listMux);
  if (out) *out = cursor.list();
  if (rotateSec) *rotateSec = cursor.rotateSec();
  portEXIT_CRITICAL(&listMux);
}

int galleryOnNames(const char **out, int max) {
  int n = 0;
  portENTER_CRITICAL(&listMux);
  const ScreenList &l = cursor.list();
  for (uint8_t p = 0; p < SCREEN_COUNT && n < max; p++)
    if (screenOn(l, l.order[p])) out[n++] = screenNameEn(l.order[p]);
  portEXIT_CRITICAL(&listMux);
  return n;
}

uint32_t galleryListGeneration() { return listGeneration; }
int32_t  galleryRotateNextIn()   { return rotateNextIn; }
