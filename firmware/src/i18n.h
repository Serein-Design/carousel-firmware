// SPDX-License-Identifier: AGPL-3.0-only
// User-facing text, one table per language. Serial logs stay in English.
//
// Panel text is upper-case ASCII: the built-in 5x7 font has no upper-case
// accented letters. Every panel string has a width budget (6 px per char:
// 10 chars across 64 px, 5 across 32 px); test/test_i18n checks them all.
// Web text is UTF-8 and has no budget.
//
// Adding a language: add an entry to Lang and a table in i18n.cpp. The tests
// fail on any missing string or any panel string over its budget.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "screen_style.h"

enum Lang : uint8_t { LANG_EN = 0, LANG_FR, LANG_COUNT };

struct Strings {
  const char *code;            // HTML lang attribute
  const char *name;            // shown in the language picker, in its own language

  // --- Panel: screen names, banner (10) -------------------------------------
  const char *scrMascot, *scrCode, *scrDance, *scrSleep, *scrRun, *scrLife, *scrBall,
             *scrMatrix, *scrPlasma, *scrFire, *scrStars, *scrText, *scrRays, *scrBars,
             *scrBlackHole, *scrCat, *scrIss, *scrClock, *scrWeather, *scrDate,
             *scrSystem, *scrMoney, *scrSocial;
  const char *scrImage;        // the NVS, MQTT and API name too: "IMAGE" in every language (5)

  // The mascot's screens (body and label faces, test_mascot_logic): NIMBUS's
  // greeting in a bubble (28 px, it may stand in portrait), and RUN's crash
  // line above the best score (label face, shares 40 px)
  const char *mascotHi, *runBug, *runBest;

