// SPDX-License-Identifier: AGPL-3.0-only
// GIF87a / GIF89a decoding for the IMAGE screen. Every byte comes
// from an upload, so nothing in the file is trusted: each index into a table
// is bounded here by construction and checked, and any doubt stops the image
// (fail-closed) instead of guessing. A frame is decoded into pixels_ first
// and reaches the canvas only once complete, so a damaged frame leaves the
// previous one on screen. The file is streamed one sub-block (<= 255 bytes)
// at a time; nothing is allocated, no local array exceeds 64 bytes (the loop
// task's stack), and every loop either reads a byte or ends.
#include "gif_decoder.h"

#include <string.h>

#include "content_logic.h"

// On the panel the decoder lives in one heap block reserved while IMAGE is
// drawn; this keeps it within the margin measured on the panel.
static_assert(sizeof(GifDecoder) <= 24576, "GifDecoder outgrew its 24 KB working set");

// --- LZW -----------------------------------------------------------------------

bool GifLzw::begin(int minCodeSize) {
  // Below 2 the codes cannot hold clear and end; above 8 the literals would
  // not fit a 256-entry palette
  if (minCodeSize < 2 || minCodeSize > 8) return false;
  min_ = (uint8_t)minCodeSize;
  reset();
  return true;
}

void GifLzw::reset() {
  clear_ = (uint16_t)(1u << min_);
  size_ = clear_ + 2;
  width_ = min_ + 1;
  prev_ = -1;
}

int GifLzw::width() const { return width_; }
int GifLzw::size() const { return size_; }

LzwResult GifLzw::push(uint16_t code, uint8_t *out, uint32_t *n, uint32_t cap) {
  if (code == clear_) { reset(); return LZW_OK; }
  if (code == clear_ + 1) return LZW_END;
  // Only defined codes, plus the one being defined (it repeats the previous
  // string), and that one only while the table has room: nothing is ever
  // read or written at or past index 4 095
  if (code > size_) return LZW_BAD;
  const bool defining = code == size_;
  if (defining && (prev_ < 0 || size_ >= LZW_MAX_CODES)) return LZW_BAD;
  if (prev_ < 0 && code > clear_) return LZW_BAD;   // the first code after a clear is a literal

  // The string comes out of the prefix chain last byte first. Prefixes always
  // point to a lower code, so the chain ends; the depth check keeps it inside
  // stack_ whatever the table holds.
  int depth = 0;
  uint16_t c = code;
  if (defining) { stack_[depth++] = first_; c = (uint16_t)prev_; }
  while (c > clear_ + 1) {
    if (depth >= LZW_MAX_CODES) return LZW_BAD;
    stack_[depth++] = suffix_[c];
    c = prefix_[c];
  }
  if (c >= clear_ || depth >= LZW_MAX_CODES) return LZW_BAD;
  stack_[depth++] = (uint8_t)c;
  const uint8_t first = (uint8_t)c;

  if (prev_ >= 0 && size_ < LZW_MAX_CODES) {
    prefix_[size_] = (uint16_t)prev_;
    suffix_[size_] = first;
    size_++;
    // GIF widens once the new size needs one more bit (no early change);
    // 12 bits at most, then codes are used without adding entries
    if (size_ == (1u << width_) && width_ < 12) width_++;
  }
  prev_ = (int16_t)code;
  first_ = first;
  // Bytes past cap (more than the frame's w*h) are dropped
  while (depth > 0 && *n < cap) out[(*n)++] = stack_[--depth];
  return LZW_OK;
}

// --- Interlace -----------------------------------------------------------------

int gifInterlacedRow(int r, int h) {
  if (r < 0 || r >= h) return -1;
  // Four passes: every 8th row from 0, every 8th from 4, every 4th from 2,
  // every 2nd from 1
  const int pass1 = (h + 7) / 8, pass2 = (h + 3) / 8, pass3 = (h + 1) / 4;
  if (r < pass1) return r * 8;
  r -= pass1;
  if (r < pass2) return 4 + r * 8;
  r -= pass2;
  if (r < pass3) return 2 + r * 4;
  return 1 + (r - pass3) * 2;
}

