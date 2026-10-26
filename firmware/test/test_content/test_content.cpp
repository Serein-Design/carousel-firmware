// SPDX-License-Identifier: AGPL-3.0-only
// Custom images without the hardware: names, GIF headers, space figures, the
// upload transaction against a fake store, the saved choice, what the IMAGE
// screen plays next, canvas clipping, and the factory reset's order.
//
//   pio test -e native -f test_content
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <Preferences.h>

#include "../../src/i18n.cpp"
#include "../../src/content_logic.cpp"

// settings.cpp's lock, set by an unfinished factory reset
static bool fakeLocked = false;
bool panelLocked() { return fakeLocked; }

void setUp() {
  fakeNvs.clear();
  nvsWriteCount = 0;
  nvsFailBegin = nvsFailWrites = false;
  fakeLocked = false;
}
void tearDown() {}

// --- Fakes and helpers -------------------------------------------------------

// Files in a map, one temp buffer; counts every write and the largest chunk.
struct FakeStore : ContentStore {
  std::map<std::string, std::vector<uint8_t>> files;
  std::vector<uint8_t> temp, firstWrite;
  bool tempOpen = false, failBegin = false, failWrite = false, failCommit = false;
  int writes = 0;
  size_t maxChunk = 0;
  uint32_t free = 90;

  bool beginTemp() override {
    if (failBegin) return false;
    tempOpen = true;
    temp.clear();
    return true;
  }
  bool writeTemp(const uint8_t *b, size_t n) override {
    if (!tempOpen || failWrite) return false;
    if (writes++ == 0) firstWrite.assign(b, b + n);
    if (n > maxChunk) maxChunk = n;
    temp.insert(temp.end(), b, b + n);
    return true;
  }
  void dropTemp() override {
    tempOpen = false;
    temp.clear();
  }
  bool commitTemp(const char *name) override {
    if (!tempOpen || failCommit) return false;
    files[name] = temp;
    dropTemp();
    return true;
  }
  bool remove(const char *name) override { return files.erase(name) > 0; }
  uint32_t freeBlocks() override { return free; }
  uint32_t blockSize() override { return 4096; }
  bool noTemp() const { return !tempOpen && temp.empty(); }
};

// A GIF header of the given size, padded to `size` bytes
static std::vector<uint8_t> gif(uint16_t w, uint16_t h, size_t size, uint8_t fill = 0x5A) {
  std::vector<uint8_t> b(size, fill);
  const uint8_t head[10] = {'G', 'I', 'F', '8', '9', 'a', (uint8_t)w, (uint8_t)(w >> 8),
                            (uint8_t)h, (uint8_t)(h >> 8)};
  memcpy(b.data(), head, std::min(size, sizeof(head)));
  return b;
}

static const size_t CHUNK = 1436;   // one TCP segment's worth, as the web server hands them

// start, every byte in 1 436-byte chunks (on after a refusal, as a drain
// does), finish
static UploadResult upload(ImageUpload &u, FakeStore &s, ImageIndex &idx, const char *name,
                           const std::vector<uint8_t> &data, bool storage = true,
                           uint32_t announced = 0) {
  UploadResult r = u.start(s, idx, storage, name, announced);
  if (r != UP_PENDING) return r;
  for (size_t i = 0; i < data.size(); i += CHUNK)
    u.write(s, data.data() + i, std::min(CHUNK, data.size() - i));
  return u.finish(s, idx);
}

static ImageInfo info(const char *name, uint32_t size = 1000, bool portrait = false) {
  ImageInfo i = {};
  snprintf(i.name, sizeof(i.name), "%s", name);
  i.size = size;
  i.portrait = portrait;
  return i;
}

static std::string sanitized(const char *in) {
  char out[IMAGE_NAME_MAX + 1];
  sanitizeImageName(in, out, sizeof(out));
  return out;
}

// --- Storage area -----------------------------------------------------------

static void test_partition_label_in_table() {
  FILE *f = fopen("partitions.csv", "r");   // pio test -d firmware runs in firmware/
  TEST_ASSERT_NOT_NULL_MESSAGE(f, "partitions.csv not found from the test's directory");
  char line[256];
  bool found = false;
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#') continue;
    char name[32] = "", type[32] = "";
    if (sscanf(line, " %31[^, ] , %31[^, ]", name, type) == 2 &&
        strcmp(name, CONTENT_PARTITION_LABEL) == 0 && strcmp(type, "data") == 0)
      found = true;
  }
  fclose(f);
  TEST_ASSERT_TRUE(found);
}

// The first 16 bytes of a block of the superblock pair: revision count, the
// name tag, then "littlefs" at byte 8
static std::vector<uint8_t> superblockHead(const char *name = "littlefs") {
  std::vector<uint8_t> b = {0x01, 0x00, 0x00, 0x00, 0xF0, 0x0F, 0xF0, 0x08};
  b.insert(b.end(), name, name + 8);
  return b;
}
static const std::vector<uint8_t> BLANK16(16, 0xFF);

// The order content.cpp follows, against a fake filesystem. block0/block1:
// what a raw read of the area's first two blocks returns.
struct FakeFs {
  bool found, pending;
  std::vector<bool> mountResults;
  std::vector<uint8_t> block0 = BLANK16, block1 = BLANK16;
  bool readOk = true;
  int mounts = 0, formats = 0;
  MountState mount() {
    if (mounts < (int)mountResults.size() && mountResults[mounts++]) return MOUNT_OK;
    return mountFailure(block0.data(), block1.data(), readOk);
  }
};
static StorageBoot bootLikeThePanel(FakeFs &fs) {
  StorageBoot b = storageBoot(fs.found, fs.pending, MOUNT_NOT_TRIED, false);
  if (b != BOOT_MOUNT) return b;
  b = storageBoot(fs.found, fs.pending, fs.mount(), false);
  if (b != BOOT_FORMAT_ONCE) return b;
  fs.formats++;
  fs.block0 = superblockHead();
  return storageBoot(fs.found, fs.pending, fs.mount(), true);
}

static void test_boot_steps() {
  FakeFs missing{false, false, {true}};
  TEST_ASSERT_EQUAL(BOOT_MISSING, bootLikeThePanel(missing));
  TEST_ASSERT_EQUAL(0, missing.mounts);
  TEST_ASSERT_EQUAL(0, missing.formats);

  FakeFs pending{true, true, {true}};
  TEST_ASSERT_EQUAL(BOOT_LOCKED, bootLikeThePanel(pending));
  TEST_ASSERT_EQUAL(0, pending.mounts);
  TEST_ASSERT_EQUAL(0, pending.formats);
  // whatever the caller passes, a pending reset never mounts nor formats
  TEST_ASSERT_EQUAL(BOOT_LOCKED, storageBoot(true, true, MOUNT_OK, false));
  TEST_ASSERT_EQUAL(BOOT_LOCKED, storageBoot(true, true, MOUNT_NO_FS, false));
  TEST_ASSERT_EQUAL(BOOT_LOCKED, storageBoot(true, true, MOUNT_FAILED, true));

  FakeFs ok{true, false, {true}};
  ok.block0 = superblockHead();
  TEST_ASSERT_EQUAL(BOOT_MOUNTED, bootLikeThePanel(ok));
  TEST_ASSERT_EQUAL(0, ok.formats);

  // An area without a filesystem (first start, after a factory reset)
  FakeFs fresh{true, false, {false, true}};
  TEST_ASSERT_EQUAL(BOOT_FORMAT_ONCE, storageBoot(true, false, MOUNT_NO_FS, false));
  TEST_ASSERT_EQUAL(BOOT_MOUNTED, bootLikeThePanel(fresh));
  TEST_ASSERT_EQUAL(1, fresh.formats);

  FakeFs broken{true, false, {false, false, false}};
  TEST_ASSERT_EQUAL(BOOT_UNAVAILABLE, bootLikeThePanel(broken));
  TEST_ASSERT_EQUAL(1, broken.formats);
  TEST_ASSERT_EQUAL(2, broken.mounts);
}

// A mount refused over a filesystem (no memory, a read error, a bad block)
// keeps the images: only an area without littlefs's name is formatted.
static void test_boot_keeps_a_filesystem_it_cannot_mount() {
  TEST_ASSERT_EQUAL(BOOT_UNAVAILABLE, storageBoot(true, false, MOUNT_FAILED, false));

  FakeFs inBlock0{true, false, {false, true}};
  inBlock0.block0 = superblockHead();
  TEST_ASSERT_EQUAL(BOOT_UNAVAILABLE, bootLikeThePanel(inBlock0));
  TEST_ASSERT_EQUAL(0, inBlock0.formats);
  TEST_ASSERT_EQUAL(1, inBlock0.mounts);

  // a power cut while littlefs rewrote block 0 leaves its name in block 1 only
  FakeFs inBlock1{true, false, {false, true}};
  inBlock1.block1 = superblockHead();
  TEST_ASSERT_EQUAL(BOOT_UNAVAILABLE, bootLikeThePanel(inBlock1));
  TEST_ASSERT_EQUAL(0, inBlock1.formats);

  // what the area holds is unknown: nothing erased
  FakeFs unreadable{true, false, {false, true}};
  unreadable.readOk = false;
  TEST_ASSERT_EQUAL(BOOT_UNAVAILABLE, bootLikeThePanel(unreadable));
  TEST_ASSERT_EQUAL(0, unreadable.formats);
}

