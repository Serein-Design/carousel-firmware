// SPDX-License-Identifier: AGPL-3.0-only
// The panel made dark by hand, without the DMA: OE off, then zeros shifted
// into every column and latched.
//
// Why: the panel's column drivers keep what they last latched and show it on
// the selected row whenever OE is low. When nothing drives OE (before setup(),
// during a restart, after the brownout reset as the power falls), it floats
// and that content lights: power-on noise (the thin grey line before the
// mark) or the last row the DMA sent (the few red, green or blue LEDs at power
// off). Zeros in the latches leave nothing to show, whatever OE does next.
//
// Header only, the sequence inlined: the brownout handler runs it from IRAM,
// with the flash already off.
#pragma once

#include <stdint.h>

struct PanelBlankPins {
  int8_t oe, lat, clk;
  int8_t data[6];   // R1 G1 B1 R2 G2 B2
};

// out.level(pin, high) sets what a pin drives; out.take(pin) makes it a plain
// output at that level, out of the DMA's hands. OE goes first, already high
// (off), so the output is never enabled while the old content is latched.
template <typename Out>
inline __attribute__((always_inline)) void panelLatchBlank(const PanelBlankPins &pins,
                                                           uint16_t columns, const Out &out) {
  out.level(pins.oe, true);
  out.take(pins.oe);
  out.level(pins.lat, false);
  out.take(pins.lat);
  out.level(pins.clk, false);
  out.take(pins.clk);
  for (int8_t pin : pins.data) {
    out.level(pin, false);
    out.take(pin);
  }
  for (uint16_t i = 0; i < columns; i++) {   // zeros in on each rising edge
    out.level(pins.clk, true);
    out.level(pins.clk, false);
  }
  out.level(pins.lat, true);
  out.level(pins.lat, false);
}
