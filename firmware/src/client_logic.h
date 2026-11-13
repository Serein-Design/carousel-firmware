// SPDX-License-Identifier: AGPL-3.0-only
// When the web server drops a client that has sent nothing. WebServer serves
// one client at a time and waits up to 5 s (HTTP_MAX_DATA_WAIT, not
// overridable under -Werror) for a client's first bytes. A browser opens
// speculative sockets that send nothing: a hook queued behind one gave up at
// its 4 s (3 of 80 calls). Hardware-free: host-tested in test_client_logic;
// web.cpp's PanelServer applies it.
#pragma once

#include <stdint.h>

// A real request's first bytes arrive within a LAN round trip, far under
// 500 ms; a hook queued behind a few idle sockets still ends well under its 4 s.
static const uint32_t IDLE_CLIENT_MS = 500;

// waitingRead: the server waits for the client's request (HC_WAIT_READ);
// hasData: bytes are there to read; idleMs: since the client was accepted,
// as unsigned millis() - start, so a wraparound is harmless; anotherWaiting:
// a connection waits in the backlog. Only then is the idle client dropped:
// alone, a slow client keeps the framework's 5 s.
inline bool dropIdleClient(bool waitingRead, bool hasData, uint32_t idleMs, bool anotherWaiting) {
  return waitingRead && !hasData && idleMs > IDLE_CLIENT_MS && anotherWaiting;
}