const char *gifErrorName(GifError e) {
  static const char *const names[] = {"none", "read", "truncated", "signature", "size", "block",
                                      "palette", "frame-rect", "code-size", "lzw", "no-frame"};
  return (size_t)e < sizeof names / sizeof names[0] ? names[e] : "unknown";
}

// --- Decoder -------------------------------------------------------------------

int GifDecoder::width() const { return width_; }
int GifDecoder::height() const { return height_; }
int GifDecoder::frames() const { return frames_; }
GifError GifDecoder::error() const { return error_; }
const uint16_t *GifDecoder::canvas() const { return canvas_; }

GifStatus GifDecoder::fail(GifError e) {
  status_ = GIF_ERROR;
  error_ = e;
  return GIF_ERROR;
}

bool GifDecoder::readAll(uint8_t *buf, int32_t len) {
  if (len <= 0) return true;
  const int32_t got = reader_->read(buf, len);
  if (got < 0 || got > len) { fail(GE_READ); return false; }
  // A short read is the end of the file: the GIF stops before its trailer
  if (got < len) { fail(GE_TRUNCATED); return false; }
  return true;
}

bool GifDecoder::readPalette(uint16_t *palette, int entries) {
  // Always 256 entries, unused ones black, so any index 0..255 is defined
  memset(palette, 0, 256 * sizeof(uint16_t));
  // 85 entries (255 bytes) at a time through block_
  for (int i = 0; i < entries; i += 85) {
    const int k = entries - i < 85 ? entries - i : 85;
    if (!readAll(block_, 3 * k)) return false;
    for (int j = 0; j < k; j++) {
      const uint8_t *rgb = block_ + 3 * j;
      palette[i + j] = (uint16_t)((rgb[0] & 0xF8) << 8 | (rgb[1] & 0xFC) << 3 | rgb[2] >> 3);
    }
  }
  return true;
}

bool GifDecoder::skipSubBlocks() {
  for (;;) {
    uint8_t len;
    if (!readAll(&len, 1)) return false;
    if (len == 0) return true;
    if (!readAll(block_, len)) return false;
  }
}

bool GifDecoder::nextByte(uint8_t *b) {
  if (blockPos_ == blockLen_) {
    uint8_t len;
    if (!readAll(&len, 1)) return false;
    // The data ended before the frame's w*h pixels
    if (len == 0) { fail(GE_LZW); return false; }
    if (!readAll(block_, len)) return false;
    blockLen_ = len;
    blockPos_ = 0;
  }
  *b = block_[blockPos_++];
  return true;
}

void GifDecoder::restart() {
  memset(canvas_, 0, sizeof canvas_);
  frames_ = 0;
  clearPending_ = false;
  gcFlags_ = gcTransparent_ = 0;
  gcDelay_ = 0;
  status_ = GIF_FRAME;
  error_ = GE_NONE;
}

bool GifDecoder::open(GifReader &r) {
  reader_ = &r;
  firstFrame_ = 0;   // 0: not open, rewind refuses
  width_ = height_ = 0;
  hasGlobal_ = false;
  memset(global_, 0, sizeof global_);
  restart();

  uint8_t head[13];
  if (!readAll(head, 6)) return false;
  if (memcmp(head, "GIF87a", 6) != 0 && memcmp(head, "GIF89a", 6) != 0) {
    fail(GE_SIGNATURE);
    return false;
  }
  if (!readAll(head + 6, 7)) return false;
  const uint16_t w = head[6] | head[7] << 8, h = head[8] | head[9] << 8;
  // The canvas holds 64x32 pixels; uploads of another size are refused
  if (!((w == 64 && h == 32) || (w == 32 && h == 64))) {
    fail(GE_SIZE);
    return false;
  }
  width_ = w;
  height_ = h;
  int32_t offset = 13;
  if (head[10] & 0x80) {
    const int entries = 2 << (head[10] & 7);
    if (!readPalette(global_, entries)) return false;
    hasGlobal_ = true;
    offset += 3 * entries;
  }
  firstFrame_ = offset;
  return true;
}

bool GifDecoder::rewind() {
  if (!reader_ || firstFrame_ == 0) return false;
  if (!reader_->seek(firstFrame_)) {
    fail(GE_READ);
    return false;
  }
  restart();
  return true;
}

