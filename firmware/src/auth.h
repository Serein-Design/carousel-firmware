// SPDX-License-Identifier: AGPL-3.0-only
// Who may change the panel: whoever can read it. The settings pages ask for
// a 6-digit code that the panel shows while someone is asking; a browser that
// typed it right gets a cookie and is not asked again.
//
// The cookie holds the device secret, made once per panel (and again after a
// factory reset or "sign out everywhere"): every signed-in browser shares it.
// SameSite=Strict keeps other sites' forms from riding on it (CSRF).
#pragma once

#include <Arduino.h>

#define PAIRING_MS       120000UL   // a code is valid 2 min
#define PAIRING_ATTEMPTS 5          // wrong codes before a new one is needed
#define PAIRING_COOLDOWN_MS 30000UL // and no new code before this: 5 guesses per
                                    // 30 s make 1 in 900 000 a 2-month job

void authBegin();                       // loads the device secret, or makes it

// Cookie header of a request -> signed in?
bool authCookieValid(const String &cookieHeader);
// "Authorization: Bearer <token>" header of an API call -> allowed?
bool authBearerValid(const String &authorizationHeader);
// The API token, shown to a signed-in user. Its own secret: it calls the API
// but cannot sign a browser in, so it cannot change Wi-Fi or other settings.
const char *authToken();
// Set-Cookie value that signs this browser in
String authSetCookie();
// New secrets: every browser is signed out and the API token changes. The
// hook token stays, so the computers set up keep driving CODE.
void authSignOutEverywhere();

// The hook token: a third secret, sent by the agents' hooks in X-Hook-Token.
// It changes what CODE shows and opens nothing else; the cookie and the API
// token are not hook tokens.
const char *authHookToken();                 // 32 hex characters
bool authRenewHookToken();                   // new token, stored; the old one is refused at once.
                                             // false: NVS refused it, the old one is kept
// "Accept hooks without a token" (absent = off): then every hook call goes on
bool authHookOpen();
void authSetHookOpen(bool open);             // stored at once
// value is the hook token? In constant time, whatever the setting (the
// install link's k)
bool authHookTokenIs(const char *value, size_t len);
// GET /id?c=<nonce>: the panel proves it holds the hook token, so the hook
// sends it nowhere else. nonce: 16 to 32 hex characters; ip: the panel's own
// IPv4 address as the request reached it. out: the first 32 hex characters
// of HMAC-SHA256 keyed with the token's 32 characters over nonce ":" ip. The
// hook picks a fresh nonce each time, so the answer is never a stable value,
// and never the token; and checks it with the address it asked, so a relay
// at another address that passes the nonce on gets a proof that fails.
// False and out "" on a bad nonce or ip, or beyond 4 proofs a second.
bool authHookProof(const char *nonce, const char *ip, char out[33]);
// May this hook call go on? header: the X-Hook-Token value, "" when absent.
// A no counts one refusal and is logged by kind, at most once every 10 s.
bool authHookAllowed(const String &header);
uint32_t authHookRefusals();                 // since start, RAM only
// GET /install with a wrong k: logged on serial, the first one, then at most
// once every 10 s, so a script retrying an old link cannot fill the log
void authLogInstallRefused();

// Someone opened the sign-in page: show a code on the panel (the current one
// if still valid, none during the cool-down). Called from the web task.
void pairingRequest();
// Code typed on the page. Right: true, and the code is used up. Wrong: false;
// after PAIRING_ATTEMPTS wrong codes the code is dropped.
bool pairingCheck(const String &typed);
// Code the panel should show now, 0 if none (read by the display loop)
uint32_t pairingCodeShown();
// Share of the validity left, 0..1, for the bar under the code
float pairingTimeLeft();
