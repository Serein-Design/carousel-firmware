// SPDX-License-Identifier: AGPL-3.0-only
// Stand-in for the ESP32's microsecond timer: the time since boot a test sets.
#pragma once

#include <cstdint>

inline int64_t fakeUptimeUs = 0;
inline int64_t esp_timer_get_time() { return fakeUptimeUs; }