GifStatus GifDecoder::next(uint32_t *delayMs) {
  if (status_ != GIF_FRAME) return status_;
  for (;;) {
    uint8_t intro;
    if (!readAll(&intro, 1)) return status_;
    if (intro == 0x21) {
      uint8_t label;
      if (!readAll(&label, 1)) return status_;
      if (label == 0xF9) {
        uint8_t size;
        if (!readAll(&size, 1)) return status_;
        if (size != 4) return fail(GE_BLOCK);
        if (!readAll(block_, 4)) return status_;
        gcFlags_ = block_[0];
        gcDelay_ = block_[1] | block_[2] << 8;
        gcTransparent_ = block_[3];
      }
      // Comments, applications (the loop count is ignored), plain text
      // and unknown labels are skipped, never drawn
      if (!skipSubBlocks()) return status_;
    } else if (intro == 0x2C) {
      if (frame() != GIF_FRAME) return status_;
      frames_++;
      if (delayMs) *delayMs = (uint32_t)gcDelay_ * 10;
      // A graphic control block applies to the next frame only
      gcFlags_ = gcTransparent_ = 0;
      gcDelay_ = 0;
      return GIF_FRAME;
    } else if (intro == 0x3B) {
      // A trailer before any frame would make every rewind loop on nothing
      if (frames_ == 0) return fail(GE_NO_FRAME);
      status_ = GIF_END;
      return GIF_END;
    } else {
      return fail(GE_BLOCK);
    }
  }
}

GifStatus GifDecoder::frame() {
  uint8_t d[9];
  if (!readAll(d, 9)) return status_;
  const uint32_t x = d[0] | d[1] << 8, y = d[2] | d[3] << 8;
  const uint32_t w = d[4] | d[5] << 8, h = d[6] | d[7] << 8;
  const uint8_t flags = d[8];
  // The rectangle must fit the logical screen, checked in 32 bits before any
  // product: then w*h <= 2 048 fits pixels_ and every row fits the canvas
  if (w == 0 || h == 0 || x + w > width_ || y + h > height_) return fail(GE_FRAME_RECT);

  const uint16_t *palette;
  if (flags & 0x80) {
    if (!readPalette(local_, 2 << (flags & 7))) return status_;
    palette = local_;
  } else if (hasGlobal_) {
    palette = global_;
  } else {
    return fail(GE_PALETTE);
  }

  uint8_t minCode;
  if (!readAll(&minCode, 1)) return status_;
  if (!lzw_.begin(minCode)) return fail(GE_CODE_SIZE);

  // Codes are packed LSB first across sub-blocks
  blockLen_ = blockPos_ = 0;
  const uint32_t total = w * h;
  uint32_t n = 0, bits = 0;
  int count = 0;
  while (n < total) {
    while (count < lzw_.width()) {
      uint8_t b;
      if (!nextByte(&b)) return status_;
      bits |= (uint32_t)b << count;
      count += 8;
    }
    const uint16_t code = bits & ((1u << lzw_.width()) - 1);
    bits >>= lzw_.width();
    count -= lzw_.width();
    // An end code before w*h pixels or any invalid code stops the image
    if (lzw_.push(code, pixels_, &n, total) != LZW_OK) return fail(GE_LZW);
  }
  // Some encoders write data past the last pixel: skipped to the terminator
  if (!skipSubBlocks()) return status_;

  // Complete: the previous frame's disposal, then this one onto the canvas
  if (clearPending_) canvasClear(canvas_, width_, height_, clearX_, clearY_, clearW_, clearH_);
  clearPending_ = false;
  const int transparent = (gcFlags_ & 1) ? gcTransparent_ : -1;
  const bool interlaced = flags & 0x40;
  for (uint32_t r = 0; r < h; r++) {
    const int row = interlaced ? gifInterlacedRow((int)r, (int)h) : (int)r;
    canvasLine(canvas_, width_, height_, (int)x, (int)y + row, (int)w, pixels_ + r * w, palette,
               transparent);
  }
  // Disposal 2 clears to black; 3 is kept as 1, the rest as 1
  if (((gcFlags_ >> 2) & 7) == 2) {
    clearPending_ = true;
    clearX_ = (uint8_t)x; clearY_ = (uint8_t)y; clearW_ = (uint8_t)w; clearH_ = (uint8_t)h;
  }
  return GIF_FRAME;
}
