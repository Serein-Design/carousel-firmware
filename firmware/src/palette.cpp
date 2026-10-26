// SPDX-License-Identifier: AGPL-3.0-only
#include "palette.h"

#include "settings.h"

static constexpr uint16_t rgb565(uint32_t rgb) {
  return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

// The design's values, picked through a model of the panel (6-bit colour,
// the driver's CIE table) and still to be confirmed on it at levels 1 and 5:
// the calibration screen of development builds shows them.
static const Palette CHAUD = {
  rgb565(0xE8C9A0),   // text: beige, blue lowered so it never reads white
  rgb565(0x9A5E46),   // label: ember; the brand's charbon cannot be lit
  rgb565(0xE8893A),   // accent: orange
  rgb565(0xC8323C),   // alert: lifted bordeaux
  rgb565(0x5A3E2C),   // dim
  rgb565(0x3A281C),   // ghost: may vanish at the lowest levels, by design
  0,
};
// Monochrome amber, almost no blue. The design gives no night ghost: the day
// one is kept, already gone at night brightness on the panel model.
static const Palette AMBRE = {
  rgb565(0xD0742A), rgb565(0x5E2C0C), rgb565(0xFFA040), rgb565(0xC8323C),
  rgb565(0x3E1C08), rgb565(0x3A281C), 0,
};

// The lowest brightness (levels 1 and 2, night 25 and under): on the panel at
// level 1, ghost and dim nearly vanished and the label sat too close to dim
// (owner, 2026-10-19). The quiet roles are lit higher there, kept apart; the
// loud ones do not change.
static const Palette CHAUD_LOW = {
  CHAUD.text,
  rgb565(0xC07A48),   // label: a saturated ember, apart from the beige text by hue
  CHAUD.accent, CHAUD.alert,
  rgb565(0x7A5A42),   // dim
  rgb565(0x5A3E2C),   // ghost: the day dim
  0,
};
static const Palette AMBRE_LOW = {
  rgb565(0xE08838),   // text: a step up, "LUN. 19" was at the limit
  rgb565(0xA85A1C),   // label
  AMBRE.accent, AMBRE.alert,
  rgb565(0x74380E),   // dim
  rgb565(0x4E260A),   // ghost
  0,
};

const Palette &paletteFor(bool night, bool low) {
  if (low) return night ? AMBRE_LOW : CHAUD_LOW;
  return night ? AMBRE : CHAUD;
}

bool lowLight(uint8_t brightness8) { return brightness8 <= 25; }

const Palette &paletteAtHour(int h, int start, int end) {
  return paletteFor(inHourWindow(h, start, end));
}
