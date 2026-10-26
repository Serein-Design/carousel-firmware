// SPDX-License-Identifier: AGPL-3.0-only
// IMAGE: plays the GIFs uploaded on the settings page, the ones drawn for the
// panel's orientation, one after the other or the one chosen.
//
// Memory: the decoder (about 24 KB, its canvas inside) is reserved only while
// this screen is drawn, and given back when another screen, an overlay or an
// update check takes over. Never static: the static RAM budget has
// no room for it.
//
// Concurrency: the file is the content module's, shared with the web task's
// delete and replace. Every playback call runs under contentPlayLock(), a
// try-lock: when busy the screen shows its last frame and tries again at the
// next call, so the display loop never waits.
#include <new>

#include "display.h"
#include "screens/screens.h"
#include "content.h"
#include "gallery.h"
#include "gif_decoder.h"
#include "grid.h"
#include "i18n.h"
#include "palette.h"
#include "text_logic.h"
#include "type.h"
#include "updater.h"

namespace {

// The playback file as the decoder sees it
struct PlayReader : GifReader {
  int32_t read(uint8_t *buf, int32_t len) override { return contentPlayRead(buf, len); }
  bool seek(int32_t pos) override { return contentPlaySeek(pos) == pos; }
};

PlayReader reader;
GifDecoder *gif = nullptr;
Retry reserve;                         // the decoder's reservation
bool reserveLogged = false;            // one log line per run of failures

ImageState state = IMG_PLAY;
bool playing = false;                  // a file open, its canvas worth showing
bool still = false;                    // a one-frame image, decoded once
bool restart = false;                  // tilted or shown again: pick again (under the lock)
uint32_t shownAt = 0, frameAt = 0;     // first frame of this turn; next frame due
uint32_t pickAt = 0;                   // next pick, when not playing
char name[IMAGE_NAME_MAX + 1] = "";    // the image playing
// The name the pick goes on from. Picking again from it gives the same image
// back (unless a just-added one or a new choice comes first), so an image cut
// short by an overlay restarts from frame 0; a turn moves it to `name`.
char after[IMAGE_NAME_MAX + 1] = "";
bool skipAfter = false;                // `after` failed to decode: not picked again at once
uint8_t failedInRow = 0;               // every image broken: stop and wait for the next turn

// Broken images are logged once per name per start. Hashes, not
// names: 24 names would take 984 bytes of static RAM.
uint32_t logged[MAX_IMAGES];
uint8_t loggedCount = 0;

bool due(uint32_t at, uint32_t now) { return (int32_t)(now - at) >= 0; }

uint32_t nameHash(const char *s) {     // FNV-1a
  uint32_t h = 2166136261u;
  while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
  return h;
}

void logBroken(const char *why) {
  const uint32_t h = nameHash(name);
  for (uint8_t i = 0; i < loggedCount; i++) if (logged[i] == h) return;
  if (loggedCount < MAX_IMAGES) logged[loggedCount++] = h;
  Serial.printf("[IMAGE] %s skipped: %s\n", name, why);
}

// Close the file; the next pick may run at once. Call under the lock.
void stopUnderLock(uint32_t now) {
  contentPlayClose();
  playing = still = false;
  pickAt = now;
}

// The image cannot be played: skip it and pick the next one. When every
// image of the orientation failed in a row, stay black until the next turn.
void broken(const char *why, uint32_t now) {
  logBroken(why);
  stopUnderLock(now);
  strlcpy(after, name, sizeof(after));
  skipAfter = true;
  if (++failedInRow >= MAX_IMAGES) {
    failedInRow = 0;
    skipAfter = false;
    pickAt = now + TURN_MS;
  }
}

uint32_t turns = 0;                    // turns ended since the start, for imageTurns()

// The turn is over: on to the next image, from this one
void turn(uint32_t now) {
  turns++;
  strlcpy(after, name, sizeof(after));
  skipAfter = false;
  stopUnderLock(now);
}

// One frame from the decoder; at the end of a loop, the next one or the next
// image. Call under the lock.
void step(uint32_t now) {
  for (int tries = 0; tries < 2; tries++) {         // a loop that ended starts again once
    uint32_t ms = 0;
    const GifStatus s = gif->next(&ms);
    if (s == GIF_FRAME) {
      frameAt = now + gifFrameDelay((int)ms);
      failedInRow = 0;
      return;
    }
    if (s == GIF_ERROR) return broken(gifErrorName(gif->error()), now);
    // GIF_END: a still stays as it is, an animation plays again
    still = gif->frames() == 1;
    if (still || turnOver(now - shownAt, true, false)) {
      if (!still) turn(now);
      return;
    }
    if (!gif->rewind()) return broken(gifErrorName(gif->error()), now);
  }
}

// Pick the image of this orientation and decode its first frame
void start(uint32_t now) {
  if (!contentPlayLock()) return;                    // busy: try at the next call
  restart = false;
  char picked[IMAGE_NAME_MAX + 1];
  const ImagePick p = contentPick(portrait, after, skipAfter ? after : nullptr, picked,
                                  sizeof(picked));
  state = p.state;
  if (p.state != IMG_PLAY) {
    pickAt = now + RETRY_MS;                         // an upload may arrive any time
  } else if (p.index < 0) {
    pickAt = now + TURN_MS;                          // only broken images: black
    skipAfter = false;
  } else {
    strlcpy(name, picked, sizeof(name));
    // open() reads from the current position and rewind() counts from 0
    if (!contentPlayOpen(name, nullptr) || !reader.seek(0)) {
      broken("unreadable", now);
    } else if (!gif->open(reader)) {
      broken(gifErrorName(gif->error()), now);
    } else {
      playing = true;
      still = false;
      shownAt = now;
      step(now);
    }
  }
  contentPlayUnlock();
}

// The next frame when it is due, the turn when it is over
void advance(uint32_t now) {
  if (!contentPlayLock()) return;                    // busy: the last frame stays
  if (restart || !contentPlayValid()) {
    stopUnderLock(now);                              // deleted or replaced: pick again
  } else if (still) {
    if (turnOver(now - shownAt, true, true)) turn(now);
  } else if (turnOver(now - shownAt, false, false)) {
    turn(now);                                       // a loop longer than a minute
  } else if (due(frameAt, now)) {
    step(now);
  }
  contentPlayUnlock();
}

// --- States without an image, as the prototype ---------------

// 13x10 picture: a frame, a sun and a hill; crossed in the alert colour
// without storage
void icon(int x, int y, bool crossed, const Palette &p) {
  const uint16_t c = crossed ? p.alert : p.accent;
  const uint16_t hill = crossed ? p.dim : p.accent;
  display->drawRect(x, y, 13, 10, c);
  display->fillRect(x + 3, y + 2, 2, 2, c);
  for (int i = 2; i <= 10; i++) {
    const int top = i <= 4 ? 9 - i : i <= 6 ? i - 1 : i <= 9 ? 13 - i : 5;
    const int from = top > 4 ? top : 4;
    display->drawFastVLine(x + i, y + from, 8 - from, hill);
  }
  if (crossed)
    for (int i = 0; i < 13; i++) display->drawPixel(x + i, y + (i * 9 + 6) / 12, c);
}

void drawState(ImageState s) {
  if (s == IMG_NO_IMAGE) {
    drawNoImage();
    return;
  }
  const Palette &p = palette();
  const Strings &t = tr();
  const bool crossed = s == IMG_NO_STORAGE;
  if (!portrait) {
    const char *const *w = s == IMG_OTHER_ONLY ? t.imgOther : t.imgNoStore;
    icon(25, 2, crossed, p);
    drawText(*display, w[0], W() / 2, 14, Face::BODY, p.text, Align::CENTER);
    drawText(*display, w[1], W() / 2, 24, Face::LABEL, p.label, Align::CENTER);
  } else {
    const char *const *w = s == IMG_OTHER_ONLY ? t.imgOtherTall : t.imgNoStoreTall;
    const Face top = stateFace(w[0], w[1], GRID_PORTRAIT_LINE_W);   // STORE: too wide for the body face
    icon(9, 6, crossed, p);
    drawText(*display, w[0], W() / 2, 22, top, p.text, Align::CENTER);
    drawText(*display, w[1], W() / 2, 31, top, p.text, Align::CENTER);
    drawText(*display, w[2], W() / 2, 45, Face::LABEL, p.label, Align::CENTER);
    drawText(*display, w[3], W() / 2, 52, Face::LABEL, p.label, Align::CENTER);
  }
}

}  // namespace

