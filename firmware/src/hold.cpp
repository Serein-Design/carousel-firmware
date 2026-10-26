// SPDX-License-Identifier: AGPL-3.0-only
#include "hold.h"

#include <math.h>

#include "palette.h"

float holdFill(uint32_t heldMs, uint32_t fullMs, uint32_t showMs) {
  if (heldMs <= showMs) return 0;
  if (heldMs >= fullMs) return 1;
  return (float)(heldMs - showMs) / (fullMs - showMs);
}

void drawHoldLine(Adafruit_GFX &g, float fill) {
  if (fill <= 0) return;
  const Palette &p = palette();
  const int16_t y = g.height() - 1, w = g.width();
  const int16_t lit = fill >= 1 ? w : (int16_t)lroundf(w * fill);
  // The row is cleared first: the strip's own line would read as part of it
  g.drawFastHLine(0, y, w, p.off);
  if (lit > 0) g.drawFastHLine(0, y, lit, p.accent);
}