static void test_mount_failure_reads_the_superblock() {
  const std::vector<uint8_t> lfs = superblockHead(), zeros(16, 0);
  TEST_ASSERT_EQUAL(MOUNT_NO_FS, mountFailure(BLANK16.data(), BLANK16.data(), true));
  TEST_ASSERT_EQUAL(MOUNT_NO_FS, mountFailure(zeros.data(), zeros.data(), true));
  TEST_ASSERT_EQUAL(MOUNT_FAILED, mountFailure(lfs.data(), BLANK16.data(), true));
  TEST_ASSERT_EQUAL(MOUNT_FAILED, mountFailure(BLANK16.data(), lfs.data(), true));
  TEST_ASSERT_EQUAL(MOUNT_FAILED, mountFailure(lfs.data(), lfs.data(), true));
  TEST_ASSERT_EQUAL(MOUNT_FAILED, mountFailure(BLANK16.data(), BLANK16.data(), false));
  TEST_ASSERT_EQUAL(MOUNT_FAILED, mountFailure(nullptr, BLANK16.data(), true));
  TEST_ASSERT_EQUAL(MOUNT_FAILED, mountFailure(BLANK16.data(), nullptr, true));

  // only the exact name at byte 8 is a filesystem: another name, or the
  // name elsewhere (a GIF that mentions it) is not
  const std::vector<uint8_t> other = superblockHead("littlefz");
  TEST_ASSERT_EQUAL(MOUNT_NO_FS, mountFailure(other.data(), other.data(), true));
  std::vector<uint8_t> shifted(16, 0xFF);
  memcpy(shifted.data(), "littlefs", 8);
  TEST_ASSERT_EQUAL(MOUNT_NO_FS, mountFailure(shifted.data(), shifted.data(), true));
}

// --- Factory reset ----------------------------------------------------------

struct FakeReset : ResetOps {
  std::string log;
  std::string fail;   // the steps that fail: M I C S X
  bool step(char c) { log += c; return fail.find(c) == std::string::npos; }
  bool markPending() override { return step('M'); }
  bool eraseImages() override { return step('I'); }
  bool eraseCrashDump() override { return step('C'); }
  bool eraseSettings() override { return step('S'); }
  bool clearPending() override { return step('X'); }
};

static void test_reset_order() {
  FakeReset ops;
  TEST_ASSERT_TRUE(runFactoryReset(ops));
  TEST_ASSERT_EQUAL_STRING("MICSX", ops.log.c_str());
}

static void test_reset_stops_at_failure() {
  const char *steps = "MICSX";
  for (int i = 0; i < 5; i++) {
    FakeReset ops;
    ops.fail = steps[i];
    TEST_ASSERT_FALSE(runFactoryReset(ops));
    TEST_ASSERT_EQUAL_STRING(std::string(steps, i + 1).c_str(), ops.log.c_str());
  }
}

// The knob's reset. A marker that cannot be written leaves nothing for the
// next start to resume: the erases run anyway, so the owner's data goes now.
static void test_knob_reset_erases_without_marker() {
  FakeReset ok;
  TEST_ASSERT_TRUE(runFactoryResetNow(ok));
  TEST_ASSERT_EQUAL_STRING("MICSX", ok.log.c_str());

  FakeReset unmarked;
  unmarked.fail = "M";
  TEST_ASSERT_TRUE(runFactoryResetNow(unmarked));   // every erase read back blank
  TEST_ASSERT_EQUAL_STRING("MICSX", unmarked.log.c_str());

  // Without a marker, one erase that fails does not stop the others. The
  // image area refusing its marker is likely to refuse its erase too, and
  // the crash dump and the settings must still go.
  for (char c : std::string("ICS")) {
    FakeReset unmarkedFailing;
    unmarkedFailing.fail = std::string("M") + c;
    TEST_ASSERT_FALSE(runFactoryResetNow(unmarkedFailing));
    TEST_ASSERT_EQUAL_STRING("MICS", unmarkedFailing.log.c_str());
  }

  // with its marker, a failure stops the run as runFactoryReset() does
  const char *steps = "ICSX";
  for (int i = 0; i < 4; i++) {
    FakeReset marked;
    marked.fail = steps[i];
    TEST_ASSERT_FALSE(runFactoryResetNow(marked));
    TEST_ASSERT_EQUAL_STRING((std::string("M") + std::string(steps, i + 1)).c_str(),
                             marked.log.c_str());
  }
}

// How the knob's reset ends. Done, or failed after its marker (the next
// start resumes it): restart. Failed without a marker: nothing would resume
// it, so a restart would be a normal start on what is left of the previous
// owner's data; the panel stays locked on this start instead.
static void test_knob_reset_end() {
  TEST_ASSERT_EQUAL(KR_DONE, knobResetEnd(true, true));
  TEST_ASSERT_EQUAL(KR_DONE, knobResetEnd(true, false));
  TEST_ASSERT_EQUAL(KR_RESUME, knobResetEnd(false, true));
  TEST_ASSERT_EQUAL(KR_LOCKED, knobResetEnd(false, false));
}

static void test_reset_resume_is_the_same_run() {
  FakeReset first, resumed;
  runFactoryReset(first);
  TEST_ASSERT_EQUAL(RB_DONE, resetAtBoot(MARK_PRESENT, resumed));
  TEST_ASSERT_EQUAL_STRING(first.log.c_str(), resumed.log.c_str());
}

static void test_boot_without_marker_runs_nothing() {
  FakeReset ops;
  TEST_ASSERT_EQUAL(RB_NORMAL, resetAtBoot(MARK_ABSENT, ops));
  TEST_ASSERT_EQUAL_STRING("", ops.log.c_str());
}

static void test_boot_with_marker_resumes_or_locks() {
  for (MarkerState m : {MARK_PRESENT, MARK_UNREADABLE}) {
    FakeReset ok;
    TEST_ASSERT_EQUAL(RB_DONE, resetAtBoot(m, ok));
    TEST_ASSERT_EQUAL_STRING("MICSX", ok.log.c_str());
    for (char c : std::string("MICSX")) {
      FakeReset failing;
      failing.fail = c;
      TEST_ASSERT_EQUAL(RB_LOCKED, resetAtBoot(m, failing));
    }
  }
}

// A locked panel never mounts its image area, even when the marker reads
// blank this start (a marker step that failed half-way leaves it so).
static void test_locked_panel_never_mounts() {
  TEST_ASSERT_FALSE(resetPending(false, MARK_ABSENT));
  TEST_ASSERT_TRUE(resetPending(false, MARK_PRESENT));
  TEST_ASSERT_TRUE(resetPending(false, MARK_UNREADABLE));
  for (MarkerState m : {MARK_ABSENT, MARK_PRESENT, MARK_UNREADABLE}) {
    TEST_ASSERT_TRUE(resetPending(true, m));
    TEST_ASSERT_EQUAL(BOOT_LOCKED, storageBoot(true, resetPending(true, m), MOUNT_NOT_TRIED, false));
  }
}

static void test_blank_check() {
  std::vector<uint32_t> words(256, 0xFFFFFFFF);
  TEST_ASSERT_TRUE(blankFlash(words.data(), words.size()));
  TEST_ASSERT_TRUE(blankFlash(words.data(), 0));
  words[100] = 0xFFFFFFFE;   // one programmed bit
  TEST_ASSERT_FALSE(blankFlash(words.data(), words.size()));
  words[100] = 0xFFFFFFFF;
  words[255] = 0x7FFFFFFF;
  TEST_ASSERT_FALSE(blankFlash(words.data(), words.size()));
}

