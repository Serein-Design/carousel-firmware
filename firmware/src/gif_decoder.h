// SPDX-License-Identifier: AGPL-3.0-only
// The GIF decoder of the IMAGE screen. Uploaded GIFs are untrusted, so the
// panel decodes them with code whose every bound is ours and tested on the PC
// (test/test_gif, under AddressSanitizer, with truncated and mutated files).
// Hardware-free: the file comes through a GifReader, frames are composed on a
// canvas inside the object, nothing is allocated and nothing is logged.
#pragma once

#include <stdint.h>

static const int GIF_MAX_PIXELS = 64 * 32;    // logical screen 64x32 or 32x64 only
static const int LZW_MAX_CODES = 4096;        // 12-bit codes, GIF89a

// The file: the content area on the panel, memory in the tests. The decoder
// reads from the reader's current position at open (the start of the file).
struct GifReader {
  virtual int32_t read(uint8_t *buf, int32_t len) = 0;   // bytes read; 0 = end of file; < 0 = error
  virtual bool seek(int32_t pos) = 0;                     // from the start of the file
  virtual ~GifReader() {}
};

enum LzwResult : uint8_t { LZW_OK, LZW_END, LZW_BAD };

// One frame's code table; public for its tests.
class GifLzw {
public:
  bool begin(int minCodeSize);                // 2..8, else false and nothing changed
  // Appends the code's string at out[*n], never at or past cap (the rest is
  // dropped); the clear code resets the table; LZW_END on the end code;
  // LZW_BAD on any invalid code
  LzwResult push(uint16_t code, uint8_t *out, uint32_t *n, uint32_t cap);
  int width() const;                          // bits of the next code, 3..12
  int size() const;                           // next free code, at most LZW_MAX_CODES
private:
  void reset();
  uint16_t prefix_[LZW_MAX_CODES];
  uint8_t suffix_[LZW_MAX_CODES], stack_[LZW_MAX_CODES];
  uint16_t clear_ = 4, size_ = 6;
  int16_t prev_ = -1;                         // previous code, -1 after a clear
  uint8_t first_ = 0;                         // first byte of the previous code's string
  uint8_t width_ = 3, min_ = 2;
};

int gifInterlacedRow(int r, int h);           // stored row r of an interlaced frame -> its row, -1 if r is out of 0..h-1

enum GifStatus : int8_t { GIF_ERROR = -1, GIF_END = 0, GIF_FRAME = 1 };
enum GifError : uint8_t { GE_NONE, GE_READ, GE_TRUNCATED, GE_SIGNATURE, GE_SIZE, GE_BLOCK,
                          GE_PALETTE, GE_FRAME_RECT, GE_CODE_SIZE, GE_LZW, GE_NO_FRAME };
const char *gifErrorName(GifError e);         // short English word for the serial log

// About 23.9 KB, canvas inside: on the panel made with new (std::nothrow),
// never static.
class GifDecoder {
public:
  bool open(GifReader &r);                    // signature, 64x32 or 32x64 screen, global palette; canvas black
  // Next frame composed on the canvas; delay as written (cs x 10); END at the
  // trailer; END and ERROR are sticky until open or rewind
  GifStatus next(uint32_t *delayMs);
  bool rewind();                              // back to frame 1, canvas black: a new loop
  int width() const;
  int height() const;                         // the logical screen
  int frames() const;                         // frames composed since open or rewind
  GifError error() const;
  const uint16_t *canvas() const;             // width() x height() RGB565, row after row
private:
  GifStatus fail(GifError e);
  bool readAll(uint8_t *buf, int32_t len);
  bool readPalette(uint16_t *palette, int entries);
  bool skipSubBlocks();
  bool nextByte(uint8_t *b);                  // the frame's data, across sub-blocks
  GifStatus frame();
  void restart();

  GifLzw lzw_;                                // 16 384 B of tables
  uint8_t pixels_[GIF_MAX_PIXELS];            // one frame's indices, composed only when complete
  uint16_t global_[256], local_[256];         // RGB565, unused entries 0 (black)
  uint8_t block_[256];                        // one sub-block: the file is never held whole
  uint16_t canvas_[GIF_MAX_PIXELS];
  GifReader *reader_ = nullptr;
  int32_t firstFrame_ = 0;                    // file offset of the first block after the palette
  uint16_t width_ = 0, height_ = 0;
  bool hasGlobal_ = false;
  // Disposal 2 of the frame composed last, applied before the next one
  bool clearPending_ = false;
  uint8_t clearX_ = 0, clearY_ = 0, clearW_ = 0, clearH_ = 0;
  // Graphic control of the next frame
  uint8_t gcFlags_ = 0, gcTransparent_ = 0;
  uint16_t gcDelay_ = 0;
  // Bit reader over the frame's sub-blocks
  uint8_t blockLen_ = 0, blockPos_ = 0;
  int frames_ = 0;
  GifStatus status_ = GIF_ERROR;              // GIF_FRAME: playing; END and ERROR sticky
  GifError error_ = GE_NONE;
};
