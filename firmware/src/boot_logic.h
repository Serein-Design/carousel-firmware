// SPDX-License-Identifier: AGPL-3.0-only
// The boot sequence's logic, without the hardware: which mode the owner's
// setting means on this build, the wait until the panel is ready (the Wi-Fi
// joined), the beats (the mark fading in and out, a black gap, the word
// fading in, the line widening and the first screen rising from it), who
// draws the frame (the mark's task, then loop()), how far the line is
// filled, Detailed's IP and its light, and where the word, line and IP sit;
// and what the opening shows of the first screen and of the word beat at each
// moment.
// Host-tested in test_boot; screens/boot.cpp draws from it.
#pragma once

#include <Adafruit_GFX.h>
#include <atomic>
#include <stddef.h>
#include <stdint.h>

// The beats, from the sequence's first frame (the mark's task draws it from
// setup(), boot_task.h): the mark fades in over BOOT_MARK_IN_MS, holds until
// BOOT_MARK_HOLD_MS, fades out over BOOT_MARK_OUT_MS to black; the panel
// stays black BOOT_BLACK_MS; then the word and its line fade in over
// BOOT_WORD_FADE_MS from BOOT_WORD_AT_MS. Never both lit: the owner judged
// the crossfade on the panel (2026-10-23). The word beat lasts at least
// until BOOT_MIN_MS. A panel ready before it still sees both beats whole; one
// ready later opens at once, nothing held after the line fills.
#define BOOT_MARK_IN_MS    600UL
#define BOOT_MARK_HOLD_MS  1500UL
#define BOOT_MARK_OUT_MS   600UL
#define BOOT_BLACK_MS      300UL
#define BOOT_WORD_AT_MS    (BOOT_MARK_HOLD_MS + BOOT_MARK_OUT_MS + BOOT_BLACK_MS)
#define BOOT_WORD_FADE_MS  600UL
#define BOOT_MIN_MS        4000UL
// What the word's beat lasts at least: the same after a software restart,
// which starts at the word (BootProgress::shorten)
#define BOOT_WORD_BEAT_MS  (BOOT_MIN_MS - BOOT_WORD_AT_MS)
// The opening: the line under the word widens over BOOT_LINE_WIDEN_MS to the
// panel's full width as the word fades out, then the first screen rises from
// it over BOOT_LINE_RISE_MS, both on a symmetric ease-in-out so they start and
// land softly
#define BOOT_LINE_WIDEN_MS 500UL
#define BOOT_LINE_RISE_MS  900UL
#define BOOT_OPEN_MS       (BOOT_LINE_WIDEN_MS + BOOT_LINE_RISE_MS)
// Since power-up, the longest the sequence waits: past it the screen opens,
// whatever is still pending (the Wi-Fi keeps trying behind them, as before)
#define BOOT_MAX_MS        15000UL
// While the Wi-Fi is awaited the line creeps towards three quarters over
// this long from the word's beat, then waits: it moves while something
// happens, never claims the Wi-Fi joined
#define BOOT_CREEP_MS      4000UL
// Detailed's IP fades in over this long once the Wi-Fi is joined
#define BOOT_IP_FADE_MS    400UL

// BEAT_MARK runs to BOOT_WORD_AT_MS, the black gap included
enum BootBeat : uint8_t { BEAT_MARK, BEAT_WORD, BEAT_OPEN, BEAT_OVER };

// Who draws the boot's frames: the mark's task from setup() on, so the mark
// lights while setup() is still starting the rest, then loop(). One owner at
// a time: the task checks before each frame and lets go once loop() asked;
// loop() draws only once it let go. Without a task, loop()'s from the start.
class BootBaton {
public:
  // Before the task starts: the frame is its own
  void taskTakes() { owner_ = TASK; }
  // Before each of the task's frames: false once loop() asked
  bool taskMayDraw() const { return owner_ == TASK; }
  // loop() wants the frame: the task's frame under way is its last
  void loopAsks() {
    uint8_t task = TASK;
    owner_.compare_exchange_strong(task, ASKED);
  }
  // The task drew its last frame: the frame is loop()'s
  void taskLeft() { owner_ = LOOP; }
  bool loopMayDraw() const { return owner_ == LOOP; }

private:
  enum : uint8_t { LOOP, TASK, ASKED };
  std::atomic<uint8_t> owner_{LOOP};
};

// What bootDetail (feedback_style.h) means on this build: Auto is Detailed
// on a development build (git describe past a tag, a bare commit, a dirty
// tree, or the test console built in), Simple on a release tag.
bool bootDetailedFor(uint8_t bootDetail, const char *version, bool testConsole);

// An IPv4 address in two lines for portrait, cut after its second dot
// ("192.168." and "1.42"); first holds the whole text when there is no
// second dot
void bootSplitIp(const char *ip, char *first, char *second, size_t size);

