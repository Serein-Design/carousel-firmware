// SPDX-License-Identifier: AGPL-3.0-only
// The Stage preview without the hardware: the frame the panel shows as a
// 24-bit BMP (every browser draws one without a script), and the gate every
// read-only browser file goes through. Checked on the PC by test/test_preview.
#pragma once

#include <stddef.h>
#include <stdint.h>

// The panel's own size, unrotated (web.cpp checks it against board.h)
static const int PREVIEW_W = 64, PREVIEW_H = 32;
static const size_t PREVIEW_BMP_SIZE = 54 + PREVIEW_W * PREVIEW_H * 3;   // 6198

// A read-only file a page loads (the preview, an image): 403 for an unknown
// host or another site, then 401 without a session, then 503 while locked or
// with nothing to serve. Never the sign-in page: opening it puts a code on
// the panel. The preview passes locked=false (RESET FAILED holds no secret)
// and available=true.
enum ReadVerdict : uint8_t { READ_OK, READ_FORBIDDEN, READ_UNAUTHORIZED, READ_UNAVAILABLE };
ReadVerdict readGate(bool hostOk, bool crossSite, bool cookieOk, bool locked, bool available);

// shown: PREVIEW_W x PREVIEW_H RGB565, row after row, the panel's unrotated
// layout. rotation: the frame's, 0-3, mapped back as Adafruit GFX's
// GFXcanvas16::drawPixel maps it, so a portrait frame comes out upright,
// 32 wide and 64 tall. black: every pixel black and shown not read (the
// sign-in code is on the panel). out: PREVIEW_BMP_SIZE bytes.
void previewBmp(const uint16_t *shown, uint8_t rotation, bool black, uint8_t *out);
