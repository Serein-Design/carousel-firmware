// SPDX-License-Identifier: AGPL-3.0-only
// The parts of signed updates that need no network: versions and the signed
// message. Pure, checked on the PC by test/test_update.
#pragma once

#include <stddef.h>
#include <stdint.h>

// "v1.2.3" (or "1.2.3", anything after a '-' ignored) -> {1, 2, 3}
bool parseVersion(const char *v, int out[3]);
// candidate strictly above current; false when either does not parse
bool isNewer(const char *candidate, const char *current);
// A tagged release, "v1.2.3" exactly; development builds ("v1.2.3-4-gabc")
// never update themselves.
bool isReleaseVersion(const char *v);
// What the release workflow signs: "carousel-firmware <version> <sha256 hex>".
// The version is inside, so an old signed image cannot pose as a new one.
void signedMessage(char *out, size_t size, const char *version, const uint8_t sha[32]);
// Hex string -> bytes; false on odd length, bad digit or too long
bool hexToBytes(const char *hex, uint8_t *out, size_t max, size_t &len);