// c (RGB565) at level/255 of its light, each channel alone: every fade of the boot
uint16_t bootFaded(uint16_t c, uint8_t level);

struct BootInputs {
  bool wifiJoined;    // the station has an IP: the panel is ready
  bool setupOpen;     // the setup access point opened: its own screen takes over
  bool wifiFailed;    // the router refused the password, or the network is not found
};

class BootProgress {
public:
  // After a software restart (an update, the page's restart): no mark and no
  // black gap, the sequence starts at the word. Before the first update; a
  // power-on keeps the whole sequence.
  void shorten() { wordAt_ = 0; }
  // Once per frame, with millis(): the first call starts the sequence
  void update(const BootInputs &in, uint32_t now);
  // The Wi-Fi failed: the line holds where it was
  bool stopped() const { return stopped_; }
  // The beat on show; BEAT_OVER at once when the setup access point opens
  // (its screen takes over, no curtains)
  uint8_t beat(uint32_t now) const;
  // The mark's light, 0-255: rising over BOOT_MARK_IN_MS, held, then falling
  // over BOOT_MARK_OUT_MS from BOOT_MARK_HOLD_MS, dark after
  uint8_t markLevel(uint32_t now) const;
  // The word's light, 0-255: dark until BOOT_WORD_AT_MS, rising over
  // BOOT_WORD_FADE_MS; scales the word, its line and Detailed's IP
  uint8_t wordLevel(uint32_t now) const;
  // How far the opening went, 0 to 1, linear in time: 0 before it, 1 after
  float opened(uint32_t now) const;
  // Lit columns of a line width columns wide. Empty when the word comes in,
  // creeping while the Wi-Fi is awaited; once it is joined, full by the time
  // the opening starts (at once when that is now)
  uint8_t fill(uint8_t width, uint32_t now) const;
  // Detailed's IP's light, 0-255, on top of the word's: dark until the Wi-Fi
  // is joined, then rising over BOOT_IP_FADE_MS
  uint8_t ipLevel(uint32_t now) const;

private:
  // When the opening starts; only meaningful once ready_
  uint32_t openAt() const;
  // The line's share, 0-1, while the Wi-Fi is awaited
  float creep(uint32_t now) const;

  uint32_t wordAt_    = BOOT_WORD_AT_MS;   // from the first frame to the word
  bool     started_   = false;
  bool     stopped_   = false;
  bool     setupOpen_ = false;
  bool     ready_     = false;
  bool     joined_    = false;
  uint32_t startAt_   = 0;   // the sequence's first frame
  uint32_t readyAt_   = 0;   // the Wi-Fi joined, or BOOT_MAX_MS reached
  uint32_t joinedAt_  = 0;
  uint32_t stoppedAt_ = 0;
};

// Where the word beat sits, rows from the top: the word's ink, the line,
// Detailed's IP (-1: Simple, none). The word and its line are centred the
// same in both modes, the IP comes under the line (landscape: the foot row).
struct BootLayout {
  int16_t wordY, lineY, ipY;
};
BootLayout bootLayout(int16_t height, int16_t inkHeight, bool detailed);

// One frame of the opening, at progress p (BootProgress::opened): how much of
// the first screen shows, and how the word beat is drawn over the black around
// it. The screen shows from row stageTop down (its own top row there); the
// word beat at wordLight (0-255); the full line on row lineRow (-1: none),
// from column lineLeft to lineRight.
struct BootOpeningFrame {
  int16_t stageTop;
  uint8_t wordLight;
  int16_t lineRow, lineLeft, lineRight;
};
// w x h the frame's size on screen; lineY, inkLeft and inkRight the word
// beat's line and the word's ink (bootLayout, BootInk)
BootOpeningFrame bootOpeningAt(float p, int16_t w, int16_t h, int16_t lineY, int16_t inkLeft,
                               int16_t inkRight);

// Writes the first screen into out where the opening frame shows it, nothing
// elsewhere: stage is the whole first screen as drawn, out the frame; both
// width x height RGB565 in the panel's unrotated layout, rotation the frame's
void bootOpeningStage(const BootOpeningFrame &f, const uint16_t *stage, uint16_t *out,
                      int16_t width, int16_t height, uint8_t rotation);

// Draws nothing, keeps the box of what was drawn: the word's ink, whatever
// face or artwork draws it, so the line and the layout follow it
class BootInk : public Adafruit_GFX {
public:
  BootInk(int16_t w, int16_t h) : Adafruit_GFX(w, h) {}
  void drawPixel(int16_t x, int16_t y, uint16_t) override;
  bool any() const { return right >= left; }
  int16_t left = INT16_MAX, right = INT16_MIN, top = INT16_MAX, bottom = INT16_MIN;
};
