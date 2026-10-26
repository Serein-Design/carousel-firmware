// SPDX-License-Identifier: AGPL-3.0-only
// Every language is complete, and every panel string fits its width budget
// in the 5x7 font (6 px per character) and uses only characters it can draw.
//
//   pio test -e native
#include <unity.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "../../src/i18n.cpp"

static_assert(sizeof(Strings) % sizeof(const char *) == 0, "Strings holds only const char *");
static const size_t FIELD_COUNT = sizeof(Strings) / sizeof(const char *);

struct Budget { const char *const Strings::*field; size_t maxChars; const char *label; };
#define B(f, n) {&Strings::f, n, #f}
static const Budget PANEL[] = {
  B(scrMascot, 10), B(scrCode, 10), B(scrDance, 10), B(scrSleep, 10), B(scrRun, 10),
  B(scrLife, 10), B(scrBall, 10), B(scrMatrix, 10), B(scrPlasma, 10), B(scrFire, 10),
  B(scrStars, 10), B(scrText, 10), B(scrRays, 10), B(scrBars, 10), B(scrBlackHole, 10),
  B(scrCat, 10), B(scrIss, 10), B(scrClock, 10), B(scrWeather, 10), B(scrDate, 10),
  B(scrSystem, 10), B(scrMoney, 10), B(scrSocial, 10), B(scrImage, 5),
  B(weatherLoading, 10), B(weekPrefix, 1), B(near, 5),
  B(land, 5), B(ocean, 5), B(setup, 10), B(factoryReset, 5),
};   // the Wi-Fi setup screen's and the reach card's words: test_reach; DATE's,
     // SYSTEM's, MONEY's and SOCIAL's, in their faces: test_info_logic
#undef B

void setUp() {}
void tearDown() {}

static void test_every_string_is_present() {
  for (int l = 0; l < LANG_COUNT; l++) {
    const char *const *fields = reinterpret_cast<const char *const *>(&STRINGS[l]);
    for (size_t i = 0; i < FIELD_COUNT; i++) {
      char msg[64];
      snprintf(msg, sizeof(msg), "language %d, field #%zu", l, i);
      TEST_ASSERT_NOT_NULL_MESSAGE(fields[i], msg);
      TEST_ASSERT_TRUE_MESSAGE(fields[i][0] != '\0', msg);
    }
  }
}

static bool panelSafe(const char *s) {
  for (; *s; s++)
    if (*s < 0x20 || *s > 0x7E) return false;
  return true;
}

static void test_panel_strings_fit_and_are_drawable() {
  for (int l = 0; l < LANG_COUNT; l++) {
    for (const Budget &b : PANEL) {
      const char *s = STRINGS[l].*b.field;
      char msg[96];
      snprintf(msg, sizeof(msg), "%s.%s = \"%s\"", STRINGS[l].code, b.label, s);
      TEST_ASSERT_TRUE_MESSAGE(strlen(s) <= b.maxChars, msg);
      TEST_ASSERT_TRUE_MESSAGE(panelSafe(s), msg);
    }
    for (const char *s : STRINGS[l].imgOther)       TEST_ASSERT_TRUE(strlen(s) <= 10 && panelSafe(s));
    for (const char *s : STRINGS[l].imgNoStore)     TEST_ASSERT_TRUE(strlen(s) <= 10 && panelSafe(s));
    for (const char *s : STRINGS[l].imgOtherTall)   TEST_ASSERT_TRUE(strlen(s) <= 5 && panelSafe(s));
    for (const char *s : STRINGS[l].imgNoStoreTall) TEST_ASSERT_TRUE(strlen(s) <= 5 && panelSafe(s));
  }
}

// IMAGE is the NVS, MQTT and API name of the screen: the same in every language
static void test_image_screen_name() {
  for (int l = 0; l < LANG_COUNT; l++) {
    TEST_ASSERT_EQUAL_STRING_MESSAGE("IMAGE", STRINGS[l].scrImage, STRINGS[l].code);
    TEST_ASSERT_TRUE(strlen(STRINGS[l].scrImage) <= 5);
  }
}

static void assertLines(const char *const *got, std::initializer_list<const char *> want,
                        const char *label) {
  size_t i = 0;
  for (const char *w : want) TEST_ASSERT_EQUAL_STRING_MESSAGE(w, got[i++], label);
}

// The words of the two other states without an image (no image at all is test_states)
static void test_image_panel_texts() {
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  assertLines(en.imgOther, {"NO IMAGE", "FOR 64X32"}, "en.imgOther");
  assertLines(en.imgNoStore, {"NO STORAGE", "SEE WEB"}, "en.imgNoStore");
  assertLines(en.imgOtherTall, {"NO", "IMAGE", "FOR", "32X64"}, "en.imgOtherTall");
  assertLines(en.imgNoStoreTall, {"NO", "STORE", "SEE", "WEB"}, "en.imgNoStoreTall");
  assertLines(fr.imgOther, {"SANS IMAGE", "EN 64X32"}, "fr.imgOther");
  assertLines(fr.imgNoStore, {"PAS DISPO", "VOIR WEB"}, "fr.imgNoStore");
  assertLines(fr.imgOtherTall, {"SANS", "IMAGE", "EN", "32X64"}, "fr.imgOtherTall");
  assertLines(fr.imgNoStoreTall, {"PAS", "DISPO", "VOIR", "WEB"}, "fr.imgNoStoreTall");
}

// The locked page's text (the panel's own LOCKED words are measured in test_states)
static void test_reset_failed_texts() {
  for (int l = 0; l < LANG_COUNT; l++) {
    TEST_ASSERT_TRUE(strlen(STRINGS[l].webResetFailed) > 40);   // a paragraph, not a word
    // Read by whoever started the reset: what to do, and where the USB
    // reinstall is explained; never a "previous owner"
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(STRINGS[l].webResetFailed, "Build and flash"),
                                 STRINGS[l].code);
    // The locked page is not the Wi-Fi page: its own title
    TEST_ASSERT_TRUE_MESSAGE(strcmp(STRINGS[l].resetFailedTitle, STRINGS[l].wifiTitle) != 0,
                             STRINGS[l].code);
  }
  TEST_ASSERT_NULL(strstr(STRINGS[LANG_EN].webResetFailed, "owner"));
  TEST_ASSERT_NULL(strstr(STRINGS[LANG_FR].webResetFailed, "propri"));
}

// A knob reset that could not write its marker is not resumed at the next
// start: its page says to run it again, never that the panel retries alone
static void test_reset_failed_knob_text() {
  TEST_ASSERT_EQUAL_STRING(
      "The reset did not finish and will not start again by itself. Hold the knob at the next "
      "start to run it again, and do not hand the panel over until it restarts on the Wi-Fi "
      "setup screen.",
      STRINGS[LANG_EN].webResetFailedKnob);
  TEST_ASSERT_EQUAL_STRING(
      "La remise \xC3\xA0 z\xC3\xA9ro n'a pas abouti et ne repartira pas seule. Maintenez le "
      "bouton au prochain d\xC3\xA9marrage pour la relancer, et ne donnez pas le panneau tant "
      "qu'il ne red\xC3\xA9marre pas sur l'\xC3\xA9" "cran de configuration Wi-Fi.",
      STRINGS[LANG_FR].webResetFailedKnob);
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    TEST_ASSERT_EQUAL_PTR(t.webResetFailed, resetFailedText(t, false));   // marker lock
    TEST_ASSERT_EQUAL_PTR(t.webResetFailedKnob, resetFailedText(t, true));
  }
}

// The printf conversions of a format, in order: "%s needs %lu KB" -> "%s%lu"
static std::string conversions(const char *f) {
  std::string out;
  for (const char *p = f; (p = strchr(p, '%')); ) {
    const char *end = p + 1;
    while (*end == 'l') end++;
    if (*end) end++;
    out.append(p, end);
    p = end;
  }
  return out;
}

// Every language passes the same arguments to the same formats; a mismatch is
// undefined behaviour in snprintf, so it is pinned per field
static void test_image_formats() {
  struct Fmt { const char *const Strings::*field; const char *want; const char *label; };
#define F(f, w) {&Strings::f, w, #f}
  static const Fmt FORMATS[] = {
    F(webImgUsedOf, "%lu%lu"), F(webImgOnPanel, "%d"), F(webImgAdded, "%s"),
    F(webImgReplaced, "%s"), F(webImgNotGif, "%s%s"), F(webImgNotGifOther, "%s"),
    F(webImgWrongSize, "%s%u%u"), F(webImgNoRoom, "%s%lu%lu"), F(webImgTooMany, "%s"),
    F(webImgTooBig, ""), F(webImgFailed, ""), F(webImgNoFile, ""), F(webImgNotAdded, ""),
    F(webImgAddHint, ""), F(webImgShowHint, ""), F(webImgNaText, ""), F(webResetFailed, ""),
    F(webResetFailedKnob, ""), F(webNotSaved, ""), F(webSavedLater, ""),
  };
#undef F
  for (int l = 0; l < LANG_COUNT; l++)
    for (const Fmt &f : FORMATS) {
      char msg[64];
      snprintf(msg, sizeof(msg), "%s.%s", STRINGS[l].code, f.label);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(f.want, conversions(STRINGS[l].*f.field).c_str(), msg);
    }
}

