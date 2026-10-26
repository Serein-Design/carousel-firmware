// SPDX-License-Identifier: AGPL-3.0-only
// NVS stand-in: an in-memory table, a write counter, and three failures to
// inject: a closed NVS (begin refused), every write refused, and the writes
// to one key refused (a full NVS that has no room for a longer value).
#pragma once

#include <map>
#include <string>

#include "Arduino.h"

inline std::map<std::string, std::string> fakeNvs;
inline int nvsWriteCount = 0;
inline bool nvsFailBegin = false, nvsFailWrites = false;
inline std::string nvsFailKey;

// Like the real one, a refused write leaves the stored value as it was.
inline bool nvsRefused(const char *key) { return nvsFailWrites || nvsFailKey == key; }

class Preferences {
public:
  bool begin(const char *, bool = false) { return !nvsFailBegin; }
  void end() {}
  // The real putString() returns the length written: 0 for an empty value,
  // stored or not.
  size_t putString(const char *key, const String &v) {
    if (nvsRefused(key)) return 0;
    fakeNvs[key] = v.c_str();
    nvsWriteCount++;
    return v.length();
  }
  String getString(const char *key, const String &fallback = String()) {
    const auto it = fakeNvs.find(key);
    return it == fakeNvs.end() ? fallback : String(it->second.c_str());
  }
  size_t putBool(const char *key, bool v) {
    if (nvsRefused(key)) return 0;
    fakeNvs[key] = v ? "1" : "0";
    nvsWriteCount++;
    return 1;
  }
  // The real one reads only a value that fits with its NUL, and returns its
  // size NUL included: 0 when absent or too long, out then untouched.
  size_t getString(const char *key, char *out, size_t size) {
    const auto it = fakeNvs.find(key);
    if (it == fakeNvs.end() || it->second.size() + 1 > size) return 0;
    memcpy(out, it->second.c_str(), it->second.size() + 1);
    return it->second.size() + 1;
  }
  // A value of another size (another type) reads as the fallback, as a
  // type mismatch does on the real NVS
  uint16_t getUShort(const char *key, uint16_t fallback = 0) {
    const auto it = fakeNvs.find(key);
    if (it == fakeNvs.end() || it->second.size() != sizeof(uint16_t)) return fallback;
    uint16_t v;
    memcpy(&v, it->second.data(), sizeof(v));
    return v;
  }
  uint8_t getUChar(const char *key, uint8_t fallback = 0) {
    const auto it = fakeNvs.find(key);
    return it == fakeNvs.end() || it->second.size() != sizeof(uint8_t) ? fallback
                                                                       : (uint8_t)it->second[0];
  }
  bool getBool(const char *key, bool fallback = false) {
    const auto it = fakeNvs.find(key);
    return it == fakeNvs.end() ? fallback : it->second == "1";
  }
  // Numbers: the raw bytes, and their size back, as the real put*() do
  size_t putUChar(const char *key, uint8_t v)   { return putRaw(key, v); }
  size_t putUShort(const char *key, uint16_t v) { return putRaw(key, v); }
  size_t putInt(const char *key, int32_t v)     { return putRaw(key, v); }
  size_t putULong(const char *key, uint32_t v)  { return putRaw(key, v); }
  size_t putFloat(const char *key, float v)     { return putRaw(key, v); }
  // Blobs: the real getBytes() returns 0 when the value does not fit maxLen
  size_t getBytesLength(const char *key) {
    const auto it = fakeNvs.find(key);
    return it == fakeNvs.end() ? 0 : it->second.size();
  }
  size_t getBytes(const char *key, void *out, size_t maxLen) {
    const auto it = fakeNvs.find(key);
    if (it == fakeNvs.end() || it->second.size() > maxLen) return 0;
    memcpy(out, it->second.data(), it->second.size());
    return it->second.size();
  }
  size_t putBytes(const char *key, const void *v, size_t len) {
    if (nvsRefused(key)) return 0;
    fakeNvs[key] = std::string((const char *)v, len);
    nvsWriteCount++;
    return len;
  }
  bool remove(const char *key) {
    if (nvsRefused(key)) return false;
    nvsWriteCount++;
    return fakeNvs.erase(key) > 0;
  }
  bool isKey(const char *key) { return fakeNvs.count(key) > 0; }

private:
  template <typename T> size_t putRaw(const char *key, T v) {
    if (nvsRefused(key)) return 0;
    fakeNvs[key] = std::string((const char *)&v, sizeof(v));
    nvsWriteCount++;
    return sizeof(v);
  }
};

// A number stored by put*(), read back by the tests
template <typename T> T fakeNvsNumber(const char *key) {
  T v{};
  const auto it = fakeNvs.find(key);
  if (it != fakeNvs.end() && it->second.size() == sizeof(v)) memcpy(&v, it->second.data(), sizeof(v));
  return v;
}