  // --- Panel: status ----------------------------------------------------------
  // CODE: a session's state in a word, in the body face (60 px, test_code_view);
  // codeFailed: the turn ended on an error, codeToolError: one tool failed
  const char *codeThinking, *codeWorking, *codeNeedsYou, *codeDone, *codeIdle, *codeFailed,
             *codeToolError;
  const char *codeWaiting, *codeNoWifi;   // no session yet; no Wi-Fi either (body face)
  // printf formats, one %u (elapsed minutes, at most 99): the cloud's label
  // line, and the list's narrower column (label face)
  const char *codeMin, *codeMinShort;
  // CLOCK in the Atelier faces, UTF-8, measured by test_clock_logic: the
  // date ("MON 19 OCT" / "LUN. 19 OCT.", Sunday first), the readouts'
  // words ("WEEK 43", "OUTSIDE 14°"), and the 12 h clock's AM and PM beside
  // the hours (9 px at most: 28 px portrait holds them beside two figures)
  const char *clkDays[7], *clkMonths[12];
  const char *clkWeek, *clkOutside;
  const char *clkAm, *clkPm;
  const char *weatherLoading;  // weather, landscape (10)
  // A service with no fresh data keeps its last reading, dimmed, and says
  // why in the foot band, in the label face (test_states): no Wi-Fi, a key
  // the owner has not given, the server not answering. A reason per word, in
  // that order (Reason, states_logic.h).
  const char *staleWords[3];
  // WEATHER: a word per condition, in WxCond order (weather_logic.h), sentence
  // case, measured in its faces by test_weather (60 px; two label lines of
  // 28 px in portrait), and W3's hour, printf with one %u (14 px); in 12 h
  // the hour and a letter for AM or PM ("12PM" is 17 px; test_clock_logic)
  const char *wxCond[11];
  const char *wxHour, *wxHourAm, *wxHourPm;
  // MONEY, UTF-8 in the label face, measured by test_info_logic: the label
  // line in working hours or not (60 px; Short: portrait, 28 px), the foot's
  // word before the month's total; the decimal mark; the space before a
  // currency written after the amount ("45 €", "45 CHF"; info_logic.h)
  const char *working, *offHours, *workingShort, *pausedShort, *month;
  const char *decimalMark, *moneySpace;
  // SOCIAL, label face (test_info_logic): each network's name, in SocialNet
  // order (info_logic.h), on the label line (Tall: 28 px), what its count
  // counts, and a count in millions, printf with one %s ("12.3", "123")
  const char *socialNames[4], *socialNamesTall[4];
  const char *socialSubscribers, *socialFollowers, *socialMillions;
  const char *weekPrefix;      // DATE and CLOCK in portrait: "W12" (1)
  const char *dayPrefix;       // DATE in portrait, the day of the year: "D85" (1)
  // DATE's foot: the week and the day of the year, printf with one %u each
  const char *dateWeek, *dateDay;
  // SYSTEM, label face (test_info_logic): the label line, its portrait word,
  // the unit of the free RAM; printf: the frame rate (one %u), the uptime
  // (one %s), the uptime under a minute's hour (one %u); the day's letter
  const char *sysRam, *sysRamTall, *sysKb, *sysFps, *sysUp, *sysUpTall, *sysUpMin, *sysUpDay;
  const char *near;            // ISS, before the distance (5)
  const char *land, *ocean;    // ISS, portrait (5)
  const char *setup;           // button held at boot, before the countdown (10)
  const char *factoryReset;    // button held at boot, counting down (5)
  // Wi-Fi setup screen and reach card, UTF-8, measured in their faces by
  // test_reach: saved, setupJoin and setupOpen in the body face (60 px);
  // phoneConnected and wrongPassword in place of setupLabel, and reachLabel,
  // on the label line (60 px); setupScan and reachScan beside a QR code
  // (label, 30 px); setupTall heads the portrait column (label, 28 px)
  const char *saved, *phoneConnected, *wrongPassword;
  const char *setupLabel, *setupJoin, *setupOpen, *setupScan[2], *setupTall;
  const char *reachLabel, *reachScan[2];
  // The sign-in code's label line, UTF-8, in the label face (test_states):
  // 60 px, and 28 px in portrait
  const char *signInPanel, *signInPanelTall;
  // The boot's word, body face: one line (60 px), or two in portrait (28 px)
  const char *bootWord, *bootWordTall[2];
  // DATE's weekday and month in full, label face, Sunday first (test_info_logic)
  const char *days[7];
  const char *months[12];
  // IMAGE screen with nothing to play for this orientation, two lines across
  // (10) or four down (5), and with no storage area
  const char *imgOther[2], *imgNoStore[2];
  const char *imgOtherTall[4], *imgNoStoreTall[4];
  // IMAGE with no image at all (I1), measured by test_states: noImage in the
  // body face, 54 px at most; noImageAsk before the panel's address, in the
  // label face; across 28 px, noImageTall: two body lines, two label lines
  // that ask, one that says where
  const char *noImage, *noImageAsk, *noImageTall[5];
  // A factory reset that could not finish, in every language at once (the
  // setting is unreadable): what happened and what to do, label face; across
  // 28 px, the word and the step in three lines (test_states)
  const char *locked[3], *lockedTall[3];

  // --- Web: shared by the pages ------------------------------------------------
  const char *webSaved, *webOutOfMemory;
  const char *webNotSaved;     // a save: the panel refused to store the form
  const char *webSavedLater;   // stored, but the gallery did not take the screen list
  const char *webWaitingHint, *webClearSessions, *webHookAddress,
             *webStates;
  const char *webSignal, *webSignalHint, *webChangeWifi, *webFreeMemory,
             *webMemoryMin, *webLastUpdate,
             *webMinutesAgo;          // printf format, one %lu
  const char *webNotYet;       // a count or an update the panel has not fetched yet
  const char *webThousands;    // between groups of three digits: "," / U+202F
  const char *webLanguage;
  const char *webSalary,   // %s: the currency's code (region_logic.h)
             *webDayStart, *webDayEnd, *webDaysPerMonth, *webIncomeHint;
  const char *webYtKey, *webYtChannel, *webYtChannelHint, *webGhUser, *webGhHint;
  const char *webManual, *webInstagram, *webX, *webManualHint;
  // webUnsaved, webDiscard: the save bar once a field differs from the page as loaded
  const char *webSave, *webUnsaved, *webDiscard;
  const char *webCity, *webCityFound, *webCityNotFound, *webCitySearching, *webCityHint;
  // The city field's suggestions, said aloud as they come: the count (one %s)
  // and none
  const char *webCityMatches, *webCityNone;
  const char *webLastRestart;
  const char *webFirmware, *updChecking, *updUpToDate, *updAvailable, *updInstalling, *updCheck,
             *updInstall, *updAuto;
  const char *webMessage;
  const char *webNightFrom, *webNightTo, *webNightLevel, *webNightOff,
             *webNightHint;
  const char *webCredits;      // footer of Stage, Running order: the weather's data, %s MET, %s GeoNames
  const char *webDisclaimer;   // footer: not affiliated with Anthropic
  const char *webSource;       // footer: link to the source code (AGPL)
  const char *webMqttOff, *webMqttOn, *webMqttDown, *webMqttHost,
             *webMqttPort, *webMqttUser, *webMqttPass, *webMqttHint;
  const char *webApiHint;
  const char *webResetFailed;  // setup page while a factory reset is unfinished
  const char *webResetFailedKnob;  // same, after a knob reset that wrote no marker
  const char *resetFailedTitle;    // that page's title (not the Wi-Fi page's)

