// SPDX-License-Identifier: AGPL-3.0-only
// The status strip (grid G1, row 31; the bottom row in portrait): on the
// screens drawn on the system grid, a one-pixel ghost line fills from the
// left over the rotation's interval, and a 2-pixel mark at the right end
// says what the agents do. Full-art screens (images, ambiances, the
// mascot's screens, CAT, ISS) keep their bottom row. gallery.cpp draws it
// after the screen, before the overlays; the hold line, drawn last, covers
// it. Hardware-free: host-tested in test_status_strip.
#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

#include "agent_bridge.h"
#include "palette.h"

// The agents' mark: the owner is needed (accent), one is at work (dim), or
// nothing to say (no session, or all of them idle, done or stopped)
enum class AgentMark : uint8_t { NONE, WORKING, NEEDS_OWNER };

// From the active sessions' states; one that needs the owner wins
AgentMark agentMark(const AgentState *states, int n);

// Whether the screen (firmware index) is drawn on the system grid
bool screenHasStrip(uint8_t screen);

// fill: rotationFill(), negative for no line. Nothing at all on a screen
// without the strip; no mark on CODE, which shows the sessions itself.
// Draws only its lit pixels: the screen cleared the row.
void drawStatusStrip(Adafruit_GFX &g, const Palette &p, uint8_t screen, float fill, AgentMark mark);
