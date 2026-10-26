// SPDX-License-Identifier: AGPL-3.0-only
// Host stand-in for the ESP32 ROM's tinfl_decompress, the raw deflate
// (RFC 1951) inflater places_logic uses: one call with the whole input and
// an output buffer the whole result fits in, as the firmware makes it. A
// small decoder of its own (stored, fixed and dynamic Huffman blocks), so
// the tests read the real data/places.bin.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

typedef unsigned char mz_uint8;
typedef uint32_t mz_uint32;

enum {
  TINFL_FLAG_PARSE_ZLIB_HEADER = 1,
  TINFL_FLAG_HAS_MORE_INPUT = 2,
  TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
};

typedef enum {
  TINFL_STATUS_BAD_PARAM = -3,
  TINFL_STATUS_FAILED = -1,
  TINFL_STATUS_DONE = 0,
  TINFL_STATUS_NEEDS_MORE_INPUT = 1,
  TINFL_STATUS_HAS_MORE_OUTPUT = 2,
} tinfl_status;

struct tinfl_decompressor { uint32_t m_state; };
#define tinfl_init(r) ((r)->m_state = 0)

namespace stub_inflate {

struct State {
  const uint8_t *in;
  size_t inLen, pos = 0;
  uint32_t buf = 0;
  int count = 0;
  uint8_t *out;
  size_t outLen, outPos = 0;
  bool err = false;
};

inline int bits(State &s, int need) {
  uint32_t v = s.buf;
  while (s.count < need) {
    if (s.pos >= s.inLen) { s.err = true; return 0; }
    v |= (uint32_t)s.in[s.pos++] << s.count;
    s.count += 8;
  }
  s.buf = v >> need;
  s.count -= need;
  return (int)(v & ((1u << need) - 1));
}

struct Huff { short count[16]; short symbol[288]; };

inline int decode(State &s, const Huff &h) {
  int code = 0, first = 0, index = 0;
  for (int len = 1; len < 16; len++) {
    code |= bits(s, 1);
    if (s.err) return -1;
    const int n = h.count[len];
    if (code - n < first) return h.symbol[index + (code - first)];
    index += n;
    first = (first + n) << 1;
    code <<= 1;
  }
  return -1;
}

inline bool build(Huff &h, const short *length, int n) {
  memset(h.count, 0, sizeof(h.count));
  for (int i = 0; i < n; i++) h.count[length[i]]++;
  if (h.count[0] == n) return true;
  int left = 1;
  for (int len = 1; len < 16; len++) {
    left = (left << 1) - h.count[len];
    if (left < 0) return false;
  }
  short offs[16];
  offs[1] = 0;
  for (int len = 1; len < 15; len++) offs[len + 1] = offs[len] + h.count[len];
  for (int i = 0; i < n; i++)
    if (length[i]) h.symbol[offs[length[i]]++] = (short)i;
  return true;
}

inline bool codes(State &s, const Huff &lc, const Huff &dc) {
  static const short LBASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
                                  31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
  static const short LEXT[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  static const short DBASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                  193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                  6145, 8193, 12289, 16385, 24577};
  static const short DEXT[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
  for (;;) {
    int sym = decode(s, lc);
    if (sym < 0) return false;
    if (sym < 256) {
      if (s.outPos >= s.outLen) return false;
      s.out[s.outPos++] = (uint8_t)sym;
      continue;
    }
    if (sym == 256) return true;
    sym -= 257;
    if (sym >= 29) return false;
    int len = LBASE[sym] + bits(s, LEXT[sym]);
    const int ds = decode(s, dc);
    if (ds < 0 || ds >= 30) return false;
    const size_t dist = (size_t)DBASE[ds] + bits(s, DEXT[ds]);
    if (s.err || dist > s.outPos || s.outPos + len > s.outLen) return false;
    while (len--) { s.out[s.outPos] = s.out[s.outPos - dist]; s.outPos++; }
  }
}

inline bool stored(State &s) {
  s.buf = 0;
  s.count = 0;
  if (s.pos + 4 > s.inLen) return false;
  const unsigned len = s.in[s.pos] | s.in[s.pos + 1] << 8;
  const unsigned inv = s.in[s.pos + 2] | s.in[s.pos + 3] << 8;
  s.pos += 4;
  if ((len ^ 0xFFFF) != inv || s.pos + len > s.inLen || s.outPos + len > s.outLen) return false;
  memcpy(s.out + s.outPos, s.in + s.pos, len);
  s.pos += len;
  s.outPos += len;
  return true;
}

inline bool fixed(State &s) {
  short l[288 + 30];
  for (int i = 0; i < 288; i++) l[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
  for (int i = 0; i < 30; i++) l[288 + i] = 5;
  Huff lc, dc;
  return build(lc, l, 288) && build(dc, l + 288, 30) && codes(s, lc, dc);
}

inline bool dynamic(State &s) {
  static const uint8_t ORDER[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
  const int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
  if (s.err || nlen > 286 || ndist > 30) return false;
  short l[320] = {0};
  for (int i = 0; i < ncode; i++) l[ORDER[i]] = (short)bits(s, 3);
  Huff lc, dc;
  if (s.err || !build(lc, l, 19)) return false;
  int i = 0;
  while (i < nlen + ndist) {
    int sym = decode(s, lc);
    if (sym < 0) return false;
    if (sym < 16) { l[i++] = (short)sym; continue; }
    short v = 0;
    int rep;
    if (sym == 16) {
      if (!i) return false;
      v = l[i - 1];
      rep = 3 + bits(s, 2);
    } else {
      rep = sym == 17 ? 3 + bits(s, 3) : 11 + bits(s, 7);
    }
    if (s.err || i + rep > nlen + ndist) return false;
    while (rep--) l[i++] = v;
  }
  return build(lc, l, nlen) && build(dc, l + nlen, ndist) && codes(s, lc, dc);
}

}  // namespace stub_inflate

inline tinfl_status tinfl_decompress(tinfl_decompressor *, const mz_uint8 *in, size_t *inSize,
                                     mz_uint8 *outStart, mz_uint8 *outNext, size_t *outSize,
                                     const mz_uint32 flags) {
  // Only the firmware's use: everything at once, raw deflate
  if (flags != TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF || outStart != outNext)
    return TINFL_STATUS_BAD_PARAM;
  stub_inflate::State s;
  s.in = in;
  s.inLen = *inSize;
  s.out = outStart;
  s.outLen = *outSize;
  int last;
  do {
    last = stub_inflate::bits(s, 1);
    const int type = stub_inflate::bits(s, 2);
    const bool ok = !s.err && (type == 0   ? stub_inflate::stored(s)
                               : type == 1 ? stub_inflate::fixed(s)
                               : type == 2 ? stub_inflate::dynamic(s)
                                           : false);
    if (!ok || s.err) return TINFL_STATUS_FAILED;
  } while (!last);
  *inSize = s.pos;
  *outSize = s.outPos;
  return TINFL_STATUS_DONE;
}