static const char NNBSP[] = "\xE2\x80\xAF", NBSP[] = "\xC2\xA0";

// French typography: a narrow no-break space (U+202F)
// before : ; ? ! and a no-break space (U+00A0) before the unit Ko
static void test_french_spacing() {
  static const char *const Strings::*NEW_WEB[] = {
    &Strings::webResetFailed, &Strings::webResetFailedKnob, &Strings::webNotSaved, &Strings::webSavedLater,
    &Strings::webHookAddress, &Strings::webStates, &Strings::webImages, &Strings::webImgSpace, &Strings::webImgUsedOf,
    &Strings::webKb, &Strings::webImgNone, &Strings::webImgAdd, &Strings::webImgAddHint,
    &Strings::webImgUpload, &Strings::webImgOnPanel, &Strings::webImgLandscape,
    &Strings::webImgPortrait, &Strings::webImgDelete, &Strings::webImgShows, &Strings::webImgAll,
    &Strings::webImgShowHint, &Strings::webImgShow, &Strings::webImgAdded,
    &Strings::webImgReplaced, &Strings::webImgNotAdded, &Strings::webImgNotGif,
    &Strings::webImgNotGifOther, &Strings::webImgWrongSize, &Strings::webImgNoRoom,
    &Strings::webImgTooMany, &Strings::webImgTooBig, &Strings::webImgFailed,
    &Strings::webImgNoFile, &Strings::webImgNa, &Strings::webImgNaText, &Strings::webImgNaLink,
  };
  for (const auto field : NEW_WEB) {
    const char *s = STRINGS[LANG_FR].*field;
    for (const char *p = s; *p; p++) {
      if (strchr(":;?!", *p))
        TEST_ASSERT_TRUE_MESSAGE(p - s >= 3 && memcmp(p - 3, NNBSP, 3) == 0, s);
      if (p > s && strncmp(p, "Ko", 2) == 0)
        TEST_ASSERT_TRUE_MESSAGE(p - s >= 2 && memcmp(p - 2, NBSP, 2) == 0, s);
    }
  }
  TEST_ASSERT_EQUAL_STRING("Ko", STRINGS[LANG_FR].webKb);
}

// The Screens section's formats: the same conversions in every language
static void test_screens_formats() {
  struct Fmt { const char *const Strings::*field; const char *want; const char *label; };
#define F(f, w) {&Strings::f, w, #f}
  static const Fmt FORMATS[] = {
    F(webInOrder, "%u%u"), F(webMoveUp, "%s"), F(webMoveDown, "%s"), F(webTitleValue, "%s%s"),
    F(webChangeScreen, ""), F(webRotateHint, ""),
    F(webOrderHint, ""), F(webLandscapeOnly, ""), F(webPortraitOnly, ""),
    F(webRefusedEmpty, ""), F(webRefusedPortrait, ""), F(webRefusedLandscape, ""),
    F(webImgOff, ""), F(webMessage, ""), F(webCityMatches, "%s"), F(webCityNone, ""),
  };
#undef F
  for (int l = 0; l < LANG_COUNT; l++) {
    for (const Fmt &f : FORMATS) {
      char msg[64];
      snprintf(msg, sizeof(msg), "%s.%s", STRINGS[l].code, f.label);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(f.want, conversions(STRINGS[l].*f.field).c_str(), msg);
    }
    for (const char *r : STRINGS[l].webRotate)
      TEST_ASSERT_EQUAL_STRING_MESSAGE("", conversions(r).c_str(), r);
  }
  // The text field's label no longer names the screen: the Screens list does
  TEST_ASSERT_EQUAL_STRING("Scrolling text", STRINGS[LANG_EN].webMessage);
  TEST_ASSERT_EQUAL_STRING("Texte d\xC3\xA9" "filant", STRINGS[LANG_FR].webMessage);
}

// Seven intervals, knob only first; a number never parts from its unit
static void test_rotate_choices() {
  static_assert(sizeof(Strings::webRotate) / sizeof(const char *) == 7, "seven intervals");
  TEST_ASSERT_EQUAL_STRING("With the knob only", STRINGS[LANG_EN].webRotate[0]);
  TEST_ASSERT_EQUAL_STRING("Every 10\xC2\xA0s", STRINGS[LANG_EN].webRotate[1]);
  TEST_ASSERT_EQUAL_STRING("Every minute", STRINGS[LANG_EN].webRotate[4]);
  TEST_ASSERT_EQUAL_STRING("Every 5\xC2\xA0minutes", STRINGS[LANG_EN].webRotate[6]);
  TEST_ASSERT_EQUAL_STRING("Au bouton seulement", STRINGS[LANG_FR].webRotate[0]);
  TEST_ASSERT_EQUAL_STRING("Toutes les 30\xC2\xA0s", STRINGS[LANG_FR].webRotate[3]);
  TEST_ASSERT_EQUAL_STRING("Toutes les 2\xC2\xA0minutes", STRINGS[LANG_FR].webRotate[5]);
  for (int l = 0; l < LANG_COUNT; l++) {
    int withNumber = 0;
    for (const char *r : STRINGS[l].webRotate) {
      TEST_ASSERT_TRUE_MESSAGE(r && r[0], STRINGS[l].code);
      for (const char *p = r; *p; p++)
        if (*p >= '0' && *p <= '9' && !(p[1] >= '0' && p[1] <= '9')) {
          TEST_ASSERT_EQUAL_MEMORY_MESSAGE(NBSP, p + 1, 2, r);
          withNumber++;
        }
    }
    TEST_ASSERT_EQUAL_MESSAGE(5, withNumber, STRINGS[l].code);
  }
}

// French typography in the Screens section: U+202F before : ; ? ! and inside « »
static void test_french_spacing_screens() {
  static const char *const Strings::*SCREENS_WEB[] = {
    &Strings::webChangeScreen, &Strings::webRotateHint,
    &Strings::webInOrder, &Strings::webOrderHint, &Strings::webLandscapeOnly,
    &Strings::webPortraitOnly, &Strings::webMoveUp, &Strings::webMoveDown,
    &Strings::webRefusedEmpty, &Strings::webRefusedPortrait, &Strings::webRefusedLandscape,
    &Strings::webTitleValue,
    &Strings::webImgOff, &Strings::webMessage,
    &Strings::webNightHint,   // the Display section, reshaped with the page
    &Strings::webStyle, &Strings::webClockShows,
  };
  std::vector<const char *> texts;
  for (const auto field : SCREENS_WEB) texts.push_back(STRINGS[LANG_FR].*field);
  const Strings &fr = STRINGS[LANG_FR];
  for (const char *r : fr.webRotate) texts.push_back(r);
  for (const char *r : fr.codeStyles) texts.push_back(r);
  for (const char *r : fr.codeStyleHints) texts.push_back(r);
  for (const char *r : fr.clockStyles) texts.push_back(r);
  for (const char *r : fr.clockStyleHints) texts.push_back(r);
  for (const char *r : fr.weatherStyles) texts.push_back(r);
  for (const char *r : fr.weatherStyleHints) texts.push_back(r);
  for (const char *r : fr.clockShows) texts.push_back(r);
  for (const char *r : fr.clockShowsShort) texts.push_back(r);
  static const char OPEN[] = "\xC2\xAB", CLOSE[] = "\xC2\xBB";
  for (const char *s : texts) {
    for (const char *p = s; *p; p++) {
      if (strchr(":;?!", *p))
        TEST_ASSERT_TRUE_MESSAGE(p - s >= 3 && memcmp(p - 3, NNBSP, 3) == 0, s);
      if (strncmp(p, OPEN, 2) == 0)
        TEST_ASSERT_TRUE_MESSAGE(strncmp(p + 2, NNBSP, 3) == 0, s);
      if (strncmp(p, CLOSE, 2) == 0)
        TEST_ASSERT_TRUE_MESSAGE(p - s >= 3 && memcmp(p - 3, NNBSP, 3) == 0, s);
    }
  }
  // the 30 s of the hint is a number and its unit too
  TEST_ASSERT_NOT_NULL(strstr(STRINGS[LANG_FR].webRotateHint, "30\xC2\xA0s"));
  TEST_ASSERT_NOT_NULL(strstr(STRINGS[LANG_EN].webRotateHint, "30\xC2\xA0s"));
}

