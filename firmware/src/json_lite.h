// SPDX-License-Identifier: AGPL-3.0-only
// Value extraction from small JSON payloads, without a library. Enough for
// flat API answers and hook payloads; not a parser.
#pragma once

#include <Arduino.h>

// "key":"value" -> value, truncated to size-1 characters
bool extractJsonString(const char *src, const char *key, char *out, size_t size);

// First number after "key": arrays are skipped into, quoted numbers accepted
bool extractJsonNumber(const String &src, const char *key, float &out);

// Value of a top-level member of a flat JSON object, as text: strings are
// unescaped (\" \\ \n \uXXXX, the latter to UTF-8), numbers and literals
// copied as written. False if the member is missing or the value is an
// object or array. For small request bodies such as {"text":"Hi","seconds":5}.
bool jsonValue(const char *src, const char *name, char *out, size_t size);
