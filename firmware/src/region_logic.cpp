// SPDX-License-Identifier: AGPL-3.0-only
#include "region_logic.h"

void regionInfoJson(String &j, uint8_t tempUnit, uint8_t currency, bool clock12) {
  j += tempUnit == TEMP_F ? ",\"temp_unit\":\"F\"" : ",\"temp_unit\":\"C\"";
  j += ",\"currency\":\"";
  j += currencyCode(currency);
  j += clock12 ? "\",\"clock12\":true" : "\",\"clock12\":false";
}
