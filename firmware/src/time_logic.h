// SPDX-License-Identifier: AGPL-3.0-only
// Where the panel's time comes from, before and besides NTP: the order the
// time servers are asked in at start, and the Date header of an HTTP answer.
// Hardware-free: host-tested in test_time; network.cpp uses it.
#pragma once

#include <stdint.h>
#include <time.h>

// What set the time, as the log names it: "[NTP] time received (pool)"
enum TimeSource : uint8_t { TIME_ROUTER, TIME_POOL, TIME_HTTP_DATE };
const char *timeSourceName(TimeSource s);

// The time servers: the router's own when DHCP names one, then the two public
// ones of secrets.h.
enum NtpServer : uint8_t { NTP_ROUTER, NTP_PUBLIC_1, NTP_PUBLIC_2 };

// Until the first answer, one server is asked per window, in turn: the
// router's first (on the LAN, the quickest to answer), then the public ones,
// round and round. lwIP alone stays 15 s on a server that does not answer.
NtpServer ntpServerFor(uint32_t window, bool haveRouter);
// How long window lasts before the next server is asked: 2 s for the first,
// 5 s after; 15 s, lwIP's own pace, once the time is known some other way
// (an HTTP Date) and NTP only corrects it.
uint32_t ntpWindowMs(uint32_t window, bool timeKnown);

// An HTTP Date header in the format servers send (IMF-fixdate, RFC 9110):
// "Sun, 06 Nov 1994 08:49:37 GMT". False, out untouched, on anything else.
bool parseHttpDate(const char *s, time_t *out);
// A UTC time as MET Norway writes it (ISO 8601): "2026-10-25T11:00:00Z".
// False, out untouched, on anything else.
bool parseIsoUtc(const char *s, time_t *out);
// The compiler's __DATE__ ("Oct 23 2026", "Oct  3 2026") as the UTC start of
// that day; 0 if unreadable.
time_t buildDay(const char *date);
// A clock reading worth trusting: not before the day the firmware was built.
bool timePlausible(time_t now, time_t built);