// The marker sector is also LittleFS's last block: only the 16-byte magic is a
// pending reset, only a read error is unreadable. Anything a file or LittleFS
// metadata leaves there is no reset, or a panel would wipe itself at boot.
static void test_marker_from_sector() {
  const uint32_t blank[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
  TEST_ASSERT_EQUAL(MARK_ABSENT, markerFrom(blank, true));
  TEST_ASSERT_EQUAL(MARK_PRESENT, markerFrom(RESET_MAGIC, true));
  TEST_ASSERT_EQUAL(MARK_UNREADABLE, markerFrom(RESET_MAGIC, false));
  TEST_ASSERT_EQUAL(MARK_UNREADABLE, markerFrom(blank, false));
  TEST_ASSERT_EQUAL(MARK_UNREADABLE, markerFrom(nullptr, true));

  // A metadata block: revision count, then tags; a CTZ block: pointers; a
  // file's first block: "GIF89a"; zeros: none of them is a reset
  const uint32_t metadata[4] = {0x00000007, 0x0FF00008, 0x6C697474, 0x65667320};
  const uint32_t pointers[4] = {0x0000005A, 0x00000059, 0x00000057, 0x00000053};
  const uint8_t gifBytes[16] = {'G', 'I', 'F', '8', '9', 'a', 64, 0, 32, 0, 0xF7, 0, 0, 0, 0, 0};
  uint32_t gifData[4];
  memcpy(gifData, gifBytes, sizeof(gifData));
  const uint32_t zeros[4] = {0, 0, 0, 0};
  for (const uint32_t *w : {metadata, pointers, (const uint32_t *)gifData, zeros})
    TEST_ASSERT_EQUAL(MARK_ABSENT, markerFrom(w, true));

  // The magic with any single bit changed, or only part of it, is no reset
  for (int bit = 0; bit < 128; bit++) {
    uint32_t w[4];
    memcpy(w, RESET_MAGIC, sizeof(w));
    w[bit / 32] ^= 1u << (bit % 32);
    TEST_ASSERT_EQUAL(MARK_ABSENT, markerFrom(w, true));
  }
  const uint32_t half[4] = {RESET_MAGIC[0], RESET_MAGIC[1], 0xFFFFFFFF, 0xFFFFFFFF};
  TEST_ASSERT_EQUAL(MARK_ABSENT, markerFrom(half, true));
}

static void test_marker_cannot_be_uploaded() {
  uint8_t bytes[16];
  memcpy(bytes, RESET_MAGIC, sizeof(bytes));   // as the flash holds it (little-endian)
  TEST_ASSERT_NOT_EQUAL(0, memcmp(bytes, "GIF8", 4));
  TEST_ASSERT_TRUE(RESET_MAGIC[0] > (1u << 24));
  TEST_ASSERT_FALSE(blankFlash(RESET_MAGIC, 4));
  TEST_ASSERT_NOT_EQUAL(KIND_GIF, sniffImage(bytes, sizeof(bytes)).kind);

  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  std::vector<uint8_t> forged(4096, 0);
  memcpy(forged.data(), bytes, sizeof(bytes));
  TEST_ASSERT_EQUAL(UP_NOT_GIF, upload(u, s, idx, "marker.gif", forged));
  TEST_ASSERT_EQUAL(0, s.writes);
  TEST_ASSERT_TRUE(s.files.empty());
}

// The marker sector's first 16 bytes. A write that fails (or a power cut
// after the erase) leaves the sector erased, as the flash would.
struct FakeSector : MarkerSector {
  uint32_t words[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
  bool readOk = true, eraseOk = true, writeOk = true;
  int erases = 0, writes = 0;
  bool read(uint32_t out[4]) override {
    memcpy(out, words, sizeof(words));
    return readOk;
  }
  bool erase() override {
    erases++;
    if (!eraseOk) return false;
    memset(words, 0xFF, sizeof(words));
    return true;
  }
  bool write(const uint32_t in[4]) override {
    writes++;
    if (!writeOk) return false;
    memcpy(words, in, sizeof(words));
    return true;
  }
};

static void test_mark_writes_and_reads_back() {
  FakeSector blank;
  TEST_ASSERT_TRUE(markResetPending(&blank));
  TEST_ASSERT_EQUAL(1, blank.erases);
  TEST_ASSERT_EQUAL(0, memcmp(blank.words, RESET_MAGIC, sizeof(RESET_MAGIC)));

  // whatever LittleFS left in its last block is erased first
  FakeSector used;
  const uint32_t metadata[4] = {0x00000007, 0x0FF00008, 0x6C697474, 0x65667320};
  memcpy(used.words, metadata, sizeof(metadata));
  TEST_ASSERT_TRUE(markResetPending(&used));
  TEST_ASSERT_EQUAL(0, memcmp(used.words, RESET_MAGIC, sizeof(RESET_MAGIC)));

  FakeSector noErase;
  noErase.eraseOk = false;
  TEST_ASSERT_FALSE(markResetPending(&noErase));
  TEST_ASSERT_EQUAL(0, noErase.writes);

  FakeSector noWrite;
  noWrite.writeOk = false;
  TEST_ASSERT_FALSE(markResetPending(&noWrite));

  FakeSector noRead;   // never read back: not taken as marked
  noRead.readOk = false;
  TEST_ASSERT_FALSE(markResetPending(&noRead));
}

// A resumed reset marks again: a marker already there is kept, never erased
// to be written again, or a failed write would leave none and the next start
// would run on the previous owner's settings.
static void test_mark_keeps_a_marker_already_there() {
  FakeSector marked;
  memcpy(marked.words, RESET_MAGIC, sizeof(RESET_MAGIC));
  marked.writeOk = false;
  TEST_ASSERT_TRUE(markResetPending(&marked));
  TEST_ASSERT_EQUAL(0, marked.erases);
  TEST_ASSERT_EQUAL(0, marked.writes);
  TEST_ASSERT_EQUAL(MARK_PRESENT, markerFrom(marked.words, true));
}

// A sector that cannot be read may hold the marker of a reset left
// unfinished: erasing it to write again, and the write failing, would leave
// none, and the next start would run on the previous owner's settings.
static void test_mark_unreadable_sector_is_not_erased() {
  FakeSector unreadable;
  memcpy(unreadable.words, RESET_MAGIC, sizeof(RESET_MAGIC));
  unreadable.readOk = false;
  unreadable.writeOk = false;
  TEST_ASSERT_FALSE(markResetPending(&unreadable));
  TEST_ASSERT_EQUAL(0, unreadable.erases);
  TEST_ASSERT_EQUAL(0, unreadable.writes);
  TEST_ASSERT_EQUAL(0, memcmp(unreadable.words, RESET_MAGIC, sizeof(RESET_MAGIC)));
}

// An older partition table has no littlefs partition, so nowhere to hold a
// marker: step 1 is not marked. Counted as marked, a failed erase would stop
// the knob's reset and restart it, and the next start, finding no marker,
// would be a normal one on the previous owner's Wi-Fi password and secrets.
static void test_mark_without_partition_is_not_marked() {
  TEST_ASSERT_FALSE(markResetPending(nullptr));

  // so every erase runs, and a failed one leaves the panel locked
  FakeReset oldTable;
  oldTable.fail = "MC";
  const bool done = runFactoryResetNow(oldTable);
  TEST_ASSERT_EQUAL_STRING("MICS", oldTable.log.c_str());
  TEST_ASSERT_EQUAL(KR_LOCKED, knobResetEnd(done, false));
}

static void test_upload_writes_after_signature() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  const std::vector<uint8_t> data = gif(64, 32, 300);
  TEST_ASSERT_EQUAL(UP_PENDING, u.start(s, idx, true, "a.gif", 0));
  for (size_t i = 0; i < 11; i++) {
    TEST_ASSERT_EQUAL(UP_PENDING, u.write(s, &data[i], 1));
    TEST_ASSERT_EQUAL(0, s.writes);
    TEST_ASSERT_FALSE(s.tempOpen);
  }
  TEST_ASSERT_EQUAL(UP_PENDING, u.write(s, &data[11], data.size() - 11));
  TEST_ASSERT_TRUE(s.writes > 0);
  TEST_ASSERT_TRUE(s.firstWrite.size() >= 4);
  TEST_ASSERT_EQUAL(0, memcmp(s.firstWrite.data(), "GIF8", 4));
  TEST_ASSERT_EQUAL(UP_ADDED, u.finish(s, idx));
  TEST_ASSERT_TRUE(s.files["a.gif"] == data);
}

// --- Names and headers --------------------------------------------------------

static void test_sanitize_names() {
  TEST_ASSERT_EQUAL_STRING("Ete-a-Paris-.gif", sanitized("Été à Paris!.GIF").c_str());
  TEST_ASSERT_EQUAL_STRING("-x.gif", sanitized("../x.gif").c_str());
  TEST_ASSERT_EQUAL_STRING("a-b.gif", sanitized("a/b.gif").c_str());
  TEST_ASSERT_EQUAL_STRING("a-b.gif", sanitized("a\\b.gif").c_str());
  TEST_ASSERT_EQUAL_STRING("a----.gif", sanitized("a\x01\t\x1f\x7f.gif").c_str());
  TEST_ASSERT_EQUAL_STRING("hidden.gif", sanitized(".hidden.gif").c_str());
  TEST_ASSERT_EQUAL_STRING("hidden.gif", sanitized("...hidden.gif").c_str());
  TEST_ASSERT_EQUAL_STRING("image.gif", sanitized("").c_str());
  TEST_ASSERT_EQUAL_STRING("image.gif", sanitized("...").c_str());
  TEST_ASSERT_EQUAL_STRING("image.gif", sanitized(".gif").c_str());
  TEST_ASSERT_EQUAL_STRING("image.gif", sanitized(nullptr).c_str());
  TEST_ASSERT_EQUAL_STRING("photo.png.gif", sanitized("photo.png").c_str());
  TEST_ASSERT_EQUAL_STRING("cat.gif", sanitized("cat.gif").c_str());

  const std::string longStem(52, 'x');   // 56 characters with ".gif"
  const std::string cut = sanitized((longStem + ".gif").c_str());
  TEST_ASSERT_EQUAL(40, cut.size());
  TEST_ASSERT_EQUAL_STRING((std::string(36, 'x') + ".gif").c_str(), cut.c_str());

  // Property: whatever is sent, a safe, stable name comes out. Each name is
  // an exact-size heap copy, so AddressSanitizer catches a read past its NUL.
  srand(7);
  for (int run = 0; run < 3000; run++) {
    const int len = rand() % 90;
    std::vector<char> in(len + 1, '\0');
    for (int i = 0; i < len; i++) in[i] = (char)(1 + rand() % 255);
    const std::string out = sanitized(in.data());
    TEST_ASSERT_TRUE(out.size() >= 5 && out.size() <= IMAGE_NAME_MAX);
    TEST_ASSERT_NOT_EQUAL('.', out[0]);
    TEST_ASSERT_EQUAL_STRING(".gif", out.c_str() + out.size() - 4);
    for (char c : out)
      TEST_ASSERT_TRUE(isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-');
    TEST_ASSERT_TRUE(storedNameValid(out.c_str()));
    // Idempotent: a stored name sent again is stored under the same name
    TEST_ASSERT_EQUAL_STRING(out.c_str(), sanitized(out.c_str()).c_str());
  }
}

static void test_leftover_names() {
  TEST_ASSERT_TRUE(isLeftover(".upload"));
  TEST_ASSERT_TRUE(isLeftover(UPLOAD_TEMP));
  TEST_ASSERT_FALSE(isLeftover("upload"));
  TEST_ASSERT_FALSE(isLeftover(".upload.gif"));
  TEST_ASSERT_FALSE(isLeftover("a.gif"));
  // a leftover is never listed as an image
  TEST_ASSERT_FALSE(storedNameValid(".upload"));
  TEST_ASSERT_TRUE(storedNameValid("cat.gif"));
  TEST_ASSERT_TRUE(storedNameValid("Cat_1.gif"));
  TEST_ASSERT_FALSE(storedNameValid("cat.GIF"));
  TEST_ASSERT_FALSE(storedNameValid("cat"));
  TEST_ASSERT_FALSE(storedNameValid("../x.gif"));
  TEST_ASSERT_FALSE(storedNameValid(""));
  TEST_ASSERT_FALSE(storedNameValid((std::string(37, 'x') + ".gif").c_str()));
}

static void test_sniff_types() {
  const uint8_t g87[] = {'G', 'I', 'F', '8', '7', 'a', 64, 0, 32, 0, 0, 0};
  GifHeader h = sniffImage(g87, sizeof(g87));
  TEST_ASSERT_EQUAL(KIND_GIF, h.kind);
  TEST_ASSERT_EQUAL(64, h.width);
  TEST_ASSERT_EQUAL(32, h.height);
  const uint8_t g89[] = {'G', 'I', 'F', '8', '9', 'a', 32, 0, 64, 0};
  h = sniffImage(g89, sizeof(g89));
  TEST_ASSERT_EQUAL(KIND_GIF, h.kind);
  TEST_ASSERT_EQUAL(32, h.width);
  TEST_ASSERT_EQUAL(64, h.height);
  TEST_ASSERT_EQUAL(KIND_OTHER, sniffImage(g89, 9).kind);   // header cut short
  const uint8_t g88[] = {'G', 'I', 'F', '8', '8', 'a', 64, 0, 32, 0};
  TEST_ASSERT_EQUAL(KIND_OTHER, sniffImage(g88, sizeof(g88)).kind);

  const uint8_t png[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13};
  TEST_ASSERT_EQUAL(KIND_PNG, sniffImage(png, sizeof(png)).kind);
  const uint8_t jpeg[] = {0xFF, 0xD8, 0xFF, 0xE0, 0, 16, 'J', 'F', 'I', 'F', 0, 1};
  TEST_ASSERT_EQUAL(KIND_JPEG, sniffImage(jpeg, sizeof(jpeg)).kind);
  const uint8_t webp[] = {'R', 'I', 'F', 'F', 0x24, 0, 0, 0, 'W', 'E', 'B', 'P'};
  TEST_ASSERT_EQUAL(KIND_WEBP, sniffImage(webp, sizeof(webp)).kind);
  TEST_ASSERT_EQUAL(KIND_OTHER, sniffImage(webp, 11).kind);
  const uint8_t wave[] = {'R', 'I', 'F', 'F', 0x24, 0, 0, 0, 'W', 'A', 'V', 'E'};
  TEST_ASSERT_EQUAL(KIND_OTHER, sniffImage(wave, sizeof(wave)).kind);
  const uint8_t junk[] = {0x12, 0x9C, 0x00, 0x47, 0xFE};
  h = sniffImage(junk, sizeof(junk));
  TEST_ASSERT_EQUAL(KIND_OTHER, h.kind);
  TEST_ASSERT_EQUAL(0, h.width);
  TEST_ASSERT_EQUAL(KIND_OTHER, sniffImage(nullptr, 0).kind);

  bool portrait = true;
  TEST_ASSERT_TRUE(gifSizeAccepted({KIND_GIF, 64, 32}, &portrait));
  TEST_ASSERT_FALSE(portrait);
  TEST_ASSERT_TRUE(gifSizeAccepted({KIND_GIF, 32, 64}, &portrait));
  TEST_ASSERT_TRUE(portrait);
  TEST_ASSERT_FALSE(gifSizeAccepted({KIND_GIF, 128, 64}, &portrait));
  TEST_ASSERT_FALSE(gifSizeAccepted({KIND_GIF, 64, 64}, &portrait));
  TEST_ASSERT_FALSE(gifSizeAccepted({KIND_PNG, 64, 32}, &portrait));
}

// --- Uploads ------------------------------------------------------------------

static void test_upload_landscape_added() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  const std::vector<uint8_t> data = gif(64, 32, 5000);
  TEST_ASSERT_EQUAL(UP_ADDED, upload(u, s, idx, "Pac Man.GIF", data));
  TEST_ASSERT_EQUAL_STRING("Pac-Man.gif", u.name());
  TEST_ASSERT_EQUAL_STRING("Pac Man.GIF", u.sent());
  TEST_ASSERT_EQUAL(5000, u.received());
  TEST_ASSERT_FALSE(u.portrait());
  TEST_ASSERT_TRUE(s.files["Pac-Man.gif"] == data);
  TEST_ASSERT_TRUE(s.noTemp());
  TEST_ASSERT_EQUAL(1, idx.count);
  TEST_ASSERT_EQUAL_STRING("Pac-Man.gif", idx.items[0].name);
  TEST_ASSERT_EQUAL(5000, idx.items[0].size);
  TEST_ASSERT_FALSE(idx.items[0].portrait);

  TEST_ASSERT_EQUAL(UP_ADDED, upload(u, s, idx, "tall.gif", gif(32, 64, 700)));
  TEST_ASSERT_TRUE(u.portrait());
  TEST_ASSERT_TRUE(idx.items[idx.find("tall.gif")].portrait);
}

static void test_upload_streams_chunks() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  const std::vector<uint8_t> data = gif(64, 32, MAX_IMAGE_BYTES);   // exactly the cap
  TEST_ASSERT_EQUAL(UP_ADDED, upload(u, s, idx, "big.gif", data));
  TEST_ASSERT_TRUE(s.maxChunk <= CHUNK);
  TEST_ASSERT_TRUE(s.writes >= (int)(MAX_IMAGE_BYTES / CHUNK));
  TEST_ASSERT_TRUE(s.files["big.gif"] == data);
  // the transaction itself holds a header copy, never a chunk
  TEST_ASSERT_TRUE(sizeof(ImageUpload) < 200);
}

