// SPDX-License-Identifier: AGPL-3.0-only
#include "boot_logic.h"

#include <math.h>
#include <string.h>

#include "ease.h"
#include "feedback_style.h"
#include "frame_logic.h"

// A release is built from its tag, "v1.2.3" (the release workflow sets
// FIRMWARE_VERSION to it). git describe adds "-<n>-g<hash>" past a tag and
// "-dirty" for uncommitted changes, and gives a bare hash (or "dev", without
// git) when no tag is reachable.
static bool releaseVersion(const char *v) {
  return (v[0] == 'v' || v[0] == 'V') && v[1] >= '0' && v[1] <= '9' && !strstr(v, "-g") &&
         !strstr(v, "dirty");
}

bool bootDetailedFor(uint8_t bootDetail, const char *version, bool testConsole) {
  if (bootDetail == BOOT_SIMPLE) return false;
  if (bootDetail == BOOT_DETAILED) return true;
  return testConsole || !releaseVersion(version);
}

void bootSplitIp(const char *ip, char *first, char *second, size_t size) {
  const char *dot = strchr(ip, '.');
  if (dot) dot = strchr(dot + 1, '.');
  const size_t n = dot ? (size_t)(dot + 1 - ip) : strlen(ip);
  const size_t head = n < size ? n : size - 1;
  memcpy(first, ip, head);
  first[head] = '\0';
  strncpy(second, dot ? dot + 1 : "", size - 1);
  second[size - 1] = '\0';
}

uint16_t bootFaded(uint16_t c, uint8_t level) {
  const uint32_t r = (uint32_t)(c >> 11) * level / 255;
  const uint32_t g = (uint32_t)((c >> 5) & 0x3F) * level / 255;
  const uint32_t b = (uint32_t)(c & 0x1F) * level / 255;
  return (uint16_t)(r << 11 | g << 5 | b);
}

void BootProgress::update(const BootInputs &in, uint32_t now) {
  if (!started_) {
    started_ = true;
    startAt_ = now;
  }
  if (in.wifiJoined && !joined_) {
    joined_   = true;
    joinedAt_ = now;
  }
  const bool failed = in.wifiFailed && !joined_;
  if (failed != stopped_) {
    stopped_   = failed;
    stoppedAt_ = now;
  }
  setupOpen_ = setupOpen_ || in.setupOpen;
  if (!ready_ && (joined_ || now >= BOOT_MAX_MS)) {
    ready_   = true;
    readyAt_ = now;
  }
}

uint32_t BootProgress::openAt() const {
  const uint32_t least = startAt_ + wordAt_ + BOOT_WORD_BEAT_MS;
  return readyAt_ > least ? readyAt_ : least;
}

uint8_t BootProgress::beat(uint32_t now) const {
  if (setupOpen_) return BEAT_OVER;
  if (now - startAt_ < wordAt_) return BEAT_MARK;
  if (!ready_ || now < openAt()) return BEAT_WORD;
  return now - openAt() < BOOT_OPEN_MS ? BEAT_OPEN : BEAT_OVER;
}

// 255 * num / den, for a fade
static uint8_t fraction(uint32_t num, uint32_t den) {
  return num >= den ? 255 : (uint8_t)(255 * num / den);
}

uint8_t BootProgress::markLevel(uint32_t now) const {
  const uint32_t t = now - startAt_;
  if (wordAt_ == 0) return 0;
  if (t < BOOT_MARK_HOLD_MS) return fraction(t, BOOT_MARK_IN_MS);
  return 255 - fraction(t - BOOT_MARK_HOLD_MS, BOOT_MARK_OUT_MS);
}

uint8_t BootProgress::wordLevel(uint32_t now) const {
  const uint32_t t = now - startAt_;
  return t < wordAt_ ? 0 : fraction(t - wordAt_, BOOT_WORD_FADE_MS);
}

