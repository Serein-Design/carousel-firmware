// SPDX-License-Identifier: AGPL-3.0-only
// Every screen of the gallery, in the firmware's order: the one table that
// the gallery (drawing), the screen list (names, orientations, what is
// stored) and the settings page (each screen's own settings) expand, each
// taking the columns it needs. One table, so they cannot drift apart.
//
//   SCREEN(name, draw, orientation, leave, settings)
//     name         its field in Strings: tr().*name on the panel and the page,
//                  the English one in NVS, the API and MQTT
//     draw         one frame (screens/screens.h)
//     orientation  LANDSCAPE, PORTRAIT or BOTH: the gallery it belongs to
//     leave        called when another screen or an overlay takes over, or
//                  nullptr
//     settings     its block on the settings page (web_screens.cpp), or
//                  NO_SETTINGS
//
// A new screen goes at the end: panels with a stored list get it on, after
// the screens they know. Never rename an English name: NVS keeps the list,
// and the start screen, by name.
#pragma once

#define SCREEN_TABLE(SCREEN)                                                                   \
  SCREEN(scrMascot,    screenMascotWalk,    BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrCode,      screenAgentMonitor, LANDSCAPE, nullptr,          CODE_SETTINGS)        \
  SCREEN(scrDance,     screenMascotDance,   BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrSleep,     screenMascotSleep,   LANDSCAPE, nullptr,          NO_SETTINGS)          \
  SCREEN(scrRun,       screenMascotRun,     LANDSCAPE, nullptr,          NO_SETTINGS)          \
  SCREEN(scrLife,      screenLife,          BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrBall,      screenBall,          BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrMatrix,    screenMatrix,        BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrPlasma,    screenPlasma,        BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrFire,      screenFire,          BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrStars,     screenStars,         BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrText,      screenText,          LANDSCAPE, nullptr,          TEXT_SETTINGS)        \
  SCREEN(scrRays,      screenRays,          BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrBars,      screenBars,          BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrBlackHole, screenBlackHole,     BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrCat,       screenCat,           PORTRAIT,  nullptr,          NO_SETTINGS)          \
  SCREEN(scrIss,       screenIss,           BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrClock,     screenClock,         BOTH,      nullptr,          CLOCK_SETTINGS)       \
  SCREEN(scrWeather,   screenWeather,       BOTH,      nullptr,          WEATHER_SETTINGS)     \
  SCREEN(scrDate,      screenDate,          BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrSystem,    screenSystem,        BOTH,      nullptr,          NO_SETTINGS)          \
  SCREEN(scrMoney,     screenMoney,         BOTH,      nullptr,          MONEY_SETTINGS)       \
  SCREEN(scrSocial,    screenSocial,        BOTH,      nullptr,          SOCIAL_SETTINGS)      \
  SCREEN(scrImage,     screenImage,         BOTH,      screenImageLeave, IMAGE_SETTINGS)
