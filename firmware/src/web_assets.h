// SPDX-License-Identifier: AGPL-3.0-only
// The stylesheet every page links, compressed once at development time
// (tools/webcheck/assets.mjs, from firmware/web/page.css) and served as is
// with Content-Encoding: gzip, so it costs flash once instead of inline in
// every page.
#pragma once

#include <Arduino.h>

extern const uint8_t STYLESHEET_GZ[];
extern const size_t STYLESHEET_GZ_LEN;   // at most 6144 bytes (test_web_shell)