  // --- Web: Images section -----------------------------------------------------
  // printf formats; every language takes the same conversions (test_i18n):
  // webImgUsedOf %lu %lu, webImgOnPanel %d, webImgAdded / webImgReplaced / webImgNotGifOther /
  // webImgTooMany %s, webImgNotGif %s (name) %s (type), webImgWrongSize %s %u %u,
  // webImgNoRoom %s %lu %lu.
  const char *webImages, *webImgSpace, *webImgUsedOf, *webKb, *webImgNone, *webImgAdd,
             *webImgAddHint, *webImgUpload, *webImgOnPanel, *webImgLandscape, *webImgPortrait,
             *webImgDelete, *webImgShows, *webImgAll, *webImgShowHint, *webImgShow;
  const char *webImgAdded, *webImgReplaced, *webImgNotAdded, *webImgNotGif, *webImgNotGifOther,
             *webImgWrongSize, *webImgNoRoom, *webImgTooMany, *webImgTooBig, *webImgFailed,
             *webImgNoFile;
  const char *webImgNa, *webImgNaText, *webImgNaLink;   // no storage area on this panel

  // --- Web: Screens section ----------------------------------------------------
  // printf formats, the same conversions in every language (test_i18n):
  // webInOrder %u %u, webMoveUp / webMoveDown %s, webTitleValue %s %s.
  const char *webChangeScreen;
  const char *webRotate[7];    // the offered intervals, knob only first
  const char *webRotateHint, *webInOrder, *webOrderHint, *webLandscapeOnly, *webPortraitOnly,
             *webMoveUp, *webMoveDown;
  // A refused list; the last two quote webLandscapeOnly and webPortraitOnly
  const char *webRefusedEmpty, *webRefusedPortrait, *webRefusedLandscape;
  const char *webTitleValue, *webImgOff;
  // How CODE, CLOCK and WEATHER draw, under each: a name and a one-line
  // description per style, in the enums' order (screen_style.h), the default
  // first; webClockShows heads what CLOCK shows besides the time, one item
  // per ClockShow bit, lowest first; clockShowsShort the same items in a
  // word, for Running order's summary of CLOCK ("Stone · date, seconds").
  const char *webStyle;
  const char *codeStyles[CODE_STYLE_COUNT], *codeStyleHints[CODE_STYLE_COUNT];
  const char *clockStyles[CLOCK_STYLE_COUNT], *clockStyleHints[CLOCK_STYLE_COUNT];
  const char *weatherStyles[WEATHER_STYLE_COUNT], *weatherStyleHints[WEATHER_STYLE_COUNT];
  const char *webClockShows, *clockShows[4], *clockShowsShort[4];

  // --- Web: sign-in ------------------------------------------------------------
  const char *signInTitle, *signInHint, *signInCode, *signInButton, *signInWrong, *signOutAll;

