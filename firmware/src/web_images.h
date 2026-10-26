// SPDX-License-Identifier: AGPL-3.0-only
// The Images page, the settings page's Images block, the image list JSON
// and the upload gate, without the web server: checked on the PC by
// test/test_web_images; web.cpp sends what they build and acts on the
// gate's verdict.
#pragma once

#include <Arduino.h>

#include "content_logic.h"
#include "i18n.h"
#include "web_shell.h"   // escapeHtml

// The README's "Build and flash" section, "How to reinstall over USB": for
// the Images section without storage and the locked page
void reinstallLink(String &s, const Strings &t);
// The locked page's title in both languages, plain text for <title>, since
// its language setting cannot be read
void resetFailedHeading(String &s);

// The block with 24 names of 40 characters and the longest notice, in
// every language, fits this reserve (test_section_fits_reserve), so the
// page's middle chunk is built with one allocation.
static const size_t IMAGES_SECTION_RESERVE = 12288;
// A page upload announced over the cap but at most this is read and
// discarded, so the browser shows the refusal instead of a connection error.
// Above it, the answer is sent and the connection closed.
static const uint32_t PAGE_DRAIN_MAX = 1024UL * 1024;

// What the last page upload did, for the notice above the Space row.
// sent: the name as the browser sent it (unescaped), name: the stored name,
// needed / left: bytes, for "no room". imageOff: an image added or replaced
// while IMAGE is off in the owner's list, so stored and not shown.
struct ImagesNotice { UploadResult result; const char *sent; uint32_t needed, left;
                      GifHeader header; const char *name; bool imageOff; };

// The Images page. storage false: "Not available" and the reinstall link,
// no form, notice ignored. list: count images in alphabetical order;
// choice: the saved one, "" or nullptr = all in turn; notice may be nullptr.
// generation: contentGeneration(), in each thumbnail's address, so a
// replaced file is never answered from the browser's cache.
struct ImagesPageView { bool storage; const ImageInfo *list; int count; Space space; const char *choice;
                        const ImagesNotice *notice; uint32_t generation; };
// The whole page; flush sends what s holds so far and empties it
void imagesPage(String &s, const Shell &sh, const ImagesPageView &v, void (*flush)(String &));
// GET /api/images: {"images":[{"name","orientation","size"}],"count","max",
// "used","left","total","choice"} with choice "all" for all in turn; storage
// false: {"error":"no storage area"}, sent with 503.
void imagesJson(String &s, bool storage, const ImageInfo *list, int count, const Space &sp,
                const char *choice);

// What to do with an upload before its body is read. CLOSE_*: answer that
// status and close (lingering); DRAIN_413: answer 413 after reading and
// discarding the body (page only); SIGN_IN: the page's sign-in answer,
// then close. GATE_CLOSE_400 is the raw (non-multipart) body's refusal,
// decided by the handler, which has no multipart part to gate.
enum GateVerdict : uint8_t { GATE_ACCEPT, GATE_DRAIN_413, GATE_CLOSE_400, GATE_CLOSE_401,
                             GATE_CLOSE_403, GATE_CLOSE_413, GATE_SIGN_IN };
// api: /api/images (Bearer token or session cookie) rather than the page
// (cookie only); announced: the request's Content-Length, 0 = unknown.
struct GateInput { bool api, hostOk, crossSite, bearerOk, cookieOk; uint32_t announced; };
// Host and cross-site first, then authorisation, then the announced length:
// nothing about the request is believed before it is known to be the
// owner's.
GateVerdict uploadGate(const GateInput &in);

// Which file part of a multipart body the parser is in: only the first is
// stored. The count stops at 2, so no number of parts in one body can
// wrap it back to the first.
class FileParts {
public:
  void reset() { n_ = 0; }
  void next() { if (n_ < 2) n_++; }
  bool first() const { return n_ == 1; }
private:
  uint8_t n_ = 0;
};
