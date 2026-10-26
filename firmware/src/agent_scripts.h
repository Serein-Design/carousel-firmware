// SPDX-License-Identifier: AGPL-3.0-only
// The installer GET /install serves, after its prelude (agent_install.h):
// tools/agent-bridge/install.sh with hook.sh and setup.sh inside it, as
// tools/agent-bridge/embed.mjs writes it into agent_scripts.cpp. Run that
// script after every edit of one of the three; the installer test fails
// while the C++ file is stale. Compressed (raw deflate), sent from flash in
// chunks inside the gzip stream agent_install.h describes, never copied.
#pragma once

#include <Arduino.h>

extern const uint8_t INSTALL_SH_GZ[];
extern const size_t INSTALL_SH_GZ_LEN;   // at most 16384 bytes (static_assert)
extern const size_t INSTALL_SH_LEN;      // the script, inflated
extern const uint32_t INSTALL_SH_CRC;    // its CRC-32, for the gzip trailer