  // --- Web: Wi-Fi page ----------------------------------------------------------
  const char *wifiTitle, *wifiSubtitle, *wifiSaved, *wifiState,
             *wifiStateSetup, *wifiStateConnected, *wifiStateOffline;
  const char *wifiRefresh, *wifiPassword, *wifiPasswordHint, *wifiFooter, *wifiBand;
  const char *wifiLastTry, *wifiRefused, *wifiNotFound;
  const char *wifiMsgRetry, *wifiMsgBadSsid, *wifiMsgBadPassword;

  // --- Web: Shell (theatre pages) ------------------------------------------------
  // What every page shares. webTitle %s: the page's name, for <title>.
  const char *webTabs[4];      // Stage, Running order, Images, Panel, in the tabs' order
  const char *webSkip, *webNavName, *webSetupBrand, *webTitle;

  // --- Web: Wi-Fi (theatre pages) ------------------------------------------------
  // wifiMsgSaved %s: the network saved, escaped.
  const char *wifiAnother, *wifiName, *wifiMsgSaved, *wifiNoneYet;
  // wifiKnown heads the saved network's section, wifiOthers the folded list
  // under it, wifiNetworks the list when nothing is saved. wifiAway %s: the
  // saved network, out of range. wifiSignal: the bars' word, 1 to 4 bars, in
  // a sentence's lower case; wifiSignalLocked / wifiSignalOpen %s: that word,
  // for a screen reader after the name ("Maison, excellent signal, secured").
  const char *wifiKnown, *wifiOthers, *wifiNetworks, *wifiAway;
  const char *wifiSignal[4];
  const char *wifiSignalLocked, *wifiSignalOpen, *wifiContinue;

  // --- Web: Sign-in and locked (theatre pages) ------------------------------------
  // No conversion: the code itself only ever shows on the panel. The locked
  // page shows lockedLive of both tables (the setting cannot be read).
  const char *signInLive, *signInCaption, *signInFrame, *signInReload, *lockedLive;
  // signInDigit %d: a code box's place, 1 to 6, for a screen reader (the
  // page's script fills it in)
  const char *signInDigit;

  // --- Web: Error pages (theatre pages) ------------------------------------------
  // In ErrorKind order (web_access.h): not found, refused, too large, bad
  // form, busy. errBack %s: the tab's name of the page the link goes back to.
  const char *errTitle[5], *errText[5], *errBack;

  // --- Web: Stage (theatre pages) ------------------------------------------------
  // stgPlace %u %u: the place and the count; stgNext %s %lu: the next screen's
  // name, then the seconds (the page splits the line after %s);
  // stgPreviewAlt %s: the screen's name; stgAllScreens %u: every screen;
  // stgAgoSec / stgAgoMin / stgAgoHour %lu.
  const char *stgLive, *stgPlace, *stgNowSr, *stgNext;
  const char *stgEvery[6];     // webRotate[1..6], as part of a sentence
  const char *stgKnobOnly, *stgNoOther, *stgPreviewAlt, *stgStill, *stgAllScreens;
  const char *stgSessions, *stgSessionsHint;
  const char *stgStates[7];    // AgentState order: idle, thinking, tool, attention, error, done, failed
  const char *stgAgoSec, *stgAgoMin, *stgAgoHour, *stgSetUpHooks;
  // The actions. stgSteps %d %d: the step and the count; stgNightDims %d %d
  // %u %u %s: the step, the count, the night's hours and its level ("10 %");
  // stgNightDark %d %d %u %u; stgNightNone %d %d; stgShowOff / stgShowOther
  // %s: the screen's name, escaped. webNightMode also heads Panel's section.
  const char *stgBring, *stgScreen, *stgLights, *stgDimmer, *stgBrighter, *stgSteps;
  const char *stgNightDims, *stgNightDark, *stgNightNone, *webNightMode;
  const char *stgSay, *stgSend, *stgSayHint, *stgTextOff, *stgTextPortrait, *stgTextFailed;
  const char *stgShowOff, *stgShowOther, *stgShowUnknown;
  const char *stgSent, *stgFailed;   // a press answered in place, with scripts

