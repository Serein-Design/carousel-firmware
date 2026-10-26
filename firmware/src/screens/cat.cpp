// SPDX-License-Identifier: AGPL-3.0-only
// ---------------------------------------------------------------------------
// CAT (portrait): a tuxedo cat sitting on a window ledge, at night.
// It blinks, swishes its tail, yawns, follows a firefly with its eyes,
// and ends up falling asleep curled into a ball before waking up again.
// ---------------------------------------------------------------------------
#include "display.h"
#include "palette.h"
#include "screens/screens.h"
#include "type.h"

namespace cat {
  const uint32_t CYCLE = 2400;
  float    fireflyX, fireflyY;
  float    meteorX, meteorY;
  int      meteorLife = 0;
  uint32_t nextBlink = 80;
}

void screenCat(uint32_t f, bool init) {
  if (init) { cat::fireflyX = 8; cat::fireflyY = 14; cat::meteorLife = 0; cat::nextBlink = f + 80; }
  const uint32_t t = f % cat::CYCLE;
  const bool asleep  = t > 1600 && t < 2250;
  const bool yawning = t > 700  && t < 800;

  // --- Wall and window -----------------------------------------------------
  display->fillScreen(display->color565(60, 45, 70));
  const int wx = 4, wy = 3, ww = 24, wh = 30;
  for (int y = wy; y < wy + wh; y++) {
    const float k = (y - wy) / (float)wh;   // the city lights up the bottom of the sky
    display->drawFastHLine(wx, y, ww, display->color565(15 + 45 * k, 20 + 40 * k, 50 + 60 * k));
  }
  for (int i = 0; i < 10; i++)
    if (sinTab[(uint8_t)(f * 2 + i * 70)] > 70)
      display->drawPixel(wx + 2 + (i * 7) % 20, wy + 2 + (i * 5) % 14, display->color565(200, 200, 230));
  display->fillCircle(wx + 18, wy + 7, 3, display->color565(240, 235, 190));   // moon
  display->fillCircle(wx + 20, wy + 6, 3, display->color565(12, 17, 48));      // its crescent

  for (int x = wx; x < wx + ww; x++) {                                         // city rooftops
    const int h = 3 + ((x / 4) * 7) % 5;
    display->drawFastVLine(x, wy + wh - h, h, display->color565(45, 45, 75));
    if ((x * 3) % 7 == 0 && h > 4)
      display->drawPixel(x, wy + wh - h + 2, display->color565(255, 200, 90));
  }

  if (cat::meteorLife <= 0 && random(0, 700) == 0) { cat::meteorX = wx + 20; cat::meteorY = wy + 3; cat::meteorLife = 14; }
  if (cat::meteorLife > 0) {                                                   // shooting star
    display->drawPixel((int)cat::meteorX, (int)cat::meteorY, WHITE);
    display->drawPixel((int)cat::meteorX + 1, (int)cat::meteorY - 1, display->color565(120, 120, 150));
    cat::meteorX -= 1; cat::meteorY += 0.5f; cat::meteorLife--;
  }

  const uint16_t wood = display->color565(130, 85, 45);
  display->drawRect(wx - 1, wy - 1, ww + 2, wh + 2, wood);
  display->drawRect(wx - 2, wy - 2, ww + 4, wh + 4, wood);
  display->drawFastVLine(wx + ww / 2, wy, wh, wood);
  display->drawFastHLine(wx, wy + wh / 2, ww, wood);
  display->fillRect(0, wy + wh + 1, W(), 3, display->color565(160, 110, 60));  // ledge

  // Firefly fluttering behind the glass
  cat::fireflyX = wx + 12 + sinf(f * 0.023f) * 9 + sinf(f * 0.07f) * 2;
  cat::fireflyY = wy + 14 + sinf(f * 0.031f) * 8;
  if ((f / 7) % 5) display->drawPixel((int)cat::fireflyX, (int)cat::fireflyY, display->color565(200, 255, 80));

  // --- The tuxedo cat ------------------------------------------------------
  const uint16_t fur   = display->color565(20, 18, 25),  edge  = display->color565(55, 50, 65);
  const uint16_t white = display->color565(235, 235, 240), shade = display->color565(185, 185, 195);
  const int cx = 16, ground = wy + wh + 1;

  if (asleep) {
    display->fillCircle(cx, ground - 4, 5, fur);                   // curled up
    display->fillCircle(cx - 4, ground - 5, 3, fur);
    display->drawPixel(cx - 6, ground - 8, fur); display->drawPixel(cx - 3, ground - 8, fur);
    display->fillRect(cx - 7, ground - 4, 3, 2, white);            // white muzzle and chin
    display->drawFastHLine(cx - 6, ground - 6, 2, edge);           // closed eyes
    display->fillRect(cx - 3, ground - 2, 2, 2, white);
    display->fillRect(cx,     ground - 2, 2, 2, white);            // socks tucked in
    if (f % 90 < 60) {
      drawText(*display, "Z", cx + 3, ground - 14 - (int)((f % 90) / 15), Face::LABEL, palette().label);
    }
  } else {
    display->fillCircle(cx, ground - 6, 6, fur);                   // body
    display->fillRect(cx - 6, ground - 6, 13, 6, fur);
    const int head = ground - 16;
    for (int y = head + 5; y < ground - 1; y++) {                  // chest bib widening towards the bottom
      const int half = min(3, (y - head - 5) / 2);
      display->drawFastHLine(cx - half, y, 2 * half + 1, y > ground - 4 ? shade : white);
    }
    display->fillRect(cx - 4, ground - 2, 3, 2, white);            // socks
    display->fillRect(cx + 2, ground - 2, 3, 2, white);
    display->drawPixel(cx - 2, ground - 1, shade); display->drawPixel(cx + 2, ground - 1, shade);

    display->fillCircle(cx, head, 5, fur);
    display->drawLine(cx - 5, head - 2, cx - 4, head - 7, fur);    // ears
    display->drawLine(cx - 4, head - 7, cx - 1, head - 4, fur);
    display->drawLine(cx + 5, head - 2, cx + 4, head - 7, fur);
    display->drawLine(cx + 4, head - 7, cx + 1, head - 4, fur);
    if ((f / 150) % 4 == 1) display->drawPixel(cx + 4, head - 6, edge);   // twitching ear

    display->drawFastVLine(cx, head - 3, 3, white);                // white stripe between the eyes
    display->fillRect(cx - 2, head + 1, 5, 3, white);              // white muzzle
    display->fillRect(cx - 1, head + 4, 3, 1, white);              // chin

    // The eyes follow the firefly
    const int rx = constrain((int)((cat::fireflyX - cx) / 6), -1, 1);
    const int ry = constrain((int)((cat::fireflyY - head) / 10), -1, 0);
    if (isBlinking(f, cat::nextBlink)) {
      display->drawFastHLine(cx - 3, head, 2, display->color565(120, 200, 80));
      display->drawFastHLine(cx + 2, head, 2, display->color565(120, 200, 80));
    } else {
      const int offsets[2] = { -3, 2 };
      for (int ox : offsets) {
        display->fillRect(cx + ox, head - 1, 2, 2, display->color565(140, 230, 90));
        display->drawPixel(cx + ox + (rx > 0 ? 1 : 0), head - 1 + (ry < 0 ? 0 : 1), BLACK);
      }
    }
    display->drawPixel(cx, head + 1, display->color565(230, 120, 150));   // pink nose
    display->drawPixel(cx, head + 2, edge);                               // little mouth
    if (yawning) {
      display->fillRect(cx - 1, head + 2, 3, 2, display->color565(200, 60, 90));
      display->drawPixel(cx, head + 2, WHITE);
    }
    for (int s = -1; s <= 1; s += 2) {                                    // white whiskers
      display->drawLine(cx + s * 3, head + 2, cx + s * 8, head + 1, display->color565(210, 210, 220));
      display->drawLine(cx + s * 3, head + 3, cx + s * 8, head + 4, display->color565(160, 160, 175));
    }
    const float q = sinf(f * 0.06f) * 4;                                  // swishing tail
    display->drawLine(cx + 6, ground - 2, cx + 10, ground - 1, fur);
    display->drawLine(cx + 10, ground - 1, cx + 12 + (int)(q / 2), ground + 5, fur);
    display->drawLine(cx + 12 + (int)(q / 2), ground + 5, cx + 11 + (int)q, ground + 11, fur);
  }

  // Lower wall with a radiator
  display->fillRect(4, ground + 14, 24, 12, display->color565(200, 200, 205));
  for (int x = 6; x < 27; x += 3)
    display->drawFastVLine(x, ground + 15, 10, display->color565(160, 160, 170));
}