float BootProgress::opened(uint32_t now) const {
  if (!ready_ || setupOpen_ || now < openAt()) return 0.0f;
  const uint32_t t = now - openAt();
  return t >= BOOT_OPEN_MS ? 1.0f : (float)t / BOOT_OPEN_MS;
}

float BootProgress::creep(uint32_t now) const {
  const uint32_t word = startAt_ + wordAt_;
  const uint32_t at = stopped_ && stoppedAt_ < now ? stoppedAt_ : now;
  if (at <= word) return 0.0f;
  const uint32_t in = at - word < BOOT_CREEP_MS ? at - word : BOOT_CREEP_MS;
  return 0.75f * in / BOOT_CREEP_MS;
}

uint8_t BootProgress::fill(uint8_t width, uint32_t now) const {
  if (!ready_) return (uint8_t)(width * creep(now));
  // From where the line was when the Wi-Fi came (or the word, if later) to
  // full as the opening starts
  const uint32_t word = startAt_ + wordAt_;
  const uint32_t from = readyAt_ > word ? readyAt_ : word;
  const uint32_t open = openAt();
  if (now >= open || open <= from) return width;
  const float start = creep(from);
  const float t = now <= from ? 0.0f : (float)(now - from) / (open - from);
  return (uint8_t)(width * (start + (1.0f - start) * t));
}

uint8_t BootProgress::ipLevel(uint32_t now) const {
  return joined_ ? fraction(now - joinedAt_, BOOT_IP_FADE_MS) : 0;
}

BootLayout bootLayout(int16_t height, int16_t inkHeight, bool detailed) {
  // The line three rows under the word's ink, the IP three under the line
  const int16_t group = inkHeight + 4;
  BootLayout l;
  l.wordY = (height - group + 1) / 2;
  l.lineY = l.wordY + inkHeight + 3;
  l.ipY   = detailed ? l.lineY + 4 : -1;
  return l;
}

void BootInk::drawPixel(int16_t x, int16_t y, uint16_t) {
  if (x < left) left = x;
  if (x > right) right = x;
  if (y < top) top = y;
  if (y > bottom) bottom = y;
}

static uint8_t light(float share) {
  return share <= 0.0f ? 0 : share >= 1.0f ? 255 : (uint8_t)(255 * share);
}

static int16_t lerp(int16_t a, int16_t b, float e) { return (int16_t)lroundf(a + (b - a) * e); }

BootOpeningFrame bootOpeningAt(float p, int16_t w, int16_t h, int16_t lineY, int16_t inkLeft,
                               int16_t inkRight) {
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  BootOpeningFrame f = {0, 255, -1, 0, 0};
  const float widen = (float)BOOT_LINE_WIDEN_MS / BOOT_OPEN_MS;
  if (p < widen) {
    // The word fades out as its line widens to the panel's full width, the
    // screen not yet in
    const float e = easeInOut(p / widen);
    f.wordLight = light(1.0f - e);
    f.stageTop  = h;
    f.lineRow   = lineY;
    f.lineLeft  = lerp(inkLeft, 0, e);
    f.lineRight = lerp(inkRight, w - 1, e);
    return f;
  }
  // The screen hangs from the line as it rises over the top
  const float e = easeInOut((p - widen) / (1.0f - widen));
  f.wordLight = 0;
  f.lineRow   = lerp(lineY + 1, 0, e) - 1;
  f.stageTop  = f.lineRow + 1;
  f.lineRight = w - 1;
  return f;
}

void bootOpeningStage(const BootOpeningFrame &f, const uint16_t *stage, uint16_t *out,
                      int16_t width, int16_t height, uint8_t rotation) {
  const bool swap = rotation & 1;
  const int w = swap ? height : width, h = swap ? width : height;
  for (int y = f.stageTop > 0 ? f.stageTop : 0; y < h; y++)
    for (int x = 0; x < w; x++)
      out[frameIndex(rotation, width, height, x, y)] =
          stage[frameIndex(rotation, width, height, x, y - f.stageTop)];
}