  // --- Web: Images (theatre pages) ------------------------------------------------
  // imgDeleteName %s: the image's name, escaped, for its Delete button.
  const char *imgLead, *imgFileLabel, *imgShowsThis, *imgDeleteName, *imgSelectLabel,
             *imgEmptyHint;

  // --- Web: Running order (theatre pages) ------------------------------------------
  // ordCount %u %u: the screens on and every screen; ordImageShows %s: the
  // saved image's name, escaped.
  const char *ordLead, *ordCount, *ordImageShows, *ordImageAll, *ordOpenImages;
  // A row's link to its screen's page, ordSettingsOf %s: the screen's name;
  // a row's drag handle, ordHandle %s %s %s: the screen's name, its place
  // and the count (formatText); the rows' summaries: ordMoneySum %s %s %u %u
  // the salary (grouped), the currency's code, the day's start and end;
  // ordImagesAll %d the images stored, all in turn.
  const char *ordSettingsOf, *ordHandle, *ordMoneySum, *ordImagesAll;

  // --- Web: Panel (theatre pages) ---------------------------------------------------
  // pnlMqttTitle %s: the broker's state (webMqttOff / On / Down); pnlHooksMany
  // %s: the sessions, as digits (both filled by formatText). webNightMode,
  // webNightHint and webApiHint serve it too.
  const char *pnlLead, *pnlUpdates, *pnlWifi, *pnlAdvanced, *pnlLanguageLabel, *pnlSaveScope;
  // Region: the section's name, its three controls' legends and one line
  const char *pnlRegion, *pnlTemperature, *pnlCurrency, *pnlClock, *pnlRegionHint;
  // Feedback: the choices in their enums' order (feedback_style.h), a name
  // and one line each; the kinds' names; Show / Discreet / Off and their lines
  const char *pnlFeedback, *pnlTransition, *pnlBrightnessChange, *pnlKnobTurn, *pnlRequests;
  const char *pnlTransitions[4], *pnlTransitionHints[4];
  const char *pnlBrightnessFeedbacks[4], *pnlBrightnessHints[4];
  const char *pnlKnobFeedbacks[4], *pnlKnobHints[4];
  const char *pnlRequestKinds[4], *pnlRequestShows[3], *pnlRequestShowHints[3];
  // The boot screen, under Advanced: BootDetail order, and one line
  const char *pnlBoot, *pnlBoots[3], *pnlBootHint;
  const char *pnlSignal[3];    // SignalWord order: good, fair, weak
  const char *pnlMqttTitle, *pnlHooksNone, *pnlHooksOne, *pnlHooksMany, *pnlHooksHint;
  // The hooks fold: the install command (Copy, and the two things it says),
  // the refusal line (pnlHookRefused %s: the count, as digits), the setting
  // for hooks without a token (the line says which, the button changes it),
  // the renewal, and the old addresses that still work
  const char *pnlHookInstall, *pnlHookInstallHint, *pnlHookUninstall;
  const char *pnlCopy, *pnlCopied, *pnlCopyByHand, *pnlHookRefused;
  const char *pnlHookOpenOff, *pnlHookOpenOn, *pnlHookAccept, *pnlHookRefuse, *pnlHookOpenWarn;
  const char *pnlHookRenew, *pnlHookRenewHint, *pnlHookLegacy;
  const char *pnlApiTitle, *pnlToken, *pnlMemoryTitle, *pnlRestartHint;
};

extern const Strings STRINGS[LANG_COUNT];

void setLanguage(uint8_t lang);   // out-of-range values fall back to English
const Strings &tr();

// The locked page's text. A boot lock (reset marker present or unreadable)
// is resumed at each start; a knob reset that wrote no marker never is, so
// its text asks to hold the knob again.
const char *resetFailedText(const Strings &t, bool knobLock);

// UTF-8 typed on the web page -> printable ASCII: "Café été" -> "Cafe ete".
void toPanelText(const char *in, char *out, size_t size);
// The same for the TEXT message, which the body face draws: the French
// letters stay as UTF-8 ("Café à 14h"), never cut to fit; the rest folds.
void toMessageText(const char *in, char *out, size_t size);
