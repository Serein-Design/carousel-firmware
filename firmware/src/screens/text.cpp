// SPDX-License-Identifier: AGPL-3.0-only
// TEXT: the message set on the web page, in the body face and the text colour,
// crossing the panel from right to left at a calm speed (text_logic.h).
#include "display.h"
#include "palette.h"
#include "screens/screens.h"
#include "settings.h"
#include "text_logic.h"
#include "type.h"

void screenText(uint32_t f, bool init) {
  (void)f;
  static uint32_t since;
  if (init) since = millis();

  char msg[sizeof(settings.message)];
  messageDrawable(settings.message, msg, sizeof(msg));

  display->clearScreen();
  const int16_t x = textScrollX(textWidth(msg, Face::BODY), W(), millis() - since);
  drawText(*display, msg, x, TEXT_Y, Face::BODY, palette().text);
}