// The theatre pages' shell and Wi-Fi page: the same conversions in every
// language, and the tab names in the tabs' order
static void test_theatre_formats_shell() {
  struct Fmt { const char *const Strings::*field; const char *want; const char *label; };
#define F(f, w) {&Strings::f, w, #f}
  static const Fmt FORMATS[] = {
    F(webSkip, ""), F(webNavName, ""), F(webSetupBrand, ""), F(webTitle, "%s"),
    F(wifiAnother, ""), F(wifiName, ""), F(wifiMsgSaved, "%s"), F(wifiNoneYet, ""),
  };
#undef F
  static_assert(sizeof(Strings::webTabs) / sizeof(const char *) == 4, "four tabs");
  for (int l = 0; l < LANG_COUNT; l++) {
    for (const Fmt &f : FORMATS) {
      char msg[64];
      snprintf(msg, sizeof(msg), "%s.%s", STRINGS[l].code, f.label);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(f.want, conversions(STRINGS[l].*f.field).c_str(), msg);
    }
    for (const char *tab : STRINGS[l].webTabs)
      TEST_ASSERT_EQUAL_STRING_MESSAGE("", conversions(tab).c_str(), tab);
  }
  static const char *const EN_TABS[] = {"Stage", "Running order", "Images", "Panel"};
  static const char *const FR_TABS[] = {"Sc\xC3\xA8ne", "Ordre de passage", "Images", "Panneau"};
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_STRING(EN_TABS[i], STRINGS[LANG_EN].webTabs[i]);
    TEST_ASSERT_EQUAL_STRING(FR_TABS[i], STRINGS[LANG_FR].webTabs[i]);
  }
  TEST_ASSERT_EQUAL_STRING("Another network, by name", STRINGS[LANG_EN].wifiAnother);
  TEST_ASSERT_EQUAL_STRING("Un autre r\xC3\xA9seau, par son nom", STRINGS[LANG_FR].wifiAnother);
  TEST_ASSERT_EQUAL_STRING("Network name", STRINGS[LANG_EN].wifiName);
  TEST_ASSERT_EQUAL_STRING("No network found yet.", STRINGS[LANG_EN].wifiNoneYet);
  TEST_ASSERT_EQUAL_STRING("Aucun r\xC3\xA9seau trouv\xC3\xA9 pour l'instant.",
                           STRINGS[LANG_FR].wifiNoneYet);
  TEST_ASSERT_EQUAL_STRING("R\xC3\xA9glage de Carousel", STRINGS[LANG_FR].webSetupBrand);
}

// The sign-in and locked pages: no conversion anywhere (no number is ever
// filled in: the code only shows on the panel), the copy as written
static void test_theatre_formats_access() {
  static const char *const Strings::*ACCESS[] = {
    &Strings::signInLive, &Strings::signInCaption, &Strings::signInFrame, &Strings::signInReload,
    &Strings::lockedLive, &Strings::signInTitle, &Strings::signInHint, &Strings::signInCode,
    &Strings::signInButton, &Strings::signInWrong,
  };
  for (int l = 0; l < LANG_COUNT; l++)
    for (const auto field : ACCESS)
      TEST_ASSERT_EQUAL_STRING_MESSAGE("", conversions(STRINGS[l].*field).c_str(),
                                       STRINGS[l].*field);
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("code on the panel now", en.signInLive);
  TEST_ASSERT_EQUAL_STRING("code sur le panneau en ce moment", fr.signInLive);
  TEST_ASSERT_EQUAL_STRING("The digits stay on the panel; this page never shows them.",
                           en.signInCaption);
  TEST_ASSERT_EQUAL_STRING("Les chiffres restent sur le panneau\xE2\x80\xAF; cette page ne les "
                           "montre jamais.", fr.signInCaption);
  TEST_ASSERT_EQUAL_STRING("The panel shows CODE, a 6-digit code (hidden here) and a bar for the "
                           "time left", en.signInFrame);
  TEST_ASSERT_EQUAL_STRING("Le panneau montre CODE, un code \xC3\xA0 6 chiffres (cach\xC3\xA9 ici) "
                           "et une barre pour le temps restant", fr.signInFrame);
  TEST_ASSERT_EQUAL_STRING("No code on the panel? Reload this page: opening it puts a new code on "
                           "the panel for 2\xC2\xA0minutes.", en.signInReload);
  TEST_ASSERT_EQUAL_STRING("Pas de code sur le panneau\xE2\x80\xAF? Rechargez cette page\xE2\x80\xAF: "
                           "l'ouvrir affiche un nouveau code pendant 2\xC2\xA0minutes.",
                           fr.signInReload);
  // One language per table: the locked page shows both, the French one marked
  TEST_ASSERT_EQUAL_STRING("on the panel now", en.lockedLive);
  TEST_ASSERT_EQUAL_STRING("sur le panneau", fr.lockedLive);
}

// The error pages: one title and one sentence per kind, in ErrorKind order,
// no conversion; errBack %s: the page's tab name. The copy as written.
static void test_theatre_formats_errors() {
  static_assert(sizeof(Strings::errTitle) / sizeof(const char *) == 5, "five kinds");
  static_assert(sizeof(Strings::errText) / sizeof(const char *) == 5, "five kinds");
  for (int l = 0; l < LANG_COUNT; l++) {
    for (int k = 0; k < 5; k++) {
      TEST_ASSERT_EQUAL_STRING_MESSAGE("", conversions(STRINGS[l].errTitle[k]).c_str(),
                                       STRINGS[l].errTitle[k]);
      TEST_ASSERT_EQUAL_STRING_MESSAGE("", conversions(STRINGS[l].errText[k]).c_str(),
                                       STRINGS[l].errText[k]);
    }
    TEST_ASSERT_EQUAL_STRING("%s", conversions(STRINGS[l].errBack).c_str());
  }
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("Page not found", en.errTitle[0]);
  TEST_ASSERT_EQUAL_STRING("Page introuvable", fr.errTitle[0]);
  TEST_ASSERT_EQUAL_STRING("Back to %s", en.errBack);
  TEST_ASSERT_EQUAL_STRING("Retour \xC3\xA0 %s", fr.errBack);
}

