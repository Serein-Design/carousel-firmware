// SPDX-License-Identifier: AGPL-3.0-only
// The screen gallery: which screen is shown, and what the inputs do to it.
#pragma once

#include <Arduino.h>

#include "feedback_style.h"
#include "i18n.h"
#include "screen_list.h"

extern bool portrait;

// The stored brightness and the board's orientation, on the panel and the
// frame: before the boot's mark lights, so it shows as the screens will
void galleryPanelBegin();
void galleryBegin();    // start screen and running order, after galleryPanelBegin()
void handleInputs();    // encoder, button, tilt
void drawFrame();       // one frame of the current screen

// Remote control (local API, MQTT): callable from any task, applied by the
// display loop on its next frame. Screens are named in English, any case.
bool galleryShowScreen(const char *name);       // false: unknown, off, or drawn for the other orientation
void gallerySetBrightness(int level);           // 0 .. galleryBrightnessLevels() - 1
// A message: a request of its own kind (request.h), a top band over the
// screen that scrolls it twice, or for seconds if longer; from names the
// sender for its icon
void galleryNotify(const char *text, uint16_t color, uint32_t seconds,
                   RequestSource from = SOURCE_API);
// What requestShown() posts; false: the queue was full, nothing shown
struct RequestOrder;
bool galleryRequest(const RequestOrder &o);

// A stored save of the owner's list and interval, applied at once on the next
// frame. Waits up to 1 s for room in the queue; false: not applied before the
// next start (logged).
bool galleryApplyList(const ScreenList &l, uint16_t rotateSec);

// Read-only view of the gallery, for the API
const char *galleryScreenName(int i);           // English
const char *galleryCurrentScreen();
int         galleryBrightness();
int         galleryBrightnessLevels();
// The list as applied, any task: on/off without waiting, the rest copied
// under a spinlock
bool     galleryScreenOn(const char *name);     // English, any case; false: unknown or off
void     galleryList(ScreenList *out, uint16_t *rotateSec);
int      galleryOnNames(const char **out, int max);   // English names of the screens on, list order
uint32_t galleryListGeneration();               // moves when the order or on/off changes
int32_t  galleryRotateNextIn();                 // seconds before the next automatic change, -1: none
