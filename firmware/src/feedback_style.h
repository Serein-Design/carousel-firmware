// SPDX-License-Identifier: AGPL-3.0-only
// How the panel answers: the passage from one screen to the next, what a
// brightness change or a knob turn shows, and how each kind of request
// shows itself. The owner picks them on Panel; the default comes first.
#pragma once

#include <stdint.h>

enum Transition : uint8_t {
  TRANSITION_PUSH,       // the next screen pushes the last one out, 240 ms
  TRANSITION_DISSOLVE,   // ordered dither, 400 ms
  TRANSITION_WIPE,
  TRANSITION_CUT,
  TRANSITION_COUNT
};

enum BrightnessFeedback : uint8_t {
  BRIGHTNESS_DOTS,       // a bottom band: a sun, six steps, "4/6"
  BRIGHTNESS_GAUGE,      // a side gauge
  BRIGHTNESS_NUMBER,     // one big number
  BRIGHTNESS_NONE,
  BRIGHTNESS_FEEDBACK_COUNT
};

enum KnobFeedback : uint8_t {
  KNOB_BAND,             // a band with the screen's name and "4/12"
  KNOB_NEIGHBOURS,       // the screens before and after
  KNOB_DOTS,             // one dot per screen, the current one lit
  KNOB_NONE,
  KNOB_FEEDBACK_COUNT
};

// How much the boot screen says (boot_logic.h)
enum BootDetail : uint8_t {
  BOOT_AUTO,             // Detailed on a development build, Simple on a release
  BOOT_SIMPLE,           // the mark, then the Carousel word and its loading line
  BOOT_DETAILED,         // the same with the IP under the line once joined
  BOOT_DETAIL_COUNT
};

// What a request shows, per kind
enum RequestShow : uint8_t {
  REQUEST_SHOW,          // the kind's own way (band, full screen, chip)
  REQUEST_DISCREET,      // a corner chip only
  REQUEST_OFF,           // nothing beyond the change itself
  REQUEST_SHOW_COUNT
};

enum RequestKind : uint8_t {
  REQUEST_MESSAGE,       // a text to read: a band that scrolls it twice
  REQUEST_IMAGE,         // an image sent: full screen for its play
  REQUEST_COMMAND,       // a screen or brightness change: a corner chip naming the source
  REQUEST_AGENT,         // the coding agent needs the owner: CODE comes on stage
  REQUEST_KIND_COUNT
};

// Who sent a request, for the chip
enum RequestSource : uint8_t { SOURCE_WEB, SOURCE_API, SOURCE_HOME_ASSISTANT, SOURCE_AGENT, SOURCE_COUNT };
