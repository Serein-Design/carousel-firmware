// SPDX-License-Identifier: AGPL-3.0-only
// Minimal stand-ins for the Arduino / FreeRTOS environment: just what the
// host-testable modules (agent_bridge, json_lite) use. They let that code
// build unchanged on the PC and have its logic checked without the board.
#pragma once

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <deque>
#include <string>
#include <vector>

using std::min;
using std::max;

// --- Time: the test moves it forward ----------------------------------------
inline uint32_t fakeClock = 100000;
inline uint32_t millis() { return fakeClock; }
inline uint32_t esp_random() { return (uint32_t)rand() * 2654435761u; }

// The local time getLocalTime() answers: none until a test sets it
inline bool fakeTimeSet = false;
inline struct tm fakeLocalTime = {};
inline bool getLocalTime(struct tm *t, uint32_t = 5000) {
  if (!fakeTimeSet) return false;
  *t = fakeLocalTime;
  return true;
}

// As Arduino's: the value held between two bounds
#ifndef constrain
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
inline long random(long howBig) { return howBig > 0 ? (long)(esp_random() % (uint32_t)howBig) : 0; }
inline long random(long lo, long hi) { return hi > lo ? lo + random(hi - lo) : lo; }

// --- Arduino String, over std::string ---------------------------------------
// Flash strings are plain strings on the PC
#define F(x) (x)
#define PROGMEM

class String {
  std::string s;
public:
  String() {}
  String(const char *c) : s(c ? c : "") {}
  explicit String(const std::string &x) : s(x) {}
  explicit String(unsigned char v) : s(std::to_string(v)) {}
  explicit String(int v) : s(std::to_string(v)) {}
  explicit String(unsigned int v) : s(std::to_string(v)) {}
  explicit String(long v) : s(std::to_string(v)) {}
  explicit String(unsigned long v) : s(std::to_string(v)) {}
  // As Arduino's: that many decimals
  String(float v, unsigned int decimals) {
    char b[64];
    snprintf(b, sizeof(b), "%.*f", (int)decimals, (double)v);
    s = b;
  }

  const char *c_str() const { return s.c_str(); }
  bool reserve(unsigned int n) { s.reserve(n); return true; }
  unsigned int length() const { return (unsigned int)s.size(); }
  long toInt() const { return atol(s.c_str()); }
  float toFloat() const { return (float)atof(s.c_str()); }
  bool startsWith(const char *p) const { return s.rfind(p, 0) == 0; }
  char operator[](unsigned int i) const { return i < s.size() ? s[i] : '\0'; }

  int indexOf(char c, unsigned int from = 0) const {
    const size_t p = s.find(c, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(const char *str, unsigned int from = 0) const {
    const size_t p = s.find(str, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  String substring(unsigned int a) const {
    return a >= s.size() ? String() : String(s.substr(a));
  }
  String substring(unsigned int a, unsigned int b) const {
    if (a > b) std::swap(a, b);
    if (a >= s.size()) return String();
    return String(s.substr(a, std::min<size_t>(b, s.size()) - a));
  }

  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *o) { s += o; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  friend String operator+(const String &a, const String &b) { return String(a.s + b.s); }
  friend String operator+(const String &a, const char *b) { return String(a.s + b); }
  bool operator==(const char *o) const { return s == o; }
};

// --- The chip: the memory it reports, set by a test ----------------------------
struct FakeEsp {
  uint32_t freeHeap = 0, heapSize = 0;
  uint32_t getFreeHeap() const { return freeHeap; }
  uint32_t getHeapSize() const { return heapSize; }
};
inline FakeEsp ESP;

// --- Serial: silent unless a test wants to read the traces -------------------
// last: the latest printf, kept either way, for a test that checks what a
// line leaves out
struct FakeSerial {
  bool verbose = false;
  char last[256] = "";
  void printf(const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vsnprintf(last, sizeof(last), fmt, a);
    va_end(a);
    if (verbose) fputs(last, stdout);
  }
  void println(const char *t) { if (verbose) puts(t); }
};
inline FakeSerial Serial;

// --- FreeRTOS queue ----------------------------------------------------------
struct FakeQueue {
  size_t itemSize, capacity;
  std::deque<std::vector<uint8_t>> items;
};
typedef FakeQueue *QueueHandle_t;
#define pdTRUE 1

inline QueueHandle_t xQueueCreate(size_t capacity, size_t itemSize) {
  return new FakeQueue{itemSize, capacity, {}};
}
inline int xQueueSend(QueueHandle_t q, const void *item, int) {
  if (q->items.size() >= q->capacity) return 0;
  const uint8_t *b = (const uint8_t *)item;
  q->items.emplace_back(b, b + q->itemSize);
  return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q, void *item, int) {
  if (q->items.empty()) return 0;
  memcpy(item, q->items.front().data(), q->itemSize);
  q->items.pop_front();
  return pdTRUE;
}
