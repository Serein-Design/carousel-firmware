// SPDX-License-Identifier: AGPL-3.0-only
// GET /install, hardware-free: the prelude that hands the installer its
// token, address and name, and the answers that refuse it. curl pipes
// whatever comes back into sh, so a refusal is a script too: one line on
// stderr, exit 1.
#pragma once

#include <cstddef>
#include <cstdint>

constexpr size_t INSTALL_PRELUDE_SIZE = 128;

// "#!/bin/sh\nCAROUSEL_TOKEN='..'\nCAROUSEL_IP='..'\nCAROUSEL_NAME='..'\n": its
// length, or -1 when the token is not 32 lowercase hex, the IP not digits and
// dots, the name not [a-z0-9-], or it does not fit with its terminating zero.
// Single quotes around values that cannot hold one: nothing in them is ever
// read by sh as code.
int installPrelude(char *out, size_t size, const char *token, const char *ip, const char *name);

// The installer goes out gzip-compressed, as it sits in flash (agent_scripts.h),
// for "curl --compressed". The prelude changes per panel, so the panel wraps
// it around the flash bytes without compressing anything: the gzip header and
// a stored (uncompressed) deflate block holding the prelude, the body's own
// deflate blocks, then the gzip trailer over both (RFC 1951 and 1952).
constexpr size_t INSTALL_GZIP_HEAD_SIZE = 15;
constexpr size_t INSTALL_GZIP_TAIL_SIZE = 8;
// The gzip header, then the head of a stored block of preludeLen bytes
// (at most 65 535), not the last block: the body's follow
void installGzipHead(uint8_t *out, size_t preludeLen);
// The gzip trailer: the CRC-32 and the length of the prelude followed by a
// body of bodyLen bytes whose own CRC-32 is bodyCrc
void installGzipTail(uint8_t *out, const char *prelude, size_t preludeLen, uint32_t bodyCrc,
                     size_t bodyLen);

enum InstallRefusal : uint8_t { INSTALL_BAD_LINK, INSTALL_LOCKED };
// A valid sh script that says why on stderr and exits 1; it holds no token
const char *installRefusalScript(InstallRefusal why);
