// SPDX-License-Identifier: AGPL-3.0-only
// Every HTTPS session of the panel, in one task on core 0: the forecast, the
// settings page's city searches, update checks and installs, the SOCIAL
// counters, each in turn (https_logic for the timetable). A session takes
// about 40 KB of heap (two 16 KB mbedTLS buffers,
// CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN, plus the handshake) on a board without
// PSRAM: one at a time, so none fails an allocation another one took. And
// one 12 KB stack instead of three: measured on the panel, MET Norway's
// handshake (an RSA-4096 root) failed with 58 KB of byte-addressable RAM
// free and worked with 77 KB. Upgrade path: smaller or asymmetric mbedTLS
// buffers, which needs a framework built with another sdkconfig.
#pragma once

#include "https_logic.h"

void httpsBegin();            // after updateBegin(): starts the task
// From any task: the job's turn as soon as the one running ends
void httpsAsk(HttpsJob job);
// From the web task: a city search is waiting (places.cpp)
void httpsWake();
