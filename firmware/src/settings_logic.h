// SPDX-License-Identifier: AGPL-3.0-only
// Writing the settings to NVS, without the hardware: which key holds what,
// and whether every write really landed. Checked on the PC by
// test/test_settings.
#pragma once

#include <Preferences.h>

#include "settings.h"

// The screen list and the interval, at start. Never writes: a value the
// firmware cannot use (unknown names, an interval not offered, an
// orientation left without screens) is corrected in RAM until the owner
// saves the settings page. A list filled that way is not written by other
// saves (screensStored false, screensFilled set).
void readScreens(Preferences &p, Settings &s);

// How CODE, CLOCK and WEATHER draw, at start. Never writes: an absent key,
// another type, or a style this firmware does not know reads as the default;
// ClockShow bits it does not know are dropped.
void readStyles(Preferences &p, Settings &s);

// The temperature unit, the currency and the 12 h clock, at start. Never
// writes: an absent key, another type or a value this firmware does not
// know reads as the default (°C, EUR, 24 h), what v0.1.0 panels showed.
void readRegion(Preferences &p, Settings &s);

// The phone's region at Wi-Fi setup, into s and NVS, each value only where
// its key is absent: a first setup, a reset panel, or one whose region no
// save has written yet. A stored key is the owner's choice and stays, as
// does a value the form did not post (-1). False when NVS refused a write;
// that value is left out of s too.
bool presetRegion(Preferences &p, Settings &s, const RegionChoice &phone);

// Every setting under its key, in a begin()/end() session the caller opened.
// False when NVS refused any write (full, failing flash); the other keys are
// still tried.
bool writeSettings(Preferences &p, const Settings &s);

// Writes next. With before (the settings form), the screen keys too, and if
// NVS refuses part of it, before is written back, so that a restart does not
// load half of each. Without before (every other save), the screen keys stay
// as stored. False when next was not stored whole.
bool storeSettings(Preferences &p, const Settings &next, const Settings *before);

// The settings form saves a copy taken when its request came in, and the
// HTTPS task may have found the place, its coordinates or its zone since.
// Unless the form changed the city (moved), posted takes them from now, so
// that putting the copy back drops none (the zone would cost another
// lookup in the city table).
void keepLocation(Settings &posted, const Settings &now, bool moved);

#ifdef SCREENS_TEST_CONSOLE
// Development builds only: every later write of the interval is refused, as
// a full NVS would, until the next restart.
void settingsRefuseRotateWrites();
// Development builds only: locks the panel as an unfinished factory reset
// would, for the rest of this start, without erasing anything: knob false
// as a reset the next start resumes (its marker), true as a knob reset that
// wrote none. NVS is closed until the restart.
void settingsTestLock(bool knob);
#endif
