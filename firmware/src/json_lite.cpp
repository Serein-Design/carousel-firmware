// SPDX-License-Identifier: AGPL-3.0-only
#include "json_lite.h"

bool extractJsonString(const char *src, const char *key, char *out, size_t size) {
  const char *p = strstr(src, key);
  if (!p) return false;
  p = strchr(p + strlen(key), ':');
  if (!p) return false;
  p = strchr(p, '"');
  if (!p) return false;
  const char *q = strchr(++p, '"');
  if (!q) return false;
  const size_t n = min((size_t)(q - p), size - 1);
  memcpy(out, p, n);
  out[n] = '\0';
  return true;
}

// The first occurrence whose value is a number wins: an answer may name the
// same key first in a units object, where it holds a string ("°C").
bool extractJsonNumber(const String &src, const char *key, float &out) {
  for (int k = src.indexOf(key); k >= 0; k = src.indexOf(key, k + 1)) {
    int i = k + strlen(key);
    // Skip what separates the key from its value. The quote is included
    // because the YouTube API returns its counters as strings: "12345".
    while (i < (int)src.length() &&
           (src[i] == ':' || src[i] == '[' || src[i] == ' ' || src[i] == '"')) i++;
    int start = i;
    while (i < (int)src.length() &&
           (isdigit((int)src[i]) || src[i] == '-' || src[i] == '.')) i++;
    if (i == start) continue;
    out = src.substring(start, i).toFloat();
    return true;
  }
  return false;
}

static size_t putUtf8(char *out, size_t room, uint32_t cp) {
  char b[3];
  size_t n;
  if (cp < 0x80)       { b[0] = (char)cp; n = 1; }
  else if (cp < 0x800) { b[0] = (char)(0xC0 | cp >> 6); b[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
  else                 { b[0] = (char)(0xE0 | cp >> 12); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                         b[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
  if (n > room) return 0;
  memcpy(out, b, n);
  return n;
}

bool jsonValue(const char *src, const char *name, char *out, size_t size) {
  if (!size) return false;
  const size_t nameLen = strlen(name);
  for (const char *p = strchr(src, '"'); p; p = strchr(p + 1, '"')) {
    // A key is a quoted name followed by ':'; values that happen to equal
    // the name are skipped the same way.
    if (strncmp(p + 1, name, nameLen) || p[1 + nameLen] != '"') continue;
    const char *v = p + 2 + nameLen;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    if (*v != ':') continue;
    v++;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    size_t n = 0;
    if (*v == '{' || *v == '[') return false;
    if (*v != '"') {                                   // number, true, false, null
      while (*v && *v != ',' && *v != '}' && *v != ' ' && *v != '\n' && *v != '\r' && n + 1 < size)
        out[n++] = *v++;
      out[n] = '\0';
      return n > 0;
    }
    for (v++; *v && *v != '"'; v++) {
      uint32_t c = (uint8_t)*v;
      if (c == '\\') {
        v++;
        switch (*v) {
          case 'n': c = '\n'; break;
          case 't': c = '\t'; break;
          case 'r': c = '\r'; break;
          case 'u': {
            c = 0;
            for (int k = 1; k <= 4; k++) {
              const char h = v[k];
              const int d = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10
                          : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
              if (d < 0) return false;
              c = c << 4 | d;
            }
            v += 4;
            const size_t w = putUtf8(out + n, size - 1 - n, c);
            if (!w) break;
            n += w;
            continue;
          }
          case '\0': return false;
          default: c = (uint8_t)*v; break;           // \" \\ \/
        }
      }
      if (n + 1 >= size) break;
      out[n++] = (char)c;
    }
    out[n] = '\0';
    return true;
  }
  return false;
}