static void test_upload_never_started_writes_nothing() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;   // the gate refused: start() never ran
  const std::vector<uint8_t> data = gif(64, 32, 3000);
  TEST_ASSERT_NOT_EQUAL(UP_PENDING, u.write(s, data.data(), data.size()));
  TEST_ASSERT_NOT_EQUAL(UP_ADDED, u.finish(s, idx));
  TEST_ASSERT_EQUAL(0, s.writes);
  TEST_ASSERT_FALSE(s.tempOpen);
  TEST_ASSERT_EQUAL(0, idx.count);
}

static void test_upload_no_file_or_storage() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  TEST_ASSERT_EQUAL(UP_NO_STORAGE, u.start(s, idx, false, "a.gif", 100));
  TEST_ASSERT_EQUAL(UP_NO_FILE, u.start(s, idx, true, "", 100));
  TEST_ASSERT_EQUAL(UP_NO_FILE, u.start(s, idx, true, nullptr, 100));
  TEST_ASSERT_EQUAL(UP_NO_FILE, upload(u, s, idx, "a.gif", {}));
  // shorter than a GIF header: refused, nothing kept
  TEST_ASSERT_EQUAL(UP_NOT_GIF, upload(u, s, idx, "a.gif", gif(64, 32, 10)));
  TEST_ASSERT_EQUAL(0, s.writes);
  TEST_ASSERT_TRUE(s.files.empty());
}

static void test_upload_announced_over_cap() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  TEST_ASSERT_EQUAL(UP_TOO_BIG, u.start(s, idx, true, "a.gif", MAX_IMAGE_BYTES + MULTIPART_SLACK + 1));
  TEST_ASSERT_EQUAL(0, s.writes);
  TEST_ASSERT_FALSE(s.tempOpen);
  TEST_ASSERT_EQUAL(UP_PENDING, u.start(s, idx, true, "a.gif", MAX_IMAGE_BYTES + MULTIPART_SLACK));
}

static void test_upload_counted_over_cap() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  const std::vector<uint8_t> data = gif(64, 32, MAX_IMAGE_BYTES + 1);
  TEST_ASSERT_EQUAL(UP_PENDING, u.start(s, idx, true, "a.gif", 0));
  UploadResult r = UP_PENDING;
  size_t i = 0;
  for (; i < data.size() && r == UP_PENDING; i += CHUNK)
    r = u.write(s, data.data() + i, std::min(CHUNK, data.size() - i));
  TEST_ASSERT_EQUAL(UP_TOO_BIG, r);
  TEST_ASSERT_TRUE(s.noTemp());   // dropped at once
  TEST_ASSERT_EQUAL(UP_TOO_BIG, u.finish(s, idx));
  TEST_ASSERT_TRUE(s.files.empty());
  TEST_ASSERT_EQUAL(0, idx.count);
}

static void test_upload_not_gif_keeps_nothing() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  std::vector<uint8_t> png(3000, 0);
  const uint8_t sig[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  memcpy(png.data(), sig, sizeof(sig));
  TEST_ASSERT_EQUAL(UP_NOT_GIF, upload(u, s, idx, "photo-plage.png", png));
  TEST_ASSERT_EQUAL(KIND_PNG, u.header().kind);
  TEST_ASSERT_EQUAL_STRING("photo-plage.png", u.sent());
  TEST_ASSERT_EQUAL(3000, u.received());
  TEST_ASSERT_EQUAL(0, s.writes);
  TEST_ASSERT_TRUE(s.noTemp());
  TEST_ASSERT_TRUE(s.files.empty());
}

static void test_upload_wrong_size() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  TEST_ASSERT_EQUAL(UP_WRONG_SIZE, upload(u, s, idx, "big.gif", gif(128, 64, 2000)));
  TEST_ASSERT_EQUAL(KIND_GIF, u.header().kind);
  TEST_ASSERT_EQUAL(128, u.header().width);
  TEST_ASSERT_EQUAL(64, u.header().height);
  TEST_ASSERT_EQUAL(0, s.writes);
  TEST_ASSERT_TRUE(s.files.empty());
}

static void test_upload_no_room_figures() {
  FakeStore s;
  s.free = RESERVE_BLOCKS + 2;   // two blocks: 8 188 bytes for one file
  ImageIndex idx;
  ImageUpload u;
  TEST_ASSERT_EQUAL(UP_NO_ROOM, upload(u, s, idx, "a.gif", gif(64, 32, 41 * 1024)));
  TEST_ASSERT_EQUAL(8188, u.left());
  TEST_ASSERT_EQUAL(41 * 1024, u.received());   // what it needs: every byte
  TEST_ASSERT_TRUE(s.noTemp());
  TEST_ASSERT_TRUE(s.files.empty());
  TEST_ASSERT_EQUAL(0, idx.count);
}

static void test_reserve_kept() {
  ImageIndex idx;
  for (uint32_t free = 0; free <= RESERVE_BLOCKS; free++)
    TEST_ASSERT_EQUAL(0, spaceFigures(idx, free, 4096).left);
  TEST_ASSERT_EQUAL(4096, spaceFigures(idx, RESERVE_BLOCKS + 1, 4096).left);

  FakeStore s;
  ImageUpload u;
  s.free = RESERVE_BLOCKS;
  TEST_ASSERT_EQUAL(UP_NO_ROOM, upload(u, s, idx, "a.gif", gif(64, 32, 100)));
  s.free = RESERVE_BLOCKS + 1;
  TEST_ASSERT_EQUAL(UP_NO_ROOM, upload(u, s, idx, "a.gif", gif(64, 32, 4097)));
  TEST_ASSERT_EQUAL(UP_ADDED, upload(u, s, idx, "a.gif", gif(64, 32, 4096)));
}