// Stage: the same conversions in every language, the interval words aligned
// with the offered intervals, the states in AgentState order, the copy as
// written
static void test_theatre_formats_stage() {
  struct Fmt { const char *const Strings::*field; const char *want; const char *label; };
#define F(f, w) {&Strings::f, w, #f}
  static const Fmt FORMATS[] = {
    F(stgLive, ""), F(stgPlace, "%u%u"), F(stgNowSr, ""), F(stgNext, "%s%lu"),
    F(stgKnobOnly, ""), F(stgNoOther, ""), F(stgPreviewAlt, "%s"), F(stgStill, ""),
    F(stgAllScreens, "%u"), F(stgSessions, ""), F(stgSessionsHint, ""), F(stgAgoSec, "%lu"),
    F(stgAgoMin, "%lu"), F(stgAgoHour, "%lu"), F(stgSetUpHooks, ""),
  };
#undef F
  static_assert(sizeof(Strings::stgEvery) == sizeof(Strings::webRotate) - sizeof(const char *),
                "one interval word per offered interval but the knob");
  static_assert(sizeof(Strings::stgStates) / sizeof(const char *) == 7, "seven session states");
  for (int l = 0; l < LANG_COUNT; l++) {
    for (const Fmt &f : FORMATS) {
      char msg[64];
      snprintf(msg, sizeof(msg), "%s.%s", STRINGS[l].code, f.label);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(f.want, conversions(STRINGS[l].*f.field).c_str(), msg);
    }
    for (const char *e : STRINGS[l].stgEvery)
      TEST_ASSERT_EQUAL_STRING_MESSAGE("", conversions(e).c_str(), e);
    for (const char *e : STRINGS[l].stgStates)
      TEST_ASSERT_EQUAL_STRING_MESSAGE("", conversions(e).c_str(), e);
  }
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("on stage", en.stgLive);
  TEST_ASSERT_EQUAL_STRING("en sc\xC3\xA8ne", fr.stgLive);
  TEST_ASSERT_EQUAL_STRING("screen %u of %u", en.stgPlace);
  TEST_ASSERT_EQUAL_STRING("\xC3\xA9" "cran %u sur %u", fr.stgPlace);
  TEST_ASSERT_EQUAL_STRING(", en sc\xC3\xA8ne maintenant", fr.stgNowSr);
  TEST_ASSERT_EQUAL_STRING("Next: %s in %lu\xC2\xA0s", en.stgNext);
  TEST_ASSERT_EQUAL_STRING("Ensuite\xE2\x80\xAF: %s dans %lu\xC2\xA0s", fr.stgNext);
  // Each %s is followed by the rest of the line: the page splits there
  for (int l = 0; l < LANG_COUNT; l++) TEST_ASSERT_NOT_NULL(strstr(STRINGS[l].stgNext, "%s "));
  static const char *const EN_EVERY[] = {
    "a new screen every 10\xC2\xA0s", "a new screen every 20\xC2\xA0s",
    "a new screen every 30\xC2\xA0s", "a new screen every minute",
    "a new screen every 2\xC2\xA0minutes", "a new screen every 5\xC2\xA0minutes"};
  static const char *const FR_EVERY[] = {
    "un nouvel \xC3\xA9" "cran toutes les 10\xC2\xA0s",
    "un nouvel \xC3\xA9" "cran toutes les 20\xC2\xA0s",
    "un nouvel \xC3\xA9" "cran toutes les 30\xC2\xA0s",
    "un nouvel \xC3\xA9" "cran chaque minute",
    "un nouvel \xC3\xA9" "cran toutes les 2\xC2\xA0minutes",
    "un nouvel \xC3\xA9" "cran toutes les 5\xC2\xA0minutes"};
  for (int i = 0; i < 6; i++) {
    TEST_ASSERT_EQUAL_STRING(EN_EVERY[i], en.stgEvery[i]);
    TEST_ASSERT_EQUAL_STRING(FR_EVERY[i], fr.stgEvery[i]);
  }
  TEST_ASSERT_EQUAL_STRING("Changes with the knob only", en.stgKnobOnly);
  TEST_ASSERT_EQUAL_STRING("Change au bouton seulement", fr.stgKnobOnly);
  TEST_ASSERT_EQUAL_STRING("No other screen to change to in this orientation", en.stgNoOther);
  TEST_ASSERT_EQUAL_STRING("Aucun autre \xC3\xA9" "cran \xC3\xA0 montrer dans cette orientation",
                           fr.stgNoOther);
  TEST_ASSERT_EQUAL_STRING("What the panel shows now: %s", en.stgPreviewAlt);
  TEST_ASSERT_EQUAL_STRING("Ce que le panneau montre en ce moment\xE2\x80\xAF: %s", fr.stgPreviewAlt);
  TEST_ASSERT_EQUAL_STRING("Une image prise \xC3\xA0 l'ouverture de la page. Rechargez-la pour la "
                           "revoir.", fr.stgStill);
  TEST_ASSERT_EQUAL_STRING("All %u screens and their order", en.stgAllScreens);
  TEST_ASSERT_EQUAL_STRING("Les %u \xC3\xA9" "crans et leur ordre", fr.stgAllScreens);
  TEST_ASSERT_EQUAL_STRING("Sessions de code", fr.stgSessions);
  TEST_ASSERT_EQUAL_STRING("Claude Code sessions the panel hears from right now. They show on the "
                           "CODE screen.", en.stgSessionsHint);
  TEST_ASSERT_EQUAL_STRING("Les sessions Claude Code que le panneau entend en ce moment. Elles "
                           "passent sur l'\xC3\xA9" "cran CODE.", fr.stgSessionsHint);
  static const char *const FR_STATES[] = {"en attente", "r\xC3\xA9" "fl\xC3\xA9" "chit",
                                          "utilise un outil", "vous attend", "erreur",
                                          "termin\xC3\xA9", "\xC3\xA9" "chec"};
  for (int i = 0; i < 7; i++) TEST_ASSERT_EQUAL_STRING(FR_STATES[i], fr.stgStates[i]);
  TEST_ASSERT_EQUAL_STRING("%lu\xC2\xA0s ago", en.stgAgoSec);
  TEST_ASSERT_EQUAL_STRING("%lu\xC2\xA0min ago", en.stgAgoMin);
  TEST_ASSERT_EQUAL_STRING("%lu\xC2\xA0h ago", en.stgAgoHour);
  TEST_ASSERT_EQUAL_STRING("il y a %lu\xC2\xA0s", fr.stgAgoSec);
  TEST_ASSERT_EQUAL_STRING("il y a %lu\xC2\xA0min", fr.stgAgoMin);
  TEST_ASSERT_EQUAL_STRING("il y a %lu\xC2\xA0h", fr.stgAgoHour);
  TEST_ASSERT_EQUAL_STRING("Set up the hooks", en.stgSetUpHooks);
  TEST_ASSERT_EQUAL_STRING("Installer les hooks", fr.stgSetUpHooks);
}

// Stage's actions: the same conversions in every language, the copy as
// written
static void test_theatre_formats_stage_actions() {
  struct Fmt { const char *const Strings::*field; const char *want; const char *label; };
#define F(f, w) {&Strings::f, w, #f}
  static const Fmt FORMATS[] = {
    F(stgBring, ""), F(stgScreen, ""), F(stgLights, ""), F(stgDimmer, ""), F(stgBrighter, ""),
    F(stgSteps, "%d%d"), F(stgNightDims, "%d%d%u%u%s"), F(stgNightDark, "%d%d%u%u"),
    F(stgNightNone, "%d%d"), F(webNightMode, ""), F(stgSay, ""), F(stgSend, ""),
    F(stgSayHint, ""), F(stgTextOff, ""), F(stgTextPortrait, ""), F(stgTextFailed, ""),
    F(stgShowOff, "%s"), F(stgShowOther, "%s"), F(stgShowUnknown, ""), F(stgSent, ""),
    F(stgFailed, ""),
  };
#undef F
  for (int l = 0; l < LANG_COUNT; l++)
    for (const Fmt &f : FORMATS) {
      char msg[64];
      snprintf(msg, sizeof(msg), "%s.%s", STRINGS[l].code, f.label);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(f.want, conversions(STRINGS[l].*f.field).c_str(), msg);
    }
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("Bring on a screen", en.stgBring);
  TEST_ASSERT_EQUAL_STRING("Faire entrer un \xC3\xA9" "cran", fr.stgBring);
  TEST_ASSERT_EQUAL_STRING("Screen", en.stgScreen);
  TEST_ASSERT_EQUAL_STRING("\xC3\x89" "cran", fr.stgScreen);
  TEST_ASSERT_EQUAL_STRING("Brightness", en.stgLights);
  TEST_ASSERT_EQUAL_STRING("Luminosit\xC3\xA9", fr.stgLights);
  TEST_ASSERT_EQUAL_STRING("Dimmer", en.stgDimmer);
  TEST_ASSERT_EQUAL_STRING("Brighter", en.stgBrighter);
  TEST_ASSERT_EQUAL_STRING("Moins fort", fr.stgDimmer);
  TEST_ASSERT_EQUAL_STRING("Plus fort", fr.stgBrighter);
  TEST_ASSERT_EQUAL_STRING("Brightness %d of %d", en.stgSteps);
  TEST_ASSERT_EQUAL_STRING("Luminosit\xC3\xA9 %d sur %d", fr.stgSteps);
  TEST_ASSERT_EQUAL_STRING("Level %d of %d. At night, from %u:00 to %u:00, the panel dims to %s.",
                           en.stgNightDims);
  TEST_ASSERT_EQUAL_STRING("Niveau %d sur %d. La nuit, de %u:00 \xC3\xA0 %u:00, le panneau baisse "
                           "\xC3\xA0 %s.", fr.stgNightDims);
  TEST_ASSERT_EQUAL_STRING("Level %d of %d. At night, from %u:00 to %u:00, the panel goes dark.",
                           en.stgNightDark);
  TEST_ASSERT_EQUAL_STRING("Niveau %d sur %d. La nuit, de %u:00 \xC3\xA0 %u:00, le panneau "
                           "s'\xC3\xA9teint.", fr.stgNightDark);
  TEST_ASSERT_EQUAL_STRING("Level %d of %d. No night mode.", en.stgNightNone);
  TEST_ASSERT_EQUAL_STRING("Niveau %d sur %d. Pas de mode nuit.", fr.stgNightNone);
  TEST_ASSERT_EQUAL_STRING("Night mode", en.webNightMode);
  TEST_ASSERT_EQUAL_STRING("Mode nuit", fr.webNightMode);
  TEST_ASSERT_EQUAL_STRING("Say something", en.stgSay);
  TEST_ASSERT_EQUAL_STRING("Dire quelque chose", fr.stgSay);
  TEST_ASSERT_EQUAL_STRING("Send", en.stgSend);
  TEST_ASSERT_EQUAL_STRING("Envoyer", fr.stgSend);
  TEST_ASSERT_EQUAL_STRING("The TEXT screen scrolls it, and it comes on stage now.", en.stgSayHint);
  TEST_ASSERT_EQUAL_STRING("L'\xC3\xA9" "cran TEXTE le fait d\xC3\xA9" "filer, et il entre en "
                           "sc\xC3\xA8ne tout de suite.", fr.stgSayHint);
  TEST_ASSERT_EQUAL_STRING("Saved. TEXT is off in Running order, so the panel does not show it.",
                           en.stgTextOff);
  TEST_ASSERT_EQUAL_STRING("Enregistr\xC3\xA9. TEXTE est \xC3\xA9teint dans l'ordre de passage, "
                           "donc le panneau ne le montre pas.", fr.stgTextOff);
  TEST_ASSERT_EQUAL_STRING("Saved. TEXT shows in landscape only: turn the panel to see it.",
                           en.stgTextPortrait);
  TEST_ASSERT_EQUAL_STRING("Enregistr\xC3\xA9. TEXTE ne s'affiche qu'en paysage\xE2\x80\xAF: tournez "
                           "le panneau pour le voir.", fr.stgTextPortrait);
  TEST_ASSERT_EQUAL_STRING("Not saved: the panel's memory refused the message. Try again; if it "
                           "fails twice, restart the panel.", en.stgTextFailed);
  TEST_ASSERT_EQUAL_STRING("Pas enregistr\xC3\xA9\xE2\x80\xAF: la m\xC3\xA9moire du panneau a refus"
                           "\xC3\xA9 le message. R\xC3\xA9" "essayez\xE2\x80\xAF; si \xC3\xA7" "a "
                           "\xC3\xA9" "choue deux fois, red\xC3\xA9marrez le panneau.",
                           fr.stgTextFailed);
  TEST_ASSERT_EQUAL_STRING("Not shown: %s is off in Running order. Turn it on there first.",
                           en.stgShowOff);
  TEST_ASSERT_EQUAL_STRING("Pas affich\xC3\xA9\xE2\x80\xAF: %s est \xC3\xA9teint dans l'ordre de "
                           "passage. Allumez-le d'abord.", fr.stgShowOff);
  TEST_ASSERT_EQUAL_STRING("Not shown: %s is drawn for the other orientation.", en.stgShowOther);
  TEST_ASSERT_EQUAL_STRING("Pas affich\xC3\xA9\xE2\x80\xAF: %s est dessin\xC3\xA9 pour l'autre "
                           "orientation.", fr.stgShowOther);
  TEST_ASSERT_EQUAL_STRING("Not shown: the panel has no screen by that name.", en.stgShowUnknown);
  TEST_ASSERT_EQUAL_STRING("Pas affich\xC3\xA9\xE2\x80\xAF: le panneau n'a pas d'\xC3\xA9" "cran de "
                           "ce nom.", fr.stgShowUnknown);
  TEST_ASSERT_EQUAL_STRING("Sent: the panel shows it now.", en.stgSent);
  TEST_ASSERT_EQUAL_STRING("Envoy\xC3\xA9\xE2\x80\xAF: le panneau l'affiche.", fr.stgSent);
  TEST_ASSERT_EQUAL_STRING("That did not go through: reload the page and try again.", en.stgFailed);
  TEST_ASSERT_EQUAL_STRING("\xC3\x87" "a n'est pas pass\xC3\xA9\xE2\x80\xAF: rechargez la page et "
                           "r\xC3\xA9" "essayez.", fr.stgFailed);
}

