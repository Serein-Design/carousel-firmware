// SPDX-License-Identifier: AGPL-3.0-only
// The browser routes Stage and the Wi-Fi page post to (/show, /brightness,
// /text, /wifi): who may post, how much is read, and what a value may be.
// Hardware-free, so test/test_web_forms checks every branch on the PC;
// web.cpp reads the request and sends the answers.
#pragma once

#include <Arduino.h>
#include <string.h>

// The largest form these pages send is the Wi-Fi one, about 200 bytes
// encoded; the cap leaves room for a 95-character message typed with
// accents. A body over it is refused unread.
static const size_t FORM_BODY_MAX = 512;

enum FormVerdict : uint8_t { FORM_ACCEPT, FORM_FORBIDDEN, FORM_SIGN_IN, FORM_BAD_LENGTH,
                             FORM_TOO_LARGE, FORM_LOCKED };

// What is known before the first body byte. cookieOk: the session cookie;
// the API token is never asked, so a Bearer-only request reads as signed out.
// announced: Content-Length as the library holds it, signed: the header's
// text read as a number, so "-1" is -1, and no header is 0.
struct FormInput { bool hostOk, crossSite, cookieOk, locked; int32_t announced; };

// The visitor first (host or site: 403; no session: the sign-in page), then
// the size (a negative one: 400; over FORM_BODY_MAX: 413), then the lock
// (the locked page). Accepted: 0 <= announced <= FORM_BODY_MAX.
FormVerdict formGate(const FormInput &in);

// Zeroes n bytes that held a secret (a password, a key) before the memory is
// freed or goes out of scope. memset there is a dead store GCC may drop: the
// stores go through a volatile pointer, which it must keep.
void wipeSecret(void *p, size_t n);

class FormBody {                                           // one urlencoded body, at most FORM_BODY_MAX bytes
 public:
  // false, and empty, over FORM_BODY_MAX. A NUL byte ends the body there.
  bool set(const char *data, size_t n);
  // The first value posted under name, decoded ('+', %XX; a malformed % is
  // kept as typed, and so is %00, so no value holds a NUL). Returns the
  // decoded length, even when out (size - 1 bytes and a NUL) holds less;
  // -1: absent. A name posted without '=' has an empty value.
  int  value(const char *name, char *out, size_t size) const;
  bool has(const char *name) const;
  // A body can hold a password: wiped before its memory is given back
  ~FormBody() { wipeSecret(data_, sizeof(data_)); }
 private:
  char data_[FORM_BODY_MAX + 1];
  size_t n_ = 0;
};

// A brightness step as the page posts it: "0" .. levels - 1, digits only,
// no sign, no leading zero, nothing around it
bool formLevel(const char *text, int levels, int *level);
// The message as the Running order field stores it: the French letters kept,
// the others folded to what the panel's font draws, and " * " added before it loops, unless it ends
// with a space. size: the stored field's.
void panelMessage(const char *typed, char *out, size_t size);

// The Wi-Fi form's bounds, on the decoded lengths value() returns (-1:
// absent, read as empty): a name of 1 to 32 bytes, a password of none or 8
// to 63
enum WifiValues : uint8_t { WIFI_VALUES_OK, WIFI_BAD_SSID, WIFI_BAD_PASSWORD };
WifiValues wifiValues(int ssidLen, int passLen);
// The saved network chosen (by its row or its name) with no password typed:
// nothing to save, the panel restarts and retries it with the password it
// stores. False with nothing saved.
bool wifiRetrySaved(const char *ssid, int passLen, const char *saved);