static void test_upload_25th_refused_replace_allowed() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  for (int i = 0; i < MAX_IMAGES; i++) {
    char name[16];
    snprintf(name, sizeof(name), "img%02d.gif", i);
    TEST_ASSERT_EQUAL(UP_ADDED, upload(u, s, idx, name, gif(64, 32, 200)));
  }
  TEST_ASSERT_EQUAL(MAX_IMAGES, idx.count);
  const int writes = s.writes;
  TEST_ASSERT_EQUAL(UP_TOO_MANY, upload(u, s, idx, "new.gif", gif(64, 32, 200)));
  TEST_ASSERT_EQUAL(writes, s.writes);
  TEST_ASSERT_TRUE(s.noTemp());
  TEST_ASSERT_EQUAL(UP_REPLACED, upload(u, s, idx, "img07.gif", gif(64, 32, 300)));
  TEST_ASSERT_EQUAL(MAX_IMAGES, idx.count);
}

static void test_upload_refused_drops_temp() {
  // refused half-way after writing: the temp goes at once, the old file stays
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  const std::vector<uint8_t> old = gif(64, 32, 500, 0x11);
  TEST_ASSERT_EQUAL(UP_ADDED, upload(u, s, idx, "a.gif", old));

  TEST_ASSERT_EQUAL(UP_PENDING, u.start(s, idx, true, "a.gif", 0));
  const std::vector<uint8_t> data = gif(64, 32, 3 * CHUNK);
  TEST_ASSERT_EQUAL(UP_PENDING, u.write(s, data.data(), CHUNK));
  TEST_ASSERT_TRUE(s.tempOpen);
  s.failWrite = true;
  TEST_ASSERT_EQUAL(UP_FAILED, u.write(s, data.data() + CHUNK, CHUNK));
  TEST_ASSERT_TRUE(s.noTemp());
  s.failWrite = false;
  const int writes = s.writes;
  TEST_ASSERT_EQUAL(UP_FAILED, u.write(s, data.data() + 2 * CHUNK, CHUNK));   // only counted
  TEST_ASSERT_EQUAL(writes, s.writes);
  TEST_ASSERT_EQUAL(3 * CHUNK, u.received());
  TEST_ASSERT_EQUAL(UP_FAILED, u.finish(s, idx));
  TEST_ASSERT_TRUE(s.files["a.gif"] == old);
  TEST_ASSERT_EQUAL(500, idx.items[0].size);

  s.failBegin = true;
  TEST_ASSERT_EQUAL(UP_FAILED, upload(u, s, idx, "b.gif", gif(64, 32, 900)));
  s.failBegin = false;
  TEST_ASSERT_TRUE(s.noTemp());
  TEST_ASSERT_EQUAL(1, idx.count);
}

static void test_upload_abort_drops_temp() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  const std::vector<uint8_t> old = gif(64, 32, 500, 0x11);
  TEST_ASSERT_EQUAL(UP_ADDED, upload(u, s, idx, "a.gif", old));
  const uint32_t gen = idx.generation;
  const std::vector<uint8_t> data = gif(64, 32, 4 * CHUNK);
  TEST_ASSERT_EQUAL(UP_PENDING, u.start(s, idx, true, "a.gif", 0));
  u.write(s, data.data(), CHUNK);
  u.write(s, data.data() + CHUNK, CHUNK);
  u.abort(s);
  TEST_ASSERT_TRUE(s.noTemp());
  TEST_ASSERT_NOT_EQUAL(UP_ADDED, u.finish(s, idx));
  TEST_ASSERT_NOT_EQUAL(UP_REPLACED, u.finish(s, idx));
  TEST_ASSERT_TRUE(s.files["a.gif"] == old);
  TEST_ASSERT_EQUAL(gen, idx.generation);
}

static void test_replace_atomic() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  const std::vector<uint8_t> old = gif(64, 32, 6000, 0x11);
  TEST_ASSERT_EQUAL(UP_ADDED, upload(u, s, idx, "a.gif", old));
  uint32_t gen = idx.generation;

  // every way a replacement can fail leaves the old file and its entry
  TEST_ASSERT_EQUAL(UP_WRONG_SIZE, upload(u, s, idx, "a.gif", gif(64, 64, 3000)));
  TEST_ASSERT_EQUAL(UP_TOO_BIG, upload(u, s, idx, "a.gif", gif(64, 32, MAX_IMAGE_BYTES + 10)));
  s.failCommit = true;
  TEST_ASSERT_EQUAL(UP_FAILED, upload(u, s, idx, "a.gif", gif(64, 32, 3000)));
  s.failCommit = false;
  // the new file needs room of its own: the old one is not counted as free
  s.free = RESERVE_BLOCKS + 1;
  TEST_ASSERT_EQUAL(UP_NO_ROOM, upload(u, s, idx, "a.gif", gif(64, 32, 5000)));
  s.free = 90;
  TEST_ASSERT_TRUE(s.files["a.gif"] == old);
  TEST_ASSERT_TRUE(s.noTemp());
  TEST_ASSERT_EQUAL(6000, idx.items[0].size);
  TEST_ASSERT_EQUAL(gen, idx.generation);

  const std::vector<uint8_t> fresh = gif(32, 64, 3000, 0x22);
  TEST_ASSERT_EQUAL(UP_REPLACED, upload(u, s, idx, "a.gif", fresh));
  TEST_ASSERT_TRUE(s.files["a.gif"] == fresh);
  TEST_ASSERT_EQUAL(1, idx.count);
  TEST_ASSERT_EQUAL(3000, idx.items[0].size);
  TEST_ASSERT_TRUE(idx.items[0].portrait);
  TEST_ASSERT_NOT_EQUAL(gen, idx.generation);
}

static void test_sent_name_cut() {
  FakeStore s;
  ImageIndex idx;
  ImageUpload u;
  const std::string longName(100, 'n');
  u.start(s, idx, true, longName.c_str(), 0);
  TEST_ASSERT_EQUAL(60, strlen(u.sent()));
  // a two-byte character across byte 60 is not cut in half
  const std::string accented = std::string(59, 'n') + "é.gif";
  u.start(s, idx, true, accented.c_str(), 0);
  TEST_ASSERT_EQUAL(59, strlen(u.sent()));
  u.abort(s);
}

static void test_status_codes() {
  TEST_ASSERT_EQUAL(200, uploadStatus(UP_ADDED));
  TEST_ASSERT_EQUAL(200, uploadStatus(UP_REPLACED));
  TEST_ASSERT_EQUAL(400, uploadStatus(UP_PENDING));   // the request ended mid-file
  TEST_ASSERT_EQUAL(400, uploadStatus(UP_NO_FILE));
  TEST_ASSERT_EQUAL(503, uploadStatus(UP_NO_STORAGE));
  TEST_ASSERT_EQUAL(413, uploadStatus(UP_TOO_BIG));
  TEST_ASSERT_EQUAL(400, uploadStatus(UP_NOT_GIF));
  TEST_ASSERT_EQUAL(400, uploadStatus(UP_WRONG_SIZE));
  TEST_ASSERT_EQUAL(507, uploadStatus(UP_NO_ROOM));
  TEST_ASSERT_EQUAL(507, uploadStatus(UP_TOO_MANY));
  TEST_ASSERT_EQUAL(500, uploadStatus(UP_FAILED));
}

// The page's answer when the Images page cannot be built: the upload's own
// result in plain text, never "busy" after a stored file
static void test_upload_texts() {
  TEST_ASSERT_EQUAL_STRING("image added", uploadText(UP_ADDED));
  TEST_ASSERT_EQUAL_STRING("image replaced", uploadText(UP_REPLACED));
  for (UploadResult r : {UP_PENDING, UP_NO_FILE, UP_NO_STORAGE, UP_TOO_BIG, UP_NOT_GIF,
                         UP_WRONG_SIZE, UP_NO_ROOM, UP_TOO_MANY, UP_FAILED})
    TEST_ASSERT_EQUAL_STRING("image not added", uploadText(r));
}

// --- Space and index ----------------------------------------------------------

static void test_space_figures() {
  TEST_ASSERT_EQUAL(0, ctzCapacity(0, 4096));
  TEST_ASSERT_EQUAL(4096, ctzCapacity(1, 4096));
  TEST_ASSERT_EQUAL(8188, ctzCapacity(2, 4096));
  TEST_ASSERT_EQUAL(12276, ctzCapacity(3, 4096));
  TEST_ASSERT_EQUAL(5 * 4096 - 4 * (2 * 4 - 1), ctzCapacity(5, 4096));

  ImageIndex idx;
  Space sp = spaceFigures(idx, 90, 4096);
  TEST_ASSERT_EQUAL(0, sp.used);
  TEST_ASSERT_EQUAL(ctzCapacity(90 - RESERVE_BLOCKS, 4096), sp.left);
  TEST_ASSERT_EQUAL(sp.left, sp.total);

  idx.put(info("a.gif", 100));
  idx.put(info("b.gif", 5000));
  sp = spaceFigures(idx, 50, 4096);
  TEST_ASSERT_EQUAL(5100, sp.used);
  TEST_ASSERT_EQUAL(ctzCapacity(46, 4096), sp.left);
  TEST_ASSERT_EQUAL(sp.used + sp.left, sp.total);

  TEST_ASSERT_EQUAL(0, kbUp(0));
  TEST_ASSERT_EQUAL(1, kbUp(100));
  TEST_ASSERT_EQUAL(1, kbUp(1024));
  TEST_ASSERT_EQUAL(2, kbUp(1025));
  TEST_ASSERT_EQUAL(4194304, kbUp(0xFFFFFFFF));
  TEST_ASSERT_EQUAL(0, kbDown(1023));
  TEST_ASSERT_EQUAL(2, kbDown(2048));
  TEST_ASSERT_EQUAL(2, kbDown(3071));
}

