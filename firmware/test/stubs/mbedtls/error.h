// SPDX-License-Identifier: AGPL-3.0-only
// Stand-in for mbedTLS's error.h: the one call src/error_codes.cpp replaces.
#pragma once

#include <stddef.h>

extern "C" void mbedtls_strerror(int errnum, char *buffer, size_t buflen);
