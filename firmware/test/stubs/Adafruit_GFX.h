// SPDX-License-Identifier: AGPL-3.0-only
// Stand-in for the Adafruit GFX base class and its 16-bit canvas: what the
// host-tested drawing code calls. A test canvas derives from Adafruit_GFX and
// gives the pixel; test_poster_cells draws the real screens on GFXcanvas16.
// Lines, rectangles and circles set the pixels Adafruit's own routines set
// (Adafruit_GFX.cpp, 1.12.6). print() draws nothing: the screens drawn on the
// PC write their words with type.h. setRotation turns the canvas as Adafruit's.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <utility>

class Adafruit_GFX {
 public:
  Adafruit_GFX(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h), _width(w), _height(h) {}
  virtual ~Adafruit_GFX() = default;
  virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;
  int16_t width() const { return _width; }
  int16_t height() const { return _height; }
  uint8_t getRotation() const { return rotation; }
  // 1 and 3 swap the sides, as Adafruit's: the pixels are mapped by the canvas
  void setRotation(uint8_t r) {
    rotation = r & 3;
    _width = rotation & 1 ? HEIGHT : WIDTH;
    _height = rotation & 1 ? WIDTH : HEIGHT;
  }

  void drawRGBBitmap(int16_t x, int16_t y, const uint16_t *pcolors, int16_t w, int16_t h) {
    for (int16_t j = 0; j < h; j++)
      for (int16_t i = 0; i < w; i++) drawPixel(x + i, y + j, pcolors[j * w + i]);
  }

  virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    drawLine(x, y, x, y + h - 1, color);
  }
  virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    drawLine(x, y, x + w - 1, y, color);
  }
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    for (int16_t i = x; i < x + w; i++) drawFastVLine(i, y, h, color);
  }
  virtual void fillScreen(uint16_t color) { fillRect(0, 0, _width, _height, color); }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
  }

  // Bresenham, as Adafruit's writeLine
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    const bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) {
      std::swap(x0, y0);
      std::swap(x1, y1);
    }
    if (x0 > x1) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    const int16_t dx = x1 - x0, dy = std::abs(y1 - y0);
    int16_t err = dx / 2;
    const int16_t ystep = y0 < y1 ? 1 : -1;
    for (; x0 <= x1; x0++) {
      if (steep) drawPixel(y0, x0, color);
      else drawPixel(x0, y0, color);
      err -= dy;
      if (err < 0) {
        y0 += ystep;
        err += dx;
      }
    }
  }

  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    drawPixel(x0, y0 + r, color);
    drawPixel(x0, y0 - r, color);
    drawPixel(x0 + r, y0, color);
    drawPixel(x0 - r, y0, color);
    while (x < y) {
      if (f >= 0) {
        y--;
        ddy += 2;
        f += ddy;
      }
      x++;
      ddx += 2;
      f += ddx;
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 - x, y0 + y, color);
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 - x, y0 - y, color);
      drawPixel(x0 + y, y0 + x, color);
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 + y, y0 - x, color);
      drawPixel(x0 - y, y0 - x, color);
    }
  }

  // As Adafruit's fillCircle and fillCircleHelper (both halves)
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    drawFastVLine(x0, y0 - r, 2 * r + 1, color);
    int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r, px = x, py = y;
    while (x < y) {
      if (f >= 0) {
        y--;
        ddy += 2;
        f += ddy;
      }
      x++;
      ddx += 2;
      f += ddx;
      if (x < y + 1) {
        drawFastVLine(x0 + x, y0 - y, 2 * y + 1, color);
        drawFastVLine(x0 - x, y0 - y, 2 * y + 1, color);
      }
      if (y != py) {
        drawFastVLine(x0 + py, y0 - px, 2 * px + 1, color);
        drawFastVLine(x0 - py, y0 - px, 2 * px + 1, color);
        py = y;
      }
      px = x;
    }
  }

  // The classic text cursor, kept but never drawn (see the header)
  void setCursor(int16_t x, int16_t y) { cursor_x = x; cursor_y = y; }
  void setTextColor(uint16_t c) { textcolor = c; }
  void setTextColor(uint16_t c, uint16_t) { textcolor = c; }
  void setTextSize(uint8_t s) { textsize = s; }
  void setTextWrap(bool w) { wrap = w; }
  size_t print(const char *) { return 0; }
  size_t print(char) { return 0; }
  size_t print(int) { return 0; }

 protected:
  const int16_t WIDTH, HEIGHT;   // the canvas as built; _width and _height follow the rotation
  int16_t _width, _height;
  uint8_t rotation = 0;
  int16_t cursor_x = 0, cursor_y = 0;
  uint16_t textcolor = 0xFFFF;
  uint8_t textsize = 1;
  bool wrap = true;
};

// A frame in memory, as Adafruit's: RGB565, row after row
class GFXcanvas16 : public Adafruit_GFX {
 public:
  GFXcanvas16(uint16_t w, uint16_t h, bool allocate = true) : Adafruit_GFX(w, h) {
    if (allocate) {
      buffer = (uint16_t *)calloc((size_t)w * h, sizeof(uint16_t));
      buffer_owned = true;
    }
  }
  ~GFXcanvas16() override {
    if (buffer_owned) free(buffer);
  }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (!buffer || x < 0 || y < 0 || x >= _width || y >= _height) return;
    toRaw(x, y);
    buffer[x + y * WIDTH] = color;
  }
  // Negative sizes count back from x or y, as the canvas's own
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    if (w < 0) {
      w = -w;
      x -= w - 1;
    }
    for (int16_t i = 0; i < w; i++) drawPixel(x + i, y, color);
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    if (h < 0) {
      h = -h;
      y -= h - 1;
    }
    for (int16_t i = 0; i < h; i++) drawPixel(x, y + i, color);
  }
  uint16_t getPixel(int16_t x, int16_t y) const {
    if (!buffer || x < 0 || y < 0 || x >= _width || y >= _height) return 0;
    toRaw(x, y);
    return buffer[x + y * WIDTH];
  }
  uint16_t *getBuffer() const { return buffer; }   // as built, unrotated

 protected:
  // A point of the rotated canvas to its place in the buffer
  void toRaw(int16_t &x, int16_t &y) const {
    int16_t t;
    switch (rotation) {
      case 1: t = x; x = WIDTH - 1 - y; y = t; break;
      case 2: x = WIDTH - 1 - x; y = HEIGHT - 1 - y; break;
      case 3: t = x; x = y; y = HEIGHT - 1 - t; break;
      default: break;
    }
  }
  uint16_t *buffer = nullptr;
  bool buffer_owned = false;
};