static void test_index_order() {
  ImageIndex idx;
  for (const char *n : {"b.gif", "A.gif", "a.gif", "C.gif", "2.gif", "10.gif"})
    TEST_ASSERT_TRUE(idx.put(info(n)));
  const char *expected[] = {"10.gif", "2.gif", "A.gif", "a.gif", "b.gif", "C.gif"};
  TEST_ASSERT_EQUAL(6, idx.count);
  for (int i = 0; i < 6; i++) TEST_ASSERT_EQUAL_STRING(expected[i], idx.items[i].name);
  TEST_ASSERT_TRUE(imageOrder("A.gif", "a.gif") < 0);
  TEST_ASSERT_TRUE(imageOrder("a.gif", "B.gif") < 0);
  TEST_ASSERT_EQUAL(0, imageOrder("a.gif", "a.gif"));
  TEST_ASSERT_EQUAL(-1, idx.find("B.gif"));   // names are case-sensitive
  TEST_ASSERT_EQUAL(4, idx.find("b.gif"));

  for (int i = 0; idx.count < MAX_IMAGES; i++) {
    char n[16];
    snprintf(n, sizeof(n), "z%02d.gif", i);
    idx.put(info(n));
  }
  const uint32_t gen = idx.generation;
  TEST_ASSERT_FALSE(idx.put(info("new.gif")));
  TEST_ASSERT_EQUAL(gen, idx.generation);
  TEST_ASSERT_TRUE(idx.put(info("a.gif", 77)));
  TEST_ASSERT_EQUAL(77, idx.items[idx.find("a.gif")].size);
  TEST_ASSERT_EQUAL(MAX_IMAGES, idx.count);
}

// /img as the walk sees it: entries in order, the bytes of each file, the
// opens that fail and where the walk itself fails
struct FakeDir : ImageDir {
  struct Entry { std::string name; bool dir; std::vector<uint8_t> bytes; bool openFails; };
  std::vector<Entry> entries;
  size_t at = 0, failAt = SIZE_MAX;
  std::vector<std::string> notes;
  int next(const char **name, bool *isDir) override {
    if (at == failAt) return -1;
    if (at >= entries.size()) return 0;
    *name = entries[at].name.c_str();
    *isDir = entries[at].dir;
    at++;
    return 1;
  }
  bool head(const char *name, uint32_t *size, uint8_t *b, size_t *n) override {
    for (const Entry &e : entries) {
      if (e.name != name) continue;
      if (e.openFails) return false;
      *size = (uint32_t)e.bytes.size();
      *n = std::min(e.bytes.size(), (size_t)12);
      memcpy(b, e.bytes.data(), *n);
      return true;
    }
    return false;
  }
  void note(const char *name, IndexNote why) override {
    notes.push_back(std::to_string((int)why) + ":" + name);
  }
};

static void test_scan_lists_images() {
  FakeDir d;
  d.entries = {{"b.gif", false, gif(64, 32, 900), false},
               {"sub", true, {}, false},
               {".upload", false, gif(64, 32, 50), false},
               {"bad name.gif", false, gif(64, 32, 50), false},
               {"a.gif", false, gif(32, 64, 700), false},
               {"cut.gif", false, {'G', 'I', 'F'}, false}};
  ImageIndex idx;
  TEST_ASSERT_TRUE(scanImages(d, idx));
  TEST_ASSERT_EQUAL(3, idx.count);
  TEST_ASSERT_EQUAL_STRING("a.gif", idx.items[0].name);
  TEST_ASSERT_TRUE(idx.items[0].portrait);
  TEST_ASSERT_EQUAL(700, idx.items[0].size);
  TEST_ASSERT_EQUAL_STRING("b.gif", idx.items[1].name);
  TEST_ASSERT_FALSE(idx.items[1].portrait);
  TEST_ASSERT_EQUAL_STRING("cut.gif", idx.items[2].name);   // unreadable header: landscape
  TEST_ASSERT_FALSE(idx.items[2].portrait);
  TEST_ASSERT_EQUAL(3, (int)d.notes.size());               // sub, .upload, "bad name.gif"
}

// A file that cannot be opened (no memory, a read error) is still on the
// flash: the listing is incomplete, so the saved choice must stay, and the
// walk goes on with the next entries.
static void test_scan_incomplete_when_a_file_cannot_be_opened() {
  FakeDir d;
  d.entries = {{"a.gif", false, gif(64, 32, 500), false},
               {"chosen.gif", false, gif(64, 32, 500), true},
               {"c.gif", false, gif(64, 32, 500), false}};
  ImageIndex idx;
  TEST_ASSERT_FALSE(scanImages(d, idx));
  TEST_ASSERT_EQUAL(2, idx.count);
  TEST_ASSERT_EQUAL_STRING("c.gif", idx.items[1].name);
  TEST_ASSERT_EQUAL(1, (int)d.notes.size());

  fakeNvs["image"] = "chosen.gif";
  ImageChoice choice;
  choice.load();
  FakeDir again = d;
  again.at = 0;
  ImageIndex fresh;
  if (scanImages(again, fresh)) choice.validate(fresh);   // what contentBegin() does
  TEST_ASSERT_EQUAL_STRING("chosen.gif", choice.get());
  TEST_ASSERT_EQUAL_STRING("chosen.gif", fakeNvs["image"].c_str());
}

static void test_scan_incomplete_when_the_walk_fails() {
  FakeDir d;
  d.entries = {{"a.gif", false, gif(64, 32, 500), false},
               {"b.gif", false, gif(64, 32, 500), false}};
  d.failAt = 1;
  ImageIndex idx;
  TEST_ASSERT_FALSE(scanImages(d, idx));
  TEST_ASSERT_EQUAL(1, idx.count);
}

// The scan counts the same files again at every start. From 0 each time, a
// restart after a replace would bring back the address the Images page gave
// the old file, which the browser keeps a year: each start draws its own.
static void test_generation_new_after_restart() {
  FakeDir d;
  d.entries = {{"a.gif", false, gif(64, 32, 500), false},
               {"b.gif", false, gif(64, 32, 500), false},
               {"c.gif", false, gif(64, 32, 500), false}};
  ImageIndex first(0x7fffff00u);
  TEST_ASSERT_TRUE(scanImages(d, first));
  const uint32_t shown = first.generation;     // a.gif's address before the replace
  TEST_ASSERT_TRUE(first.put(info("a.gif", 900)));
  FakeDir again = d;
  again.at = 0;
  ImageIndex next(0x12345678u);
  TEST_ASSERT_TRUE(scanImages(again, next));
  TEST_ASSERT_NOT_EQUAL(shown, next.generation);
  TEST_ASSERT_EQUAL_UINT32(0x12345678u + 3, next.generation);
  // the counter wraps: a start drawn at the top still moves on every change
  ImageIndex top(0xffffffffu);
  TEST_ASSERT_TRUE(top.put(info("a.gif")));
  TEST_ASSERT_EQUAL_UINT32(0, top.generation);
}

static void test_scan_incomplete_over_the_cap() {
  FakeDir d;
  for (int i = 0; i <= MAX_IMAGES; i++) {
    char n[16];
    snprintf(n, sizeof(n), "i%02d.gif", i);
    d.entries.push_back({n, false, gif(64, 32, 300), false});
  }
  ImageIndex idx;
  TEST_ASSERT_FALSE(scanImages(d, idx));
  TEST_ASSERT_EQUAL(MAX_IMAGES, idx.count);
}

static void test_delete_resets_choice_and_bumps_generation() {
  ImageIndex idx;
  idx.put(info("a.gif"));
  idx.put(info("b.gif"));
  ImageChoice choice;
  choice.set("b.gif");
  const int writes = nvsWriteCount;
  const uint32_t gen = idx.generation;
  TEST_ASSERT_TRUE(idx.erase("b.gif"));
  TEST_ASSERT_EQUAL(gen + 1, idx.generation);
  TEST_ASSERT_EQUAL(1, idx.count);
  choice.validate(idx);
  TEST_ASSERT_EQUAL_STRING("", choice.get());
  TEST_ASSERT_EQUAL(writes + 1, nvsWriteCount);
  choice.validate(idx);
  TEST_ASSERT_EQUAL(writes + 1, nvsWriteCount);
}

static void test_delete_unknown() {
  ImageIndex idx;
  idx.put(info("a.gif"));
  const uint32_t gen = idx.generation;
  TEST_ASSERT_FALSE(idx.erase("zz.gif"));
  TEST_ASSERT_FALSE(idx.erase(""));
  TEST_ASSERT_EQUAL(1, idx.count);
  TEST_ASSERT_EQUAL(gen, idx.generation);
}

// --- Choice -------------------------------------------------------------------

static void test_choice_writes_only_on_change() {
  ImageChoice c;
  c.load();
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
  TEST_ASSERT_TRUE(c.set("a.gif"));
  TEST_ASSERT_EQUAL(1, nvsWriteCount);
  TEST_ASSERT_TRUE(c.set("a.gif"));
  TEST_ASSERT_EQUAL(1, nvsWriteCount);
  TEST_ASSERT_FALSE(c.set("../x.gif"));
  TEST_ASSERT_EQUAL(1, nvsWriteCount);
  TEST_ASSERT_EQUAL_STRING("a.gif", c.get());

  ImageChoice again;
  again.load();
  TEST_ASSERT_EQUAL_STRING("a.gif", again.get());

  TEST_ASSERT_TRUE(c.set(""));
  TEST_ASSERT_EQUAL(2, nvsWriteCount);
  TEST_ASSERT_TRUE(c.set(nullptr));
  TEST_ASSERT_TRUE(c.set(""));
  TEST_ASSERT_EQUAL(2, nvsWriteCount);
  TEST_ASSERT_EQUAL_STRING("", c.get());
}

static void test_choice_stale_or_absent() {
  fakeNvs["bright"] = "7";   // another setting, untouched
  ImageChoice c;
  c.load();
  TEST_ASSERT_EQUAL_STRING("", c.get());

  ImageIndex idx;
  idx.put(info("kept.gif"));
  fakeNvs["image"] = "gone.gif";
  c.load();
  TEST_ASSERT_EQUAL_STRING("gone.gif", c.get());
  c.validate(idx);
  TEST_ASSERT_EQUAL_STRING("", c.get());
  TEST_ASSERT_EQUAL(1, nvsWriteCount);
  c.load();
  TEST_ASSERT_EQUAL_STRING("", c.get());

  fakeNvs["image"] = "kept.gif";
  c.load();
  c.validate(idx);
  TEST_ASSERT_EQUAL_STRING("kept.gif", c.get());
  TEST_ASSERT_EQUAL(1, nvsWriteCount);

  fakeNvs["image"] = "../../etc";   // never a valid name: all in turn
  c.load();
  TEST_ASSERT_EQUAL_STRING("", c.get());
  TEST_ASSERT_EQUAL_STRING("7", fakeNvs["bright"].c_str());
}