// The Images page: the same conversions in every language, the copy as
// written, and the IMAGE-off notice naming Running order
static void test_theatre_formats_images() {
  struct Fmt { const char *const Strings::*field; const char *want; const char *label; };
#define F(f, w) {&Strings::f, w, #f}
  static const Fmt FORMATS[] = {
    F(imgLead, ""), F(imgFileLabel, ""), F(imgShowsThis, ""), F(imgDeleteName, "%s"),
    F(imgSelectLabel, ""), F(imgEmptyHint, ""), F(webImgOff, ""),
  };
#undef F
  for (int l = 0; l < LANG_COUNT; l++)
    for (const Fmt &f : FORMATS) {
      char msg[64];
      snprintf(msg, sizeof(msg), "%s.%s", STRINGS[l].code, f.label);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(f.want, conversions(STRINGS[l].*f.field).c_str(), msg);
    }
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("Your own GIFs, shown in IMAGE's turn of the running order.", en.imgLead);
  TEST_ASSERT_EQUAL_STRING("Vos propres GIF, montr\xC3\xA9s au tour d'IMAGE dans l'ordre de passage.",
                           fr.imgLead);
  TEST_ASSERT_EQUAL_STRING("GIF file", en.imgFileLabel);
  TEST_ASSERT_EQUAL_STRING("Fichier GIF", fr.imgFileLabel);
  TEST_ASSERT_EQUAL_STRING("Panel shows this", en.imgShowsThis);
  TEST_ASSERT_EQUAL_STRING("Le panneau montre celle-ci", fr.imgShowsThis);
  TEST_ASSERT_EQUAL_STRING("Delete %s", en.imgDeleteName);
  TEST_ASSERT_EQUAL_STRING("Supprimer %s", fr.imgDeleteName);
  TEST_ASSERT_EQUAL_STRING("Image", en.imgSelectLabel);
  TEST_ASSERT_EQUAL_STRING("Image", fr.imgSelectLabel);
  TEST_ASSERT_EQUAL_STRING("Until you add one, the IMAGE screen shows the frame above. Add a GIF: "
                           "the panel shows it at once, then in IMAGE's turn.", en.imgEmptyHint);
  TEST_ASSERT_EQUAL_STRING("Tant que vous n'en ajoutez pas, l'\xC3\xA9" "cran IMAGE montre l'image "
                           "ci-dessus. Ajoutez un GIF\xE2\x80\xAF: le panneau le montre tout de "
                           "suite, puis au tour d'IMAGE.", fr.imgEmptyHint);
  TEST_ASSERT_EQUAL_STRING("IMAGE is off in Running order, so the panel does not show it.",
                           en.webImgOff);
  TEST_ASSERT_EQUAL_STRING("IMAGE est \xC3\xA9teint dans l'ordre de passage, donc le panneau ne "
                           "la montre pas.", fr.webImgOff);
}

// Running order: the same conversions in every language, the copy as
// written, and the list's hint without the lead it now leaves to the page
static void test_theatre_formats_order() {
  struct Fmt { const char *const Strings::*field; const char *want; const char *label; };
#define F(f, w) {&Strings::f, w, #f}
  static const Fmt FORMATS[] = {
    F(ordLead, ""), F(ordCount, "%u%u"), F(ordImageShows, "%s"), F(ordImageAll, ""),
    F(ordOpenImages, ""), F(webOrderHint, ""), F(ordSettingsOf, "%s"),
    F(ordHandle, "%s%s%s"), F(ordMoneySum, "%s%s%u%u"), F(ordImagesAll, "%d"),
  };
#undef F
  for (int l = 0; l < LANG_COUNT; l++)
    for (const Fmt &f : FORMATS) {
      char msg[64];
      snprintf(msg, sizeof(msg), "%s.%s", STRINGS[l].code, f.label);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(f.want, conversions(STRINGS[l].*f.field).c_str(), msg);
    }
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("What the panel plays, in this order. The knob and Home Assistant "
                           "follow this list.", en.ordLead);
  TEST_ASSERT_EQUAL_STRING("Ce que joue le panneau, dans cet ordre. Le bouton et Home Assistant "
                           "suivent cette liste.", fr.ordLead);
  TEST_ASSERT_EQUAL_STRING("Untick a screen to skip it. Drag it, or use its arrows, to move it; "
                           "Save keeps the order.",
                           en.webOrderHint);
  TEST_ASSERT_EQUAL_STRING("D\xC3\xA9" "cochez un \xC3\xA9" "cran pour le sauter. Faites-le glisser, ou utilisez "
                           "ses fl\xC3\xA8" "ches, pour le d\xC3\xA9placer\xE2\x80\xAF; Enregistrer garde l'ordre.",
                           fr.webOrderHint);
  TEST_ASSERT_EQUAL_STRING("%u of %u on", en.ordCount);
  TEST_ASSERT_EQUAL_STRING("%u sur %u allum\xC3\xA9s", fr.ordCount);
  TEST_ASSERT_EQUAL_STRING("Shows %s. Add, delete or choose the image on the Images page.",
                           en.ordImageShows);
  TEST_ASSERT_EQUAL_STRING("Montre %s. Ajoutez, supprimez ou choisissez l'image sur la page Images.",
                           fr.ordImageShows);
  TEST_ASSERT_EQUAL_STRING("Shows every image in turn. Add, delete or choose the image on the "
                           "Images page.", en.ordImageAll);
  TEST_ASSERT_EQUAL_STRING("Montre chaque image \xC3\xA0 tour de r\xC3\xB4le. Ajoutez, supprimez ou "
                           "choisissez l'image sur la page Images.", fr.ordImageAll);
  TEST_ASSERT_EQUAL_STRING("Open Images", en.ordOpenImages);
  TEST_ASSERT_EQUAL_STRING("Ouvrir Images", fr.ordOpenImages);
  // A row's link to its screen's page, and the summaries' words; a number
  // never parts from its unit
  TEST_ASSERT_EQUAL_STRING("%s settings", en.ordSettingsOf);
  TEST_ASSERT_EQUAL_STRING("R\xC3\xA9glages de %s", fr.ordSettingsOf);
  // A row's drag handle: the screen, its place and the count
  TEST_ASSERT_EQUAL_STRING("Move %s, position %s of %s", en.ordHandle);
  TEST_ASSERT_EQUAL_STRING("D\xC3\xA9placer %s, position %s sur %s", fr.ordHandle);
  TEST_ASSERT_EQUAL_STRING("%s\xC2\xA0%s \xC2\xB7 %u\xE2\x80\x93%u\xC2\xA0h", en.ordMoneySum);
  TEST_ASSERT_EQUAL_STRING("%s\xC2\xA0%s \xC2\xB7 %u\xC2\xA0h\xE2\x80\x93%u\xC2\xA0h", fr.ordMoneySum);
  // The salary's label names the chosen currency by its code
  TEST_ASSERT_EQUAL_STRING("Net monthly salary (%s)", en.webSalary);
  TEST_ASSERT_EQUAL_STRING("Salaire net mensuel (%s)", fr.webSalary);
  for (int l = 0; l < LANG_COUNT; l++)
    for (const char *w : STRINGS[l].clockShowsShort) TEST_ASSERT_TRUE(w && w[0]);
  TEST_ASSERT_EQUAL_STRING("seconds", en.clockShowsShort[1]);
  TEST_ASSERT_EQUAL_STRING("semaine", fr.clockShowsShort[2]);
}

