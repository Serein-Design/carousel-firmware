// SPDX-License-Identifier: AGPL-3.0-only
#include "calibration.h"

#ifdef SCREENS_TEST_CONSOLE
#include "grid.h"
#include "type.h"

static bool shown = false;
static bool night = false;

void calibrationToggle() { shown = !shown; }
void calibrationFlipPalette() { night = !night; }
bool calibrationShown() { return shown; }
const Palette &calibrationPalette(bool low) { return paletteFor(night, low); }

static const int16_t SWATCH_W = 20, SWATCH_H = 5;

void drawCalibration(Adafruit_GFX &g, const Palette &p, bool facesPage) {
  if (facesPage) {
    const int16_t mid = g.width() / 2;
    drawText(g, "LUN. 19 OCT.", mid, GRID_LABEL_TOP, Face::LABEL, p.label, Align::CENTER);
    drawText(g, "Vous attend", mid, GRID_MAIN_TOP, Face::BODY, p.text, Align::CENTER);
    // the 13-row digits under the body line, down to row 30
    drawText(g, "14:32", mid, 18, Face::PIERRE, p.text, Align::CENTER);
    return;
  }
  // off is the panel's black: nothing to see, no row. Six rows of 5 fill
  // rows 1-30 with no gap between them.
  struct Role { const char *name; uint16_t color; };
  const Role roles[] = {{"TEXT", p.text}, {"LABEL", p.label}, {"ACCENT", p.accent},
                        {"ALERT", p.alert}, {"DIM", p.dim},   {"GHOST", p.ghost}};
  const int16_t blockX = g.width() - GRID_MARGIN - SWATCH_W;
  for (int i = 0; i < 6; i++) {
    const int16_t y = 1 + i * SWATCH_H;
    drawText(g, roles[i].name, GRID_MARGIN, y, Face::LABEL, roles[i].color);
    for (int16_t r = 0; r < SWATCH_H; r++)
      for (int16_t c = 0; c < SWATCH_W; c++) g.drawPixel(blockX + c, y + r, roles[i].color);
  }
}
#endif