// A refused NVS write keeps the old choice: the panel shows what is saved.
// NVS closed by the lock keeps the choice in RAM only, by design; NVS that
// cannot be opened for any other reason is a refusal like any other.
static void test_choice_write_refused() {
  ImageChoice c;
  TEST_ASSERT_TRUE(c.set("a.gif"));
  nvsFailWrites = true;
  TEST_ASSERT_FALSE(c.set("b.gif"));
  TEST_ASSERT_EQUAL_STRING("a.gif", c.get());
  TEST_ASSERT_FALSE(c.set(""));
  TEST_ASSERT_EQUAL_STRING("a.gif", c.get());
  TEST_ASSERT_EQUAL_STRING("a.gif", fakeNvs["image"].c_str());
  ImageIndex idx;
  c.validate(idx);   // stale, the removal refused: kept, tried again later
  TEST_ASSERT_EQUAL_STRING("a.gif", c.get());

  nvsFailWrites = false;
  nvsFailBegin = true;
  TEST_ASSERT_FALSE(c.set("b.gif"));
  TEST_ASSERT_EQUAL_STRING("a.gif", c.get());
  fakeLocked = true;
  TEST_ASSERT_TRUE(c.set("b.gif"));
  TEST_ASSERT_EQUAL_STRING("b.gif", c.get());
  TEST_ASSERT_EQUAL_STRING("a.gif", fakeNvs["image"].c_str());
}

// Removing a key that is already gone is no failure
static void test_choice_remove_absent_key() {
  ImageChoice c;
  TEST_ASSERT_TRUE(c.set("a.gif"));
  fakeNvs.erase("image");
  TEST_ASSERT_TRUE(c.set(""));
  TEST_ASSERT_EQUAL_STRING("", c.get());
}

static void test_choice_unknown_ignored() {
  ImageIndex idx;
  idx.put(info("a.gif"));
  TEST_ASSERT_FALSE(choiceValid(idx, "nope.gif"));
  TEST_ASSERT_FALSE(choiceValid(idx, "A.gif"));
  TEST_ASSERT_TRUE(choiceValid(idx, "a.gif"));
  TEST_ASSERT_TRUE(choiceValid(idx, ""));
  TEST_ASSERT_TRUE(choiceValid(idx, nullptr));
}

// --- What the IMAGE screen plays ------------------------------------------------

static ImageIndex sample() {   // a b c landscape, p portrait
  ImageIndex idx;
  idx.put(info("c.gif"));
  idx.put(info("a.gif"));
  idx.put(info("p.gif", 1000, true));
  idx.put(info("b.gif"));
  return idx;
}
static const char *picked(const ImageIndex &idx, ImagePick p) {
  return p.state == IMG_PLAY && p.index >= 0 ? idx.items[p.index].name : "-";
}

static void test_pick_no_storage() {
  const ImageIndex idx = sample();
  TEST_ASSERT_EQUAL(IMG_NO_STORAGE, pickImage(idx, false, false, "", nullptr, nullptr, nullptr).state);
  TEST_ASSERT_EQUAL(IMG_NO_STORAGE, pickImage(ImageIndex(), false, true, "", nullptr, nullptr, nullptr).state);
}

static void test_pick_states() {
  ImageIndex empty;
  ImagePick p = pickImage(empty, true, false, "", nullptr, nullptr, nullptr);
  TEST_ASSERT_EQUAL(IMG_NO_IMAGE, p.state);
  TEST_ASSERT_EQUAL(-1, p.index);
  TEST_ASSERT_EQUAL(IMG_NO_IMAGE, pickImage(empty, true, true, "", nullptr, nullptr, nullptr).state);

  ImageIndex wide;
  wide.put(info("a.gif"));
  TEST_ASSERT_EQUAL(IMG_OTHER_ONLY, pickImage(wide, true, true, "", nullptr, nullptr, nullptr).state);
  p = pickImage(wide, true, false, "", nullptr, nullptr, nullptr);
  TEST_ASSERT_EQUAL(IMG_PLAY, p.state);
  TEST_ASSERT_EQUAL(0, p.index);
}

static void test_pick_in_turn() {
  const ImageIndex idx = sample();
  TEST_ASSERT_EQUAL_STRING("a.gif", picked(idx, pickImage(idx, true, false, "", nullptr, nullptr, nullptr)));
  TEST_ASSERT_EQUAL_STRING("b.gif", picked(idx, pickImage(idx, true, false, "", nullptr, "a.gif", nullptr)));
  TEST_ASSERT_EQUAL_STRING("c.gif", picked(idx, pickImage(idx, true, false, nullptr, nullptr, "b.gif", nullptr)));
  TEST_ASSERT_EQUAL_STRING("a.gif", picked(idx, pickImage(idx, true, false, "", nullptr, "c.gif", nullptr)));
  // after a name that was deleted meanwhile: the next one in order
  TEST_ASSERT_EQUAL_STRING("c.gif", picked(idx, pickImage(idx, true, false, "", nullptr, "bb.gif", nullptr)));
  TEST_ASSERT_EQUAL_STRING("a.gif", picked(idx, pickImage(idx, true, false, "", nullptr, "zz.gif", nullptr)));
  // portrait: only its own
  TEST_ASSERT_EQUAL_STRING("p.gif", picked(idx, pickImage(idx, true, true, "", nullptr, "p.gif", nullptr)));
}

static void test_pick_choice() {
  const ImageIndex idx = sample();
  for (const char *after : {"a.gif", "b.gif", "c.gif"})
    TEST_ASSERT_EQUAL_STRING("b.gif", picked(idx, pickImage(idx, true, false, "b.gif", nullptr, after, nullptr)));
  // chosen for the other orientation: this orientation's images in turn
  TEST_ASSERT_EQUAL_STRING("b.gif", picked(idx, pickImage(idx, true, false, "p.gif", nullptr, "a.gif", nullptr)));
  TEST_ASSERT_EQUAL_STRING("p.gif", picked(idx, pickImage(idx, true, true, "p.gif", nullptr, nullptr, nullptr)));
  // a stale choice: in turn
  TEST_ASSERT_EQUAL_STRING("c.gif", picked(idx, pickImage(idx, true, false, "gone.gif", nullptr, "b.gif", nullptr)));
}

static void test_pick_first() {
  const ImageIndex idx = sample();
  TEST_ASSERT_EQUAL_STRING("c.gif", picked(idx, pickImage(idx, true, false, "a.gif", "c.gif", "a.gif", nullptr)));
  // drawn for the other orientation, or gone: ignored
  TEST_ASSERT_EQUAL_STRING("a.gif", picked(idx, pickImage(idx, true, false, "a.gif", "p.gif", nullptr, nullptr)));
  TEST_ASSERT_EQUAL_STRING("a.gif", picked(idx, pickImage(idx, true, false, "a.gif", "gone.gif", nullptr, nullptr)));
}

static void test_pick_skips_broken() {
  const ImageIndex idx = sample();
  TEST_ASSERT_EQUAL_STRING("c.gif", picked(idx, pickImage(idx, true, false, "", nullptr, "a.gif", "b.gif")));
  // a broken choice: the others in turn
  TEST_ASSERT_EQUAL_STRING("c.gif", picked(idx, pickImage(idx, true, false, "b.gif", nullptr, "a.gif", "b.gif")));
  TEST_ASSERT_EQUAL_STRING("a.gif", picked(idx, pickImage(idx, true, false, "", "b.gif", nullptr, "b.gif")));
  // the only one is broken: nothing to play now
  const ImagePick p = pickImage(idx, true, true, "", nullptr, nullptr, "p.gif");
  TEST_ASSERT_EQUAL(IMG_PLAY, p.state);
  TEST_ASSERT_EQUAL(-1, p.index);
}

static void test_pick_after_delete_and_replace() {
  ImageIndex idx = sample();
  uint32_t gen = idx.generation;
  // b on screen, deleted: the screen sees a new generation and moves on
  idx.erase("b.gif");
  TEST_ASSERT_NOT_EQUAL(gen, idx.generation);
  TEST_ASSERT_EQUAL_STRING("c.gif", picked(idx, pickImage(idx, true, false, "", nullptr, "b.gif", nullptr)));
  // c on screen, replaced: same name, new generation, the replacement plays
  gen = idx.generation;
  idx.put(info("c.gif", 4321));
  TEST_ASSERT_NOT_EQUAL(gen, idx.generation);
  const ImagePick p = pickImage(idx, true, false, "c.gif", nullptr, "c.gif", nullptr);
  TEST_ASSERT_EQUAL_STRING("c.gif", picked(idx, p));
  TEST_ASSERT_EQUAL(4321, idx.items[p.index].size);
  // the last ones deleted: the empty state
  idx.erase("a.gif");
  idx.erase("c.gif");
  TEST_ASSERT_EQUAL(IMG_OTHER_ONLY, pickImage(idx, true, false, "", nullptr, "c.gif", nullptr).state);
  idx.erase("p.gif");
  TEST_ASSERT_EQUAL(IMG_NO_IMAGE, pickImage(idx, true, false, "", nullptr, "c.gif", nullptr).state);
}

static void test_pick_writes_nothing() {
  const ImageIndex idx = sample();
  ImageChoice c;
  c.load();
  const char *after = nullptr;
  for (uint32_t t = 0; t < 200; t++) {
    const ImagePick p = pickImage(idx, true, t % 2, c.get(), nullptr, after, nullptr);
    after = picked(idx, p);
    turnOver(t * 500, t % 3 == 0, t % 5 == 0);
  }
  TEST_ASSERT_EQUAL(0, nvsWriteCount);
}