// Panel's page: the same conversions in every language, and the changed
// hints word for word (the token's quotes the header as typed, its colon
// included)
static void test_theatre_formats_panel() {
  struct Fmt { const char *const Strings::*field; const char *want; const char *label; };
#define F(f, w) {&Strings::f, w, #f}
  static const Fmt FORMATS[] = {
    F(pnlLead, ""), F(pnlUpdates, ""), F(pnlWifi, ""), F(pnlAdvanced, ""),
    F(pnlLanguageLabel, ""), F(pnlSaveScope, ""), F(pnlRegion, ""),
    F(pnlTemperature, ""), F(pnlCurrency, ""), F(pnlClock, ""), F(pnlRegionHint, ""), F(webSalary, "%s"), F(pnlMqttTitle, "%s"), F(pnlHooksNone, ""),
    F(pnlHooksOne, ""), F(pnlHooksMany, "%s"), F(pnlHooksHint, ""), F(pnlApiTitle, ""),
    F(pnlHookInstall, ""), F(pnlHookInstallHint, ""), F(pnlHookUninstall, ""), F(pnlCopy, ""),
    F(pnlCopied, ""), F(pnlCopyByHand, ""), F(pnlHookRefused, "%s"), F(pnlHookOpenOff, ""),
    F(pnlHookOpenOn, ""), F(pnlHookAccept, ""), F(pnlHookRefuse, ""), F(pnlHookOpenWarn, ""),
    F(pnlHookRenew, ""), F(pnlHookRenewHint, ""), F(pnlHookLegacy, ""),
    F(pnlToken, ""), F(pnlMemoryTitle, ""), F(pnlRestartHint, ""), F(webNightHint, ""),
    F(webApiHint, ""), F(webUnsaved, ""), F(webDiscard, ""),
  };
#undef F
  for (int l = 0; l < LANG_COUNT; l++) {
    for (const Fmt &f : FORMATS) {
      char msg[64];
      snprintf(msg, sizeof(msg), "%s.%s", STRINGS[l].code, f.label);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(f.want, conversions(STRINGS[l].*f.field).c_str(), msg);
    }
    for (const char *w : STRINGS[l].pnlSignal)
      TEST_ASSERT_EQUAL_STRING_MESSAGE("", conversions(w).c_str(), w);
  }
  const Strings &en = STRINGS[LANG_EN], &fr = STRINGS[LANG_FR];
  TEST_ASSERT_EQUAL_STRING("good", en.pnlSignal[0]);
  TEST_ASSERT_EQUAL_STRING("fair", en.pnlSignal[1]);
  TEST_ASSERT_EQUAL_STRING("weak", en.pnlSignal[2]);
  TEST_ASSERT_EQUAL_STRING("bon", fr.pnlSignal[0]);
  TEST_ASSERT_EQUAL_STRING("moyen", fr.pnlSignal[1]);
  TEST_ASSERT_EQUAL_STRING("faible", fr.pnlSignal[2]);
  TEST_ASSERT_EQUAL_STRING("Home Assistant (MQTT): %s", en.pnlMqttTitle);
  TEST_ASSERT_EQUAL_STRING("Home Assistant (MQTT)\xE2\x80\xAF: %s", fr.pnlMqttTitle);
  TEST_ASSERT_EQUAL_STRING("Coding agent hooks: %s sessions", en.pnlHooksMany);
  TEST_ASSERT_EQUAL_STRING("Hooks des agents de code\xE2\x80\xAF: aucune session", fr.pnlHooksNone);
  TEST_ASSERT_EQUAL_STRING("%s hook calls refused since the panel started: no token, or an old "
                           "one. Run the command again on each computer.", en.pnlHookRefused);
  TEST_ASSERT_EQUAL_STRING("Hooks sans jeton\xE2\x80\xAF: refus\xC3\xA9s", fr.pnlHookOpenOff);
  TEST_ASSERT_EQUAL_STRING("Copied", en.pnlCopied);
  TEST_ASSERT_EQUAL_STRING("Copi\xC3\xA9", fr.pnlCopied);
  TEST_ASSERT_EQUAL_STRING("Hours from 0 to 23. Same hours: no night mode. At night, pressing the "
                           "knob brings back the day brightness for a minute.", en.webNightHint);
  TEST_ASSERT_EQUAL_STRING("Heures de 0 \xC3\xA0 23. M\xC3\xAAmes heures\xE2\x80\xAF: pas de mode "
                           "nuit. La nuit, appuyer sur le bouton rend la luminosit\xC3\xA9 de jour "
                           "pendant une minute.", fr.webNightHint);
  TEST_ASSERT_EQUAL_STRING("For scripts and home automation: send it as \"Authorization: Bearer "
                           "<token>\" to /api/... (see docs/api.md). Signing out every other "
                           "browser also replaces it.", en.webApiHint);
  TEST_ASSERT_EQUAL_STRING("Pour les scripts et la domotique\xE2\x80\xAF: envoyez-le en \xC2\xAB"
                           "\xE2\x80\xAF" "Authorization: Bearer <token>\xE2\x80\xAF\xC2\xBB \xC3\xA0 "
                           "/api/... (voir docs/api.md). D\xC3\xA9" "connecter tous les autres "
                           "navigateurs le remplace aussi.", fr.webApiHint);
}

