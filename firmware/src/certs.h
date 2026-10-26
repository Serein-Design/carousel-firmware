// SPDX-License-Identifier: AGPL-3.0-only
// Root certificates trusted for HTTPS: Mozilla's list, as published by curl
// (https://curl.se/ca/cacert.pem), packed by scripts/gen_crt_bundle.py into
// certs/x509_crt_bundle.bin and embedded at build time (platformio.ini).
// Regenerate it with each release, roots expire.
#pragma once

#include <WiFiClientSecure.h>

extern const uint8_t rootCertBundle[] asm("_binary_certs_x509_crt_bundle_bin_start");

inline void trustRootCertificates(WiFiClientSecure &client) { client.setCACertBundle(rootCertBundle); }