uint32_t imageTurns() { return turns; }

void screenImageLeave() {
  // Busy: the file stays open until the next pick; delete, replace and the
  // factory reset close it themselves before touching it.
  if (contentPlayLock()) {
    contentPlayClose();
    contentPlayUnlock();
  }
  delete gif;
  gif = nullptr;
  playing = still = false;
  pickAt = millis();
}

void screenImage(uint32_t f, bool init) {
  (void)f;
  // An update check needs the memory: the last frame stays meanwhile
  if (updateStatus.state == UPD_CHECKING) {
    if (gif) screenImageLeave();
    return;
  }
  const uint32_t now = millis();
  if (init) {
    restart = true;
    pickAt = now;
  }
  if (!gif) {
    if (!reserve.due(now)) {
      display->clearScreen();
      return;
    }
    gif = new (std::nothrow) GifDecoder;
    if (!gif) {
      if (!reserveLogged)
        Serial.printf("[IMAGE] no memory for the decoder (%u B), retrying every %lu ms\n",
                      (unsigned)sizeof(GifDecoder), (unsigned long)RETRY_MS);
      reserveLogged = true;
      reserve.failed(now);
      display->clearScreen();
      return;
    }
    reserve = Retry();
    reserveLogged = false;
    playing = false;
    pickAt = now;
  }
  if (playing) advance(now);
  if (!playing && due(pickAt, now)) start(now);

  if (playing && gif->width() == W() && gif->height() == H()) {
    display->drawRGBBitmap(0, 0, gif->canvas(), W(), H());
  } else {
    display->clearScreen();
    if (state != IMG_PLAY) drawState(state);
  }
}
