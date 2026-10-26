// SPDX-License-Identifier: AGPL-3.0-only
// The states nobody chooses, on the system grid and the palette's roles: the
// sign-in code (S1), a locked panel (L1), a service with no fresh data (E1)
// and no image (I1). What they say and where the digits sit is decided in
// states_logic.cpp.
#include "display.h"
#include "grid.h"
#include "panel_name.h"
#include "i18n.h"
#include "palette.h"
#include "screens/screens.h"
#include "states_logic.h"
#include "type.h"
#include "weather_logic.h"

namespace {

// A 1-bit picture in a colour: '#' lit, anything else left alone
void sprite(const char *const *rows, int n, int x, int y, uint16_t c) {
  for (int j = 0; j < n; j++)
    for (int i = 0; rows[j][i]; i++)
      if (rows[j][i] == '#') display->drawPixel(x + i, y + j, c);
}

const char *const LOCK[] = {".###.", "#...#", "#####", "#####", "##.##", "#####"};
const char *const PICTURE[] = {"#####", "#...#", "#..##", "#.###", "#####"};   // a frame and a hill

bool tallFrame() { return H() > W(); }

}  // namespace

// S1: the title, six Pierre digits in two groups of three, and under them a
// line that shortens as the code runs out. Portrait stacks the groups.
// digits: six characters, the code's (the web page's poster draws dashes).
void drawSignInCode(const char *digits, float left) {
  const Palette &p = palette();
  const bool tall = tallFrame();
  display->clearScreen();
  drawText(*display, tall ? tr().signInPanelTall : tr().signInPanel, W() / 2, GRID_LABEL_TOP,
           Face::LABEL, p.label, Align::CENTER);
  for (uint8_t i = 0; i < 6; i++) {
    const DigitSlot s = signInSlot(i, tall);
    const char one[2] = {digits[i], 0};
    drawText(*display, one, s.x, s.y, Face::PIERRE, p.text);
  }
  display->fillRect(GRID_MARGIN, tall ? SIGNIN_BAR_Y_TALL : SIGNIN_BAR_Y,
                    signInBarWidth(left, tall ? GRID_PORTRAIT_LINE_W : GRID_LINE_W), 1, p.accent);
}

// L1: a lock and LOCKED in alert, what happened, what to do, and where the
// pages are when the name fits a line. Portrait keeps the word and the step.
void drawLockedState(const Strings &s) {
  const Palette &p = palette();
  display->clearScreen();
  if (!tallFrame()) {
    sprite(LOCK, 6, GRID_MARGIN, 1, p.alert);
    drawText(*display, s.locked[0], 8, GRID_LABEL_TOP, Face::LABEL, p.alert);
    drawText(*display, s.locked[1], GRID_MARGIN, 10, Face::LABEL, p.text);
    drawText(*display, s.locked[2], GRID_MARGIN, 18, Face::LABEL, p.text);
    const AddressLines a = addressLines(panelHostName(), GRID_LINE_W);
    if (a.count == 1) drawText(*display, a.line[0], GRID_MARGIN, 26, Face::LABEL, p.dim);
    return;
  }
  sprite(LOCK, 6, (W() - 5) / 2, 12, p.alert);
  drawText(*display, s.lockedTall[0], W() / 2, 22, Face::LABEL, p.alert, Align::CENTER);
  drawText(*display, s.lockedTall[1], W() / 2, 32, Face::LABEL, p.text, Align::CENTER);
  drawText(*display, s.lockedTall[2], W() / 2, 39, Face::LABEL, p.text, Align::CENTER);
}

// I1: "No image", then the one thing to do, with the address in the accent.
// A name too wide for a line (the panel's own) takes the next line.
void drawNoImage() {
  const Palette &p = palette();
  const Strings &t = tr();
  display->clearScreen();
  if (!tallFrame()) {
    sprite(PICTURE, 5, GRID_MARGIN, 3, p.dim);
    drawText(*display, t.noImage, 8, 1, Face::BODY, p.text);
    const AddressLines a = addressLines(panelHostName(), GRID_LINE_W);
    const int16_t ask = a.count > 1 ? 11 : 17;
    drawText(*display, t.noImageAsk, GRID_MARGIN, ask, Face::LABEL, p.dim);
    for (uint8_t i = 0; i < a.count; i++)
      drawText(*display, a.line[i], GRID_MARGIN, ask + 6 + 6 * i, Face::LABEL, p.accent);
    return;
  }
  sprite(PICTURE, 5, (W() - 5) / 2, 1, p.dim);
  drawText(*display, t.noImageTall[0], W() / 2, 9, Face::BODY, p.text, Align::CENTER);
  drawText(*display, t.noImageTall[1], W() / 2, 18, Face::BODY, p.text, Align::CENTER);
  drawText(*display, t.noImageTall[2], W() / 2, 32, Face::LABEL, p.dim, Align::CENTER);
  drawText(*display, t.noImageTall[3], W() / 2, 39, Face::LABEL, p.dim, Align::CENTER);
  drawText(*display, t.noImageTall[4], W() / 2, 46, Face::LABEL, p.accent, Align::CENTER);
}

// E1: what the screen drew stays, at half its light, and a foot band says in
// a word why it is old. Called after the screen: the band hides the rows
// under it, so a screen with something there makes room when it is old.
void drawStale(Reason why) {
  const char *word = reasonWord(why, tr());
  if (!word) return;
  const Palette &p = palette();
  const bool tall = tallFrame();
  char a[32], b[32] = "";
  if (tall) splitLines(word, Face::LABEL, GRID_PORTRAIT_LINE_W, a, b, sizeof(a));
  else snprintf(a, sizeof(a), "%s", word);
  const int lines = b[0] ? 2 : 1;
  const int16_t top = H() - (2 + 6 * lines);
  for (int16_t y = 0; y < top; y++)
    for (int16_t x = 0; x < W(); x++) display->drawPixel(x, y, dimmed(display->getPixel(x, y)));
  display->fillRect(0, top, W(), H() - top, p.off);
  display->drawFastHLine(0, top, W(), p.dim);
  if (!tall) {
    // The alert's mark, a bang: the reason is never red alone
    display->drawFastVLine(GRID_MARGIN, top + 1, 3, p.alert);
    display->drawPixel(GRID_MARGIN, top + 5, p.alert);
    drawText(*display, a, GRID_MARGIN + 4, top + 1, Face::LABEL, p.text);
    return;
  }
  drawText(*display, a, W() / 2, top + 1, Face::LABEL, p.text, Align::CENTER);
  if (b[0]) drawText(*display, b, W() / 2, top + 7, Face::LABEL, p.text, Align::CENTER);
}