// French typography on every new or changed web field of the theatre pages:
// U+202F before : ; ? ! and inside « », U+00A0 between a digit and its unit
static void test_french_spacing_theatre() {
  static const char *const Strings::*THEATRE_WEB[] = {
    &Strings::webSkip, &Strings::webNavName, &Strings::webSetupBrand, &Strings::webTitle,
    &Strings::wifiAnother, &Strings::wifiName, &Strings::wifiMsgSaved, &Strings::wifiNoneYet,
    &Strings::wifiPasswordHint, &Strings::wifiBand,   // on the Wi-Fi page, fixed with it
    &Strings::wifiKnown, &Strings::wifiOthers, &Strings::wifiNetworks, &Strings::wifiAway,
    &Strings::wifiSignalLocked, &Strings::wifiSignalOpen, &Strings::wifiContinue,
    &Strings::signInLive, &Strings::signInCaption, &Strings::signInFrame, &Strings::signInReload,
    &Strings::signInTitle, &Strings::signInHint, &Strings::signInCode, &Strings::signInButton,
    &Strings::signInWrong, &Strings::signInDigit,   // on the sign-in page, fixed with it
    &Strings::stgLive, &Strings::stgPlace, &Strings::stgNowSr, &Strings::stgNext,
    &Strings::stgKnobOnly, &Strings::stgNoOther, &Strings::stgPreviewAlt, &Strings::stgStill,
    &Strings::stgAllScreens, &Strings::stgSessions, &Strings::stgSessionsHint,
    &Strings::stgAgoSec, &Strings::stgAgoMin, &Strings::stgAgoHour, &Strings::stgSetUpHooks,
    &Strings::stgBring, &Strings::stgScreen, &Strings::stgLights, &Strings::stgDimmer,
    &Strings::stgBrighter, &Strings::stgSteps, &Strings::stgNightDims, &Strings::stgNightDark,
    &Strings::stgNightNone, &Strings::webNightMode, &Strings::stgSay, &Strings::stgSend,
    &Strings::stgSayHint, &Strings::stgTextOff, &Strings::stgTextPortrait, &Strings::stgTextFailed,
    &Strings::stgShowOff, &Strings::stgShowOther, &Strings::stgShowUnknown, &Strings::stgSent,
    &Strings::stgFailed,
    &Strings::imgLead, &Strings::imgFileLabel, &Strings::imgShowsThis, &Strings::imgDeleteName,
    &Strings::imgSelectLabel, &Strings::imgEmptyHint,
    &Strings::ordLead, &Strings::ordCount, &Strings::ordImageShows, &Strings::ordImageAll,
    &Strings::ordOpenImages, &Strings::webOrderHint, &Strings::ordSettingsOf,
    &Strings::ordHandle, &Strings::ordMoneySum, &Strings::ordImagesAll,
    &Strings::pnlLead, &Strings::pnlUpdates, &Strings::pnlWifi, &Strings::pnlAdvanced,
    &Strings::pnlLanguageLabel, &Strings::pnlSaveScope, &Strings::pnlRegion,
    &Strings::pnlTemperature, &Strings::pnlCurrency, &Strings::pnlClock, &Strings::pnlRegionHint,
    &Strings::webSalary, &Strings::pnlMqttTitle,
    &Strings::pnlHooksNone, &Strings::pnlHooksOne, &Strings::pnlHooksMany, &Strings::pnlHooksHint,
    &Strings::pnlHookInstall, &Strings::pnlHookInstallHint, &Strings::pnlHookUninstall,
    &Strings::pnlCopy, &Strings::pnlCopied, &Strings::pnlCopyByHand, &Strings::pnlHookRefused,
    &Strings::pnlHookOpenOff, &Strings::pnlHookOpenOn, &Strings::pnlHookAccept,
    &Strings::pnlHookRefuse, &Strings::pnlHookOpenWarn, &Strings::pnlHookRenew,
    &Strings::pnlHookRenewHint, &Strings::pnlHookLegacy,
    &Strings::pnlApiTitle, &Strings::pnlToken, &Strings::pnlMemoryTitle, &Strings::pnlRestartHint,
    &Strings::webNightHint, &Strings::errBack, &Strings::webUnsaved, &Strings::webDiscard,
  };
  std::vector<const char *> texts;
  for (const auto field : THEATRE_WEB) texts.push_back(STRINGS[LANG_FR].*field);
  for (const char *tab : STRINGS[LANG_FR].webTabs) texts.push_back(tab);
  for (const char *e : STRINGS[LANG_FR].stgEvery) texts.push_back(e);
  for (const char *e : STRINGS[LANG_FR].stgStates) texts.push_back(e);
  for (const char *w : STRINGS[LANG_FR].pnlSignal) texts.push_back(w);
  for (const char *e : STRINGS[LANG_FR].errTitle) texts.push_back(e);
  for (const char *e : STRINGS[LANG_FR].errText) texts.push_back(e);
  static const char OPEN[] = "\xC2\xAB", CLOSE[] = "\xC2\xBB";
  static const char *const UNITS[] = {"GHz", "MHz", "dBm", "Ko", "s", "min", "h", "%"};
  for (const char *s : texts) {
    for (const char *p = s; *p; p++) {
      // An hour's colon (%u:00) is not punctuation
      if (strchr(":;?!", *p) && !(*p == ':' && p[1] >= '0' && p[1] <= '9'))
        TEST_ASSERT_TRUE_MESSAGE(p - s >= 3 && memcmp(p - 3, NNBSP, 3) == 0, s);
      if (strncmp(p, OPEN, 2) == 0)
        TEST_ASSERT_TRUE_MESSAGE(strncmp(p + 2, NNBSP, 3) == 0, s);
      if (strncmp(p, CLOSE, 2) == 0)
        TEST_ASSERT_TRUE_MESSAGE(p - s >= 3 && memcmp(p - 3, NNBSP, 3) == 0, s);
      // A digit, a space of any kind, then a unit
      if (*p >= '0' && *p <= '9' && p[1] == ' ')
        for (const char *u : UNITS) {
          const size_t n = strlen(u);
          const char after = p[2 + n];
          TEST_ASSERT_FALSE_MESSAGE(strncmp(p + 2, u, n) == 0 && !isalpha((unsigned char)after), s);
        }
    }
  }
  TEST_ASSERT_NOT_NULL(strstr(STRINGS[LANG_FR].wifiBand, "2,4\xC2\xA0GHz"));
  TEST_ASSERT_NOT_NULL(strstr(STRINGS[LANG_FR].wifiBand, "5\xC2\xA0GHz"));
  TEST_ASSERT_NOT_NULL(strstr(STRINGS[LANG_EN].wifiBand, "2.4\xC2\xA0GHz"));
  TEST_ASSERT_NOT_NULL(strstr(STRINGS[LANG_FR].signInWrong, "30\xC2\xA0s"));
}

// A refusal names the mark the owner sees on the list, word for word
static void test_refusals_name_the_marks() {
  for (int l = 0; l < LANG_COUNT; l++) {
    const Strings &t = STRINGS[l];
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(t.webRefusedPortrait, t.webLandscapeOnly), t.code);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(t.webRefusedLandscape, t.webPortraitOnly), t.code);
    TEST_ASSERT_EQUAL_MESSAGE(0, strncmp(t.webRefusedEmpty, "<b>", 3), t.code);
    TEST_ASSERT_EQUAL_MESSAGE(0, strncmp(t.webRefusedPortrait, "<b>", 3), t.code);
    TEST_ASSERT_EQUAL_MESSAGE(0, strncmp(t.webRefusedLandscape, "<b>", 3), t.code);
  }
}

static int count(const char *s, const char *word) {
  int n = 0;
  for (const char *p = s; (p = strstr(p, word)); p += strlen(word)) n++;
  return n;
}

// Copy reviewed on the page: errors open with a bold lead word, like the
// Images notices; no word said twice; the French ordinal as typeset
static void test_message_copy() {
  for (int l = 0; l < LANG_COUNT; l++)
    TEST_ASSERT_EQUAL_MESSAGE(0, strncmp(STRINGS[l].webNotSaved, "<b>", 3), STRINGS[l].code);
  // Stored, but the gallery did not take the list: it opens as "saved" does
  for (int l = 0; l < LANG_COUNT; l++)
    TEST_ASSERT_EQUAL_MESSAGE(0, strncmp(STRINGS[l].webSavedLater, STRINGS[l].webSaved,
                                         strlen(STRINGS[l].webSaved)), STRINGS[l].code);
  TEST_ASSERT_EQUAL(1, count(STRINGS[LANG_EN].webImgFailed, "again"));
  TEST_ASSERT_NOT_NULL(strstr(STRINGS[LANG_FR].webImgTooMany, "25\xE1\xB5\x89 image"));   // 25ᵉ
}

// Strings used as printf formats carry exactly one %lu, in every language
static void test_formats_take_one_number() {
  for (int l = 0; l < LANG_COUNT; l++) {
    const char *f = STRINGS[l].webMinutesAgo;
    const char *p = strstr(f, "%lu");
    TEST_ASSERT_NOT_NULL_MESSAGE(p, STRINGS[l].code);
    TEST_ASSERT_NULL_MESSAGE(strchr(p + 3, '%'), STRINGS[l].code);
    TEST_ASSERT_TRUE_MESSAGE(p == f || !strchr(f, '%') || strchr(f, '%') == p, STRINGS[l].code);
  }
}

static void test_language_selection() {
  setLanguage(LANG_FR);
  TEST_ASSERT_EQUAL_STRING("fr", tr().code);
  setLanguage(LANG_COUNT + 3);                      // corrupted NVS value
  TEST_ASSERT_EQUAL_STRING("en", tr().code);
}

