// SPDX-License-Identifier: AGPL-3.0-only
#include "agent_install.h"

#include <cstdio>
#include <cstring>

// Every character of s is in the set, and there is at least one
static bool only(const char *s, const char *set, size_t maxLen) {
  if (!s || !*s) return false;
  const size_t n = strlen(s);
  return n <= maxLen && strspn(s, set) == n;
}

int installPrelude(char *out, size_t size, const char *token, const char *ip, const char *name) {
  if (!out || !size) return -1;
  out[0] = '\0';
  if (!token || strlen(token) != 32 || !only(token, "0123456789abcdef", 32)) return -1;
  if (!only(ip, "0123456789.", 15)) return -1;
  if (!only(name, "abcdefghijklmnopqrstuvwxyz0123456789-", 63)) return -1;
  const int n = snprintf(out, size, "#!/bin/sh\nCAROUSEL_TOKEN='%s'\nCAROUSEL_IP='%s'\nCAROUSEL_NAME='%s'\n",
                         token, ip, name);
  if (n < 0 || (size_t)n >= size) {
    out[0] = '\0';
    return -1;
  }
  return n;
}

void installGzipHead(uint8_t *out, size_t preludeLen) {
  static const uint8_t GZIP[10] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0xff};   // deflate, no name, no time
  memcpy(out, GZIP, sizeof(GZIP));
  const uint16_t len = (uint16_t)preludeLen;
  out[10] = 0x00;                       // a stored block, not the last one
  out[11] = len & 0xff;
  out[12] = len >> 8;
  out[13] = ~len & 0xff;
  out[14] = (uint16_t)~len >> 8;
}

// CRC-32 (gzip's), reflected, a bit at a time: the prelude is short, and a
// table would cost 1 KB of flash
static uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    for (int k = 0; k < 8; k++) crc = crc & 1 ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
  }
  return ~crc;
}

// a * b modulo the CRC polynomial, bits reflected as in the CRC (zlib's multmodp)
static uint32_t multModP(uint32_t a, uint32_t b) {
  uint32_t p = 0;
  for (uint32_t m = 1u << 31; m; m >>= 1) {
    if (a & m) p ^= b;
    b = b & 1 ? (b >> 1) ^ 0xedb88320u : b >> 1;
  }
  return p;
}

// The CRC of a then b, from their CRCs and the length of b (zlib's
// crc32_combine): the CRC of a, moved past len zero bytes, xor that of b.
// Moving it is a product by x^(8 len), taken by squaring.
static uint32_t crc32Combine(uint32_t crcA, uint32_t crcB, size_t len) {
  uint32_t op = 1u << 31;               // x^0
  uint32_t sq = 1u << 23;               // x^8: one byte
  for (; len; len >>= 1) {
    if (len & 1) op = multModP(sq, op);
    sq = multModP(sq, sq);
  }
  return multModP(op, crcA) ^ crcB;
}

void installGzipTail(uint8_t *out, const char *prelude, size_t preludeLen, uint32_t bodyCrc,
                     size_t bodyLen) {
  const uint32_t crc = crc32Combine(crc32Update(0, (const uint8_t *)prelude, preludeLen), bodyCrc, bodyLen);
  const uint32_t size = (uint32_t)(preludeLen + bodyLen);
  for (int i = 0; i < 4; i++) {
    out[i] = crc >> (8 * i) & 0xff;
    out[4 + i] = size >> (8 * i) & 0xff;
  }
}

const char *installRefusalScript(InstallRefusal why) {
  if (why == INSTALL_LOCKED)
    return "#!/bin/sh\n"
           "echo 'carousel: the panel is locked, its factory reset is unfinished: restart it, then copy "
           "the command again from the Panel page' >&2\n"
           "exit 1\n";
  return "#!/bin/sh\n"
         "echo 'carousel: this install link is not valid any more, copy the command again from the Panel "
         "page' >&2\n"
         "exit 1\n";
}
