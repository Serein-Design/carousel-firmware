// SPDX-License-Identifier: AGPL-3.0-only
// The browser routes Stage and the Wi-Fi page post to (/show, /brightness,
// /text, /wifi): who may post, how much is read, and what a value may be;
// and how much the library may read of every other POST.
// Hardware-free, so test/test_web_forms checks every branch on the PC;
// web.cpp reads the request and sends the answers.
#pragma once

#include <Arduino.h>
#include <string.h>

// The TEXT message as typed: 95 characters (the fields' maxlength), each at
// most 3 bytes of UTF-8 among those the font takes (U+2019, U+2026...; a
// character past U+FFFF is 4 bytes but counts as 2 in maxlength)
static const size_t MESSAGE_CHARS = 95, MESSAGE_TYPED_MAX = MESSAGE_CHARS * 3;   // 285 bytes

// The largest form these pages send is Send's: "msg=" and the message, each
// byte %XX, 4 + 285 * 3 = 859 bytes (the Wi-Fi form, every field at its
// bound, is about 900 counted the same way). A body over it is refused unread.
static const size_t FORM_BODY_MAX = 1024;

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

// Every other POST (the API, sign-in, the settings form, the buttons, an
// unknown address): the library reads the body itself, whole into RAM,
// before any check, so a body over its route's cap is refused unread, after
// the route's own checks of the visitor (host, site, token or session).
// Each cap is the largest body the route takes, every byte encoded, with
// margin (test_web_forms builds them). A multipart body is out of reach:
// the library parses it before any handler can look at its size.
static const size_t BODY_CAP_SMALL = 512;      // one name or number: the API's calls, sign-in, the buttons
// /api/notify: 159 bytes of text; /api/text: MESSAGE_TYPED_MAX, as a form 5 + 285 * 3 = 860
static const size_t BODY_CAP_MESSAGE = 1024;
static const size_t BODY_CAP_SETTINGS = 2048;  // /sauver: the Panel form, filled to every bound, is 1.3 KB
// BODY_READ: 0 <= announced <= cap, read by the library as before
enum BodyVerdict : uint8_t { BODY_READ, BODY_BAD_LENGTH, BODY_TOO_LARGE };
BodyVerdict bodyVerdict(int32_t announced, size_t cap);
// A route's cap, BODY_CAP_SMALL for any address without one of its own
size_t bodyCap(const char *uri);

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