static void test_panel_text_folds_utf8() {
  char out[32];
  toPanelText("Caf\xC3\xA9 \xC3\xA9t\xC3\xA9 \xC3\x80 bient\xC3\xB4t", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Cafe ete A bientot", out);
  toPanelText("ok \xF0\x9F\x98\x80!", out, sizeof(out));       // emoji
  TEST_ASSERT_EQUAL_STRING("ok ?!", out);
  toPanelText("l\xE2\x80\x99" "API \xE2\x80\x94 \xC2\xAB ok \xC2\xBB\xE2\x80\xA6", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("l'API - \" ok \"...", out);               // phone punctuation
  toPanelText("abc\xC3", out, sizeof(out));                       // cut mid-character
  TEST_ASSERT_TRUE(panelSafe(out));
  toPanelText("abcdef", out, 4);                                   // truncated
  TEST_ASSERT_EQUAL_STRING("abc", out);
}

// A lead byte just before the NUL must not make the decoder read past the
// string: exact-size heap copies, so AddressSanitizer sees a single byte over.
static void test_panel_text_cut_character_stays_inside() {
  const char *cuts[] = {"a\xE2", "a\xE2\x80", "a\xC3", "a\xF0\x9F\x98"};
  for (const char *cut : cuts) {
    const std::vector<char> in(cut, cut + strlen(cut) + 1);
    char out[8];
    toPanelText(in.data(), out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("a?", out);             // a cut character is unknown
  }
}

// The TEXT screen's message keeps the French letters the body face draws and
// folds the rest as toPanelText does
static void test_message_text_keeps_french_letters() {
  char out[48];
  toMessageText("Caf\xC3\xA9 \xC3\xA0 14h", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Caf\xC3\xA9 \xC3\xA0 14h", out);
  toMessageText("\xC3\x87" "a \xC5\x93uvre \xC5\x92 \xC3\xBF", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("\xC3\x87" "a \xC5\x93uvre \xC5\x92 \xC3\xBF", out);
  toMessageText("\xC3\xA1\xC3\xB1\xC3\x9F \xF0\x9F\x98\x80!", out, sizeof(out));   // letters the face lacks
  TEST_ASSERT_EQUAL_STRING("ans ?!", out);
  toMessageText("l\xE2\x80\x99" "API \xE2\x80\x94 \xC2\xAB ok \xC2\xBB\xE2\x80\xA6\xC2\xA0!", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("l'API - \" ok \"... !", out);
  toMessageText("HELLO * CAROUSEL * ", out, sizeof(out));         // the stored ASCII one
  TEST_ASSERT_EQUAL_STRING("HELLO * CAROUSEL * ", out);
}

static void test_message_text_never_cuts_a_character() {
  char out[8];
  toMessageText("a\xC3\xA9\xC3\xA9", out, 5);   // the last letter does not fit: folded
  TEST_ASSERT_EQUAL_STRING("a\xC3\xA9" "e", out);
  toMessageText("a\xC3\xA9\xC3\xA9", out, 6);
  TEST_ASSERT_EQUAL_STRING("a\xC3\xA9\xC3\xA9", out);
  toMessageText("abcdef", out, 4);
  TEST_ASSERT_EQUAL_STRING("abc", out);
  toMessageText("abc\xC3", out, sizeof(out));           // cut mid-character
  TEST_ASSERT_EQUAL_STRING("abc?", out);
  toMessageText("\xC3\xA9", out, 2);                   // no room for two bytes
  TEST_ASSERT_EQUAL_STRING("e", out);
  toMessageText("\xC5\x92\xC5\x93\xC5\xB8", out, 4);       // the letters past Latin-1 too
  TEST_ASSERT_EQUAL_STRING("OoY", out);
}

// A whole UTF-8 sequence is checked before it is stored: a lead byte without its
// continuation bytes is one unknown character and the byte after it is read on
// its own. MQTT and the API are untrusted, and Home Assistant drops a state
// document it cannot decode.
static bool validUtf8(const char *s) {
  for (const uint8_t *p = (const uint8_t *)s; *p; ) {
    const int want = *p < 0x80 ? 1 : *p >= 0xC2 && *p < 0xE0 ? 2 : *p >= 0xE0 && *p < 0xF0 ? 3 : 0;
    if (!want) return false;
    for (int k = 1; k < want; k++) if ((p[k] & 0xC0) != 0x80) return false;
    if (want == 3 && (((p[0] & 0x0F) << 6 | (p[1] & 0x3F)) < 0x20)) return false;   // overlong
    p += want;
  }
  return true;
}

static void test_message_text_stores_only_valid_utf8() {
  char out[16];
  toMessageText("\xC3 a", out, sizeof(out));             // 0xC3 and a space read as \u00E0
  TEST_ASSERT_EQUAL_STRING("? a", out);
  toMessageText("\xC5R", out, sizeof(out));              // 0xC5 and R read as \u0152
  TEST_ASSERT_EQUAL_STRING("?R", out);
  toMessageText("\xC3\xE9\xA9", out, sizeof(out));       // a lead, then a sequence cut short
  TEST_ASSERT_EQUAL_STRING("??", out);
  toMessageText("\xA9x", out, sizeof(out));              // a continuation byte alone
  TEST_ASSERT_EQUAL_STRING("?x", out);
  toMessageText("\xC0\xA0!", out, sizeof(out));          // an overlong space
  TEST_ASSERT_EQUAL_STRING("?!", out);
  toMessageText("ab\xE2\x80", out, sizeof(out));         // truncated at the end
  TEST_ASSERT_EQUAL_STRING("ab?", out);
  toMessageText("ab\xC3", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("ab?", out);
  toPanelText("\xC5R", out, sizeof(out));                // the panel's fold reads it the same
  TEST_ASSERT_EQUAL_STRING("?R", out);

  // Random bytes, weighted to leads and continuations: always valid, always inside
  static const uint8_t BYTES[] = {'a', ' ', 0x80, 0x93, 0xA9, 0xBF, 0xC0, 0xC3, 0xC5, 0xE0, 0xE2, 0xF0};
  uint32_t seed = 12345;
  for (int round = 0; round < 20000; round++) {
    char in[13], msg[17];
    const int len = 1 + round % 12;
    for (int i = 0; i < len; i++) {
      seed = seed * 1103515245u + 12345u;
      in[i] = (char)BYTES[(seed >> 16) % sizeof(BYTES)];
    }
    in[len] = '\0';
    const size_t size = 1 + (seed >> 8) % 16;
    toMessageText(in, msg, size);
    TEST_ASSERT_TRUE(strlen(msg) < size);
    TEST_ASSERT_TRUE_MESSAGE(validUtf8(msg), in);
  }
}

static size_t characters(const char *s) {
  size_t n = 0;
  for (; *s; s++) if (((uint8_t)*s & 0xC0) != 0x80) n++;
  return n;
}

// The fields take 95 characters and the message holds 95 bytes: every typed
// character is kept, and the accents that no longer fit lose their mark, the
// last ones first (message[96] stays, so an older build still reads the key)
static void test_a_long_accented_message_keeps_every_character() {
  const char *typed = "Un caf\xC3\xA9 d'\xC3\xA9t\xC3\xA9 \xC3\xA0 c\xC3\xB4t\xC3\xA9 du lac\xE2\x80\xAF: "
                      "cr\xC3\xA8me br\xC3\xBBl\xC3\xA9" "e, g\xC3\xA2teau, th\xC3\xA9 et f\xC3\xAAte "
                      "pr\xC3\xA8s du vieux ch\xC3\xA2teau \xC3\xA9l\xC3\xA9gant\xE2\x80\xAF!";
  TEST_ASSERT_EQUAL(95, characters(typed));
  char out[96];
  toMessageText(typed, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Un cafe d'ete a cote du lac : creme brulee, gateau, the et fete "
                           "pres du vieux chateau elegant !", out);
  TEST_ASSERT_EQUAL(95, characters(out));

  char some[14];
  toMessageText("\xC3\xA9t\xC3\xA9 \xC3\xA0 c\xC3\xB4t\xC3\xA9", some, sizeof(some));
  TEST_ASSERT_EQUAL_STRING("\xC3\xA9t\xC3\xA9 \xC3\xA0 cote", some);   // room for three accents
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_string_is_present);
  RUN_TEST(test_panel_strings_fit_and_are_drawable);
  RUN_TEST(test_formats_take_one_number);
  RUN_TEST(test_image_screen_name);
  RUN_TEST(test_image_panel_texts);
  RUN_TEST(test_reset_failed_texts);
  RUN_TEST(test_reset_failed_knob_text);
  RUN_TEST(test_image_formats);
  RUN_TEST(test_french_spacing);
  RUN_TEST(test_screens_formats);
  RUN_TEST(test_rotate_choices);
  RUN_TEST(test_french_spacing_screens);
  RUN_TEST(test_refusals_name_the_marks);
  RUN_TEST(test_theatre_formats_shell);
  RUN_TEST(test_theatre_formats_access);
  RUN_TEST(test_theatre_formats_errors);
  RUN_TEST(test_theatre_formats_stage);
  RUN_TEST(test_theatre_formats_stage_actions);
  RUN_TEST(test_theatre_formats_images);
  RUN_TEST(test_theatre_formats_order);
  RUN_TEST(test_theatre_formats_panel);
  RUN_TEST(test_french_spacing_theatre);
  RUN_TEST(test_message_copy);
  RUN_TEST(test_language_selection);
  RUN_TEST(test_panel_text_folds_utf8);
  RUN_TEST(test_panel_text_cut_character_stays_inside);
  RUN_TEST(test_message_text_keeps_french_letters);
  RUN_TEST(test_message_text_never_cuts_a_character);
  RUN_TEST(test_message_text_stores_only_valid_utf8);
  RUN_TEST(test_a_long_accented_message_keeps_every_character);
  return UNITY_END();
}