static void test_turn_timing() {
  // still: 10 s
  TEST_ASSERT_FALSE(turnOver(9999, false, true));
  TEST_ASSERT_TRUE(turnOver(TURN_MS, false, true));
  // animated: the end of the first loop past 10 s
  TEST_ASSERT_FALSE(turnOver(9000, true, false));
  TEST_ASSERT_FALSE(turnOver(12000, false, false));
  TEST_ASSERT_TRUE(turnOver(10500, true, false));
  // never more than 60 s
  TEST_ASSERT_FALSE(turnOver(TURN_MAX_MS - 1, false, false));
  TEST_ASSERT_TRUE(turnOver(TURN_MAX_MS, false, false));
}

static void test_frame_delay() {
  TEST_ASSERT_EQUAL(100, gifFrameDelay(0));
  TEST_ASSERT_EQUAL(100, gifFrameDelay(10));
  TEST_ASSERT_EQUAL(100, gifFrameDelay(-5));
  TEST_ASSERT_EQUAL(11, gifFrameDelay(11));
  TEST_ASSERT_EQUAL(20, gifFrameDelay(20));
}

static void test_shows_after_upload() {
  TEST_ASSERT_TRUE(showsAfterUpload(true, false, false, false, true));     // page, same orientation
  TEST_ASSERT_FALSE(showsAfterUpload(true, false, true, false, true));     // page, other orientation
  TEST_ASSERT_FALSE(showsAfterUpload(false, false, false, false, true));   // API without show
  TEST_ASSERT_TRUE(showsAfterUpload(false, true, true, true, true));        // API with show
  TEST_ASSERT_FALSE(showsAfterUpload(false, true, false, true, true));
  // IMAGE off: stored, never shown, whoever asked
  TEST_ASSERT_FALSE(showsAfterUpload(true, false, false, false, false));
  TEST_ASSERT_FALSE(showsAfterUpload(false, true, true, true, false));
  TEST_ASSERT_FALSE(showsAfterUpload(true, true, true, true, false));
}

static void test_reserve_retry() {
  Retry r;
  TEST_ASSERT_TRUE(r.due(0));
  TEST_ASSERT_TRUE(r.due(0x90000000));   // never failed: due whatever the clock says
  r.failed(5000);
  TEST_ASSERT_FALSE(r.due(5000));
  TEST_ASSERT_FALSE(r.due(5999));
  TEST_ASSERT_TRUE(r.due(6000));
  TEST_ASSERT_TRUE(r.due(5000 + 0x90000000));   // a stale failure never blocks
  r.failed(0xFFFFFE00);                         // across millis() wrapping
  TEST_ASSERT_FALSE(r.due(0x100));
  TEST_ASSERT_TRUE(r.due(0xFFFFFE00 + RETRY_MS));
  r.failed(0);
  TEST_ASSERT_FALSE(r.due(500));
}

// --- Canvas -------------------------------------------------------------------

static const int CW = 64, CH = 32;
static std::vector<uint16_t> canvas() { return std::vector<uint16_t>(CW * CH, 0xAAAA); }
static int changed(const std::vector<uint16_t> &c) {
  int n = 0;
  for (uint16_t v : c) n += v != 0xAAAA;
  return n;
}

static void test_canvas_clipping() {
  uint16_t palette[256];
  for (int i = 0; i < 256; i++) palette[i] = (uint16_t)(i + 1);
  std::vector<uint8_t> px(200);
  for (size_t i = 0; i < px.size(); i++) px[i] = (uint8_t)i;

  // rows above and below, columns past both edges: nothing drawn (ASan
  // fails the test on any write outside the canvas)
  std::vector<uint16_t> c = canvas();
  canvasLine(c.data(), CW, CH, 0, -1, 64, px.data(), palette, -1);
  canvasLine(c.data(), CW, CH, 0, CH, 64, px.data(), palette, -1);
  canvasLine(c.data(), CW, CH, CW, 3, 20, px.data(), palette, -1);
  canvasLine(c.data(), CW, CH, -30, 3, 20, px.data(), palette, -1);
  canvasLine(c.data(), CW, CH, 5, 3, 0, px.data(), palette, -1);
  canvasLine(c.data(), CW, CH, 5, 3, -7, px.data(), palette, -1);
  TEST_ASSERT_EQUAL(0, changed(c));

  // negative offset: the visible part starts with the right pixel
  canvasLine(c.data(), CW, CH, -10, 2, 20, px.data(), palette, -1);
  TEST_ASSERT_EQUAL(10, changed(c));
  TEST_ASSERT_EQUAL(palette[10], c[2 * CW + 0]);
  TEST_ASSERT_EQUAL(palette[19], c[2 * CW + 9]);

  // past the right edge
  c = canvas();
  canvasLine(c.data(), CW, CH, 60, 31, 10, px.data(), palette, -1);
  TEST_ASSERT_EQUAL(4, changed(c));
  TEST_ASSERT_EQUAL(palette[3], c[31 * CW + 63]);

  // wider than the canvas
  c = canvas();
  canvasLine(c.data(), CW, CH, -50, 0, 200, px.data(), palette, -1);
  TEST_ASSERT_EQUAL(CW, changed(c));
  TEST_ASSERT_EQUAL(palette[50], c[0]);

  // transparent index left as it was
  c = canvas();
  canvasLine(c.data(), CW, CH, 0, 1, 8, px.data(), palette, 3);
  TEST_ASSERT_EQUAL(7, changed(c));
  TEST_ASSERT_EQUAL(0xAAAA, c[1 * CW + 3]);

  // clears clipped on every side
  c = canvas();
  canvasClear(c.data(), CW, CH, -5, -5, 10, 10);
  TEST_ASSERT_EQUAL(25, changed(c));
  TEST_ASSERT_EQUAL(0, c[0]);
  c = canvas();
  canvasClear(c.data(), CW, CH, 60, 28, 1000, 1000);
  TEST_ASSERT_EQUAL(16, changed(c));
  c = canvas();
  canvasClear(c.data(), CW, CH, -100, -100, 1000, 1000);
  TEST_ASSERT_EQUAL(CW * CH, changed(c));
  c = canvas();
  canvasClear(c.data(), CW, CH, 70, 0, 10, 10);
  canvasClear(c.data(), CW, CH, 0, 40, 10, 10);
  canvasClear(c.data(), CW, CH, 0, 0, 0, 10);
  canvasClear(c.data(), CW, CH, 0, 0, 10, -3);
  canvasClear(c.data(), CW, CH, 2147483000, 2147483000, 2147483000, 2147483000);
  TEST_ASSERT_EQUAL(0, changed(c));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_partition_label_in_table);
  RUN_TEST(test_boot_steps);
  RUN_TEST(test_boot_keeps_a_filesystem_it_cannot_mount);
  RUN_TEST(test_mount_failure_reads_the_superblock);
  RUN_TEST(test_reset_order);
  RUN_TEST(test_reset_stops_at_failure);
  RUN_TEST(test_knob_reset_erases_without_marker);
  RUN_TEST(test_knob_reset_end);
  RUN_TEST(test_reset_resume_is_the_same_run);
  RUN_TEST(test_boot_without_marker_runs_nothing);
  RUN_TEST(test_boot_with_marker_resumes_or_locks);
  RUN_TEST(test_locked_panel_never_mounts);
  RUN_TEST(test_blank_check);
  RUN_TEST(test_marker_from_sector);
  RUN_TEST(test_marker_cannot_be_uploaded);
  RUN_TEST(test_mark_writes_and_reads_back);
  RUN_TEST(test_mark_keeps_a_marker_already_there);
  RUN_TEST(test_mark_unreadable_sector_is_not_erased);
  RUN_TEST(test_mark_without_partition_is_not_marked);
  RUN_TEST(test_upload_writes_after_signature);
  RUN_TEST(test_sanitize_names);
  RUN_TEST(test_leftover_names);
  RUN_TEST(test_sniff_types);
  RUN_TEST(test_upload_landscape_added);
  RUN_TEST(test_upload_streams_chunks);
  RUN_TEST(test_upload_never_started_writes_nothing);
  RUN_TEST(test_upload_no_file_or_storage);
  RUN_TEST(test_upload_announced_over_cap);
  RUN_TEST(test_upload_counted_over_cap);
  RUN_TEST(test_upload_not_gif_keeps_nothing);
  RUN_TEST(test_upload_wrong_size);
  RUN_TEST(test_upload_no_room_figures);
  RUN_TEST(test_reserve_kept);
  RUN_TEST(test_upload_25th_refused_replace_allowed);
  RUN_TEST(test_upload_refused_drops_temp);
  RUN_TEST(test_upload_abort_drops_temp);
  RUN_TEST(test_replace_atomic);
  RUN_TEST(test_sent_name_cut);
  RUN_TEST(test_status_codes);
  RUN_TEST(test_upload_texts);
  RUN_TEST(test_space_figures);
  RUN_TEST(test_index_order);
  RUN_TEST(test_scan_lists_images);
  RUN_TEST(test_scan_incomplete_when_a_file_cannot_be_opened);
  RUN_TEST(test_scan_incomplete_when_the_walk_fails);
  RUN_TEST(test_scan_incomplete_over_the_cap);
  RUN_TEST(test_generation_new_after_restart);
  RUN_TEST(test_delete_resets_choice_and_bumps_generation);
  RUN_TEST(test_delete_unknown);
  RUN_TEST(test_choice_writes_only_on_change);
  RUN_TEST(test_choice_stale_or_absent);
  RUN_TEST(test_choice_write_refused);
  RUN_TEST(test_choice_remove_absent_key);
  RUN_TEST(test_choice_unknown_ignored);
  RUN_TEST(test_pick_no_storage);
  RUN_TEST(test_pick_states);
  RUN_TEST(test_pick_in_turn);
  RUN_TEST(test_pick_choice);
  RUN_TEST(test_pick_first);
  RUN_TEST(test_pick_skips_broken);
  RUN_TEST(test_pick_after_delete_and_replace);
  RUN_TEST(test_pick_writes_nothing);
  RUN_TEST(test_turn_timing);
  RUN_TEST(test_frame_delay);
  RUN_TEST(test_shows_after_upload);
  RUN_TEST(test_reserve_retry);
  RUN_TEST(test_canvas_clipping);
  return UNITY_END();
}
