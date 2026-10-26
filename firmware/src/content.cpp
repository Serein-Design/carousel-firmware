// SPDX-License-Identifier: AGPL-3.0-only
#include "content.h"

#include <LittleFS.h>
#include <atomic>
#include <dirent.h>
#include <errno.h>
#include <memory>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <new>
#include <string.h>

#include "settings.h"

// esp_littlefs uses the 4 KB flash sector as its block; the marker takes the
// last one of the partition.
static const uint32_t SECTOR = 4096;
static const TickType_t LOOP_WAIT = pdMS_TO_TICKS(20);   // loop(): the watchdog and the frame rate
static const size_t PATH_MAX_LEN = 64;   // "/img/" + a 40-character name
static const char *const MOUNT_POINT = "/littlefs";

static SemaphoreHandle_t mutex_ = nullptr;
static TaskHandle_t loopTask_ = nullptr;
// 1.2 KB: allocated in contentBegin(), after the display's DMA buffers
static ImageIndex *idx_ = nullptr;
static ImageChoice choice_;
static ImageUpload upload_;
static File play_;
static char playName_[IMAGE_NAME_MAX + 1] = "";
static char first_[IMAGE_NAME_MAX + 1] = "";
static bool mounted_ = false, indexComplete_ = false;
static std::atomic<bool> available_{false};
static std::atomic<uint32_t> generation_{0};
// Bit 0: a landscape image is listed, bit 1: a portrait one. Read by the
// display loop every frame, so it never waits on the mutex.
static std::atomic<uint8_t> orientations_{0};
// A copy of the choice behind a spinlock: MQTT reads it from loop() without
// ever waiting on the mutex.
static char choiceCopy_[IMAGE_NAME_MAX + 1] = "";
static portMUX_TYPE choiceMux_ = portMUX_INITIALIZER_UNLOCKED;

// The mutex for the current scope. loop() waits at most LOOP_WAIT and then
// reports busy; the other tasks wait, as every holder keeps it for one
// bounded step. Recursive: the display loop picks while it holds the lock.
class Hold {
public:
  Hold() {
    const TickType_t wait = xTaskGetCurrentTaskHandle() == loopTask_ ? LOOP_WAIT : portMAX_DELAY;
    held_ = mutex_ && xSemaphoreTakeRecursive(mutex_, wait) == pdTRUE;
  }
  ~Hold() { if (held_) xSemaphoreGiveRecursive(mutex_); }
  Hold(const Hold &) = delete;
  Hold &operator=(const Hold &) = delete;
  explicit operator bool() const { return held_; }
private:
  bool held_ = false;
};

static const esp_partition_t *area() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                  CONTENT_PARTITION_LABEL);
}

// Under the mutex, wherever the index changed: what other tasks read of it
static void indexChanged() {
  uint8_t o = 0;
  for (int i = 0; i < idx_->count; i++) o |= idx_->items[i].portrait ? 2 : 1;
  orientations_ = o;
  generation_ = idx_->generation;
}

static void imagePath(const char *name, char *path) {
  snprintf(path, PATH_MAX_LEN, "%s/%s", IMAGE_DIR, name);
}

static void copyChoice() {
  portENTER_CRITICAL(&choiceMux_);
  strlcpy(choiceCopy_, choice_.get(), sizeof(choiceCopy_));
  portEXIT_CRITICAL(&choiceMux_);
}

// Under the mutex: esp_littlefs refuses to unlink or rename an open file, and
// the player must never read a file that is being replaced.
static void closePlayIf(const char *name) {
  if (play_ && strcmp(playName_, name) == 0) {
    play_.close();
    playName_[0] = '\0';
  }
}

// --- Storage for the upload transaction ----------------------------------------

class LittleFsStore : public ContentStore {
public:
  bool beginTemp() override {
    written_ = 0;
    temp_ = LittleFS.open(UPLOAD_TEMP, FILE_WRITE);
    return (bool)temp_;
  }
  bool writeTemp(const uint8_t *b, size_t n) override {
    if (!temp_ || temp_.write(b, n) != n) return false;
    written_ += n;
    return true;
  }
  void dropTemp() override {
    temp_.close();
    if (LittleFS.exists(UPLOAD_TEMP) && !LittleFS.remove(UPLOAD_TEMP))
      Serial.println("[CONTENT] could not remove the upload temp file");
  }
  // rename() replaces the target in one step, so a replaced image is never
  // half there. The size read back catches a write lost when the file closed.
  bool commitTemp(const char *name) override {
    temp_.close();
    File back = LittleFS.open(UPLOAD_TEMP, FILE_READ);
    const bool whole = back && back.size() == written_;
    back.close();
    if (!whole) return false;
    char path[PATH_MAX_LEN];
    imagePath(name, path);
    closePlayIf(name);
    return LittleFS.rename(UPLOAD_TEMP, path);
  }
  bool remove(const char *name) override {
    char path[PATH_MAX_LEN];
    imagePath(name, path);
    closePlayIf(name);
    return LittleFS.remove(path);
  }
  uint32_t freeBlocks() override {
    const size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
    return total > used ? (total - used) / SECTOR : 0;
  }
  uint32_t blockSize() override { return SECTOR; }
private:
  File temp_;
  size_t written_ = 0;
};

static LittleFsStore store_;

// --- Boot ------------------------------------------------------------------------

// By label, never formatOnFail; the one format of a start is explicit.
// LittleFS.begin() fails alike for no filesystem, no memory or a read error:
// the area's superblock pair, read raw, tells them apart.
static MountState mount(const esp_partition_t *p) {
  if (LittleFS.begin(false, MOUNT_POINT, 4, CONTENT_PARTITION_LABEL)) return MOUNT_OK;
  uint32_t b0[SUPERBLOCK_HEAD / 4], b1[SUPERBLOCK_HEAD / 4];
  const bool readOk = esp_partition_read(p, 0, b0, sizeof(b0)) == ESP_OK &&
                      esp_partition_read(p, SECTOR, b1, sizeof(b1)) == ESP_OK;
  return mountFailure((const uint8_t *)b0, (const uint8_t *)b1, readOk);
}

// readdir() itself, not File::openNextFile(): that returns an empty File,
// read as the end, when one file cannot be opened (no memory, a read error)
// while entries remain, and a choice still on the flash would then be dropped.
class LittleFsDir : public ImageDir {
public:
  LittleFsDir() {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s%s", MOUNT_POINT, IMAGE_DIR);
    d_ = opendir(path);
  }
  ~LittleFsDir() override { if (d_) closedir(d_); }
  explicit operator bool() const { return d_ != nullptr; }
  int next(const char **name, bool *isDir) override {
    errno = 0;   // readdir() answers nullptr at the end and on an error
    const struct dirent *e = readdir(d_);
    if (!e) {
      if (errno == 0) return 0;
      Serial.printf("[CONTENT] listing /img stopped (errno %d)\n", errno);
      return -1;
    }
    *name = e->d_name;
    *isDir = e->d_type == DT_DIR;
    return 1;
  }
  bool head(const char *name, uint32_t *size, uint8_t *bytes, size_t *n) override {
    char path[PATH_MAX_LEN];
    imagePath(name, path);
    File f = LittleFS.open(path, FILE_READ);
    if (!f) return false;
    *size = f.size();
    *n = f.read(bytes, 12);
    return true;
  }
  void note(const char *name, IndexNote why) override {
    if (why == NOTE_NOT_IMAGE) Serial.printf("[CONTENT] not an image name, ignored: %s\n", name);
    else if (why == NOTE_UNREADABLE) Serial.printf("[CONTENT] cannot open, not listed: %s\n", name);
    else Serial.printf("[CONTENT] more than %d images, not listed: %s\n", MAX_IMAGES, name);
  }
private:
  DIR *d_ = nullptr;
};

// The index from each file's 12-byte header. True when every file of
// /img was looked at and every valid name listed: only then may a stale
// choice be dropped.
static bool loadIndex() {
  if (!LittleFS.exists(IMAGE_DIR) && !LittleFS.mkdir(IMAGE_DIR)) {
    Serial.println("[CONTENT] cannot create /img");
    return false;
  }
  // An upload cut by a restart
  if (LittleFS.exists(UPLOAD_TEMP)) {
    const bool removed = LittleFS.remove(UPLOAD_TEMP);
    Serial.printf("[CONTENT] unfinished upload %s\n", removed ? "removed" : "could not be removed");
  }
  LittleFsDir dir;
  if (!dir) {
    Serial.println("[CONTENT] cannot list /img");
    return false;
  }
  return scanImages(dir, *idx_);
}

void contentBegin() {
  mutex_ = xSemaphoreCreateRecursiveMutex();
  loopTask_ = xTaskGetCurrentTaskHandle();
  choice_.load();
  copyChoice();
  // A start of its own at every boot, so no thumbnail address comes back
  // after a restart. Before the radio esp_random() is only pseudo-random,
  // from a state the bootloader mixed with noise: unique, not secret.
  idx_ = new (std::nothrow) ImageIndex(esp_random());
  if (!mutex_ || !idx_) {
    Serial.println("[CONTENT] out of memory: images not available");
    return;
  }
  const esp_partition_t *p = area();
  const bool pending = p && resetPending(panelLocked(), contentResetMarker());
  StorageBoot step = storageBoot(p != nullptr, pending, MOUNT_NOT_TRIED, false);
  if (step == BOOT_MISSING) {
    Serial.println("[CONTENT] no littlefs partition: images need the cable flash");
    return;
  }
  if (step == BOOT_LOCKED) {
    // The reset is not finished: the area is neither mounted nor formatted
    Serial.println("[CONTENT] factory reset pending: image area locked");
    return;
  }
  MountState m = mount(p);
  step = storageBoot(true, pending, m, false);
  if (m == MOUNT_FAILED)
    Serial.println("[CONTENT] mount failed over a filesystem or an unreadable area: not formatted");
  if (step == BOOT_FORMAT_ONCE) {
    // The first start with this table, or after a factory reset
    Serial.println("[CONTENT] no filesystem in the image area: formatting it");
    const bool formatted = LittleFS.format();
    if (!formatted) Serial.println("[CONTENT] format failed");
    m = formatted ? mount(p) : MOUNT_FAILED;
    step = storageBoot(true, pending, m, true);
  }
  mounted_ = m == MOUNT_OK;
  if (step != BOOT_MOUNTED) {
    Serial.println("[CONTENT] image area cannot be mounted: images not available");
    return;
  }
  indexComplete_ = loadIndex();
  if (indexComplete_) {
    choice_.validate(*idx_);
    copyChoice();
  }
  indexChanged();
  available_ = true;
  const Space sp = spaceFigures(*idx_, store_.freeBlocks(), SECTOR);
  Serial.printf("[CONTENT] %d images, %lu KB used, %lu KB left\n", idx_->count,
                (unsigned long)kbUp(sp.used), (unsigned long)kbDown(sp.left));
}

bool contentAvailable() { return available_; }

int contentList(ImageInfo *out, int max) {
  Hold h;
  if (!h) return -1;
  if (!available_ || !out || max <= 0) return 0;
  const int n = idx_->count < max ? idx_->count : max;
  memcpy(out, idx_->items, n * sizeof(ImageInfo));
  return n;
}

Space contentSpace() {
  Hold h;
  if (!h || !available_) return {0, 0, 0};
  return spaceFigures(*idx_, store_.freeBlocks(), SECTOR);
}

uint32_t contentGeneration() { return generation_; }

// Each block is opened, read and closed under the mutex, and the client is
// written to outside it: a delete or a replace (esp_littlefs refuses both on
// an open file) never waits for a slow browser, and the display loop's
// try-lock misses at most one block's read. Any change of the index stops
// the answer, so a file replaced midway never comes out half old, half new.
// The block on the heap: the web task's stack is 8 KB.
static const size_t FILE_BLOCK = 512;

bool contentFileSend(const char *name, bool (*write)(const uint8_t *data, size_t n, void *ctx),
                     void *ctx) {
  std::unique_ptr<uint8_t[]> block(new (std::nothrow) uint8_t[FILE_BLOCK]);
  if (!block || !name || !write) return false;
  char path[PATH_MAX_LEN];
  uint32_t pos = 0, size = 0, generation = 0;
  for (;;) {
    size_t n = 0;
    {
      Hold h;
      if (!h || !available_) return false;
      if (pos == 0) {
        if (idx_->find(name) < 0) return false;
        generation = idx_->generation;
        imagePath(name, path);
      } else if (idx_->generation != generation) {
        Serial.printf("[CONTENT] %s changed while sent, answer cut\n", name);
        return false;
      }
      File f = LittleFS.open(path, FILE_READ);
      if (!f || (pos && !f.seek(pos))) return false;
      if (pos == 0) size = f.size();
      n = f.read(block.get(), FILE_BLOCK);
    }
    // The end; a read that fails sooner is a cut, not a whole file
    if (n == 0) return pos > 0 && pos == size;
    if (!write(block.get(), n, ctx)) return false;
    pos += n;
  }
}

// A broken image still counts: it shows black, not a text state
bool contentPlayable(bool portrait) {
  return available_ && (orientations_ & (portrait ? 2 : 1));
}

// --- Upload ----------------------------------------------------------------------

UploadResult contentUploadStart(const char *sentName, uint32_t announced) {
  Hold h;
  if (!idx_) {   // out of memory at start: no index to check names against
    upload_.abort(store_);
    return UP_NO_STORAGE;
  }
  return upload_.start(store_, *idx_, h && available_, sentName, announced);
}

UploadResult contentUploadWrite(const uint8_t *data, size_t n) {
  return upload_.write(store_, data, n);
}

UploadResult contentUploadFinish() {
  Hold h;
  if (!idx_ || !h) {   // only before contentBegin(): nothing can be committed
    upload_.abort(store_);
    return UP_FAILED;
  }
  const UploadResult r = upload_.finish(store_, *idx_);
  if (r == UP_ADDED || r == UP_REPLACED) {
    indexChanged();
    Serial.printf("[CONTENT] %s %s (%lu bytes)\n", r == UP_ADDED ? "added" : "replaced",
                  upload_.name(), (unsigned long)upload_.received());
  } else {
    Serial.printf("[CONTENT] upload refused (%d): %s\n", (int)r, upload_.name());
  }
  return r;
}

void contentUploadAbort() {
  Hold h;
  upload_.abort(store_);
}

const ImageUpload &contentUpload() { return upload_; }

// --- Delete and choice ---------------------------------------------------------------

bool contentDelete(const char *name) {
  Hold h;
  if (!h || !available_ || idx_->find(name) < 0) return false;
  if (!store_.remove(name)) {
    Serial.printf("[CONTENT] delete failed: %s\n", name);
    return false;
  }
  idx_->erase(name);
  indexChanged();
  if (strcmp(first_, name) == 0) first_[0] = '\0';
  // A partial index could drop a choice that still exists
  if (indexComplete_) {
    choice_.validate(*idx_);
    copyChoice();
  }
  Serial.printf("[CONTENT] deleted %s\n", name);
  return true;
}

bool contentChoose(const char *name) {
  Hold h;
  if (!h) return false;
  const bool all = !name || !*name;
  if (!all && (!available_ || idx_->find(name) < 0)) return false;
  if (!choice_.set(all ? "" : name)) {
    Serial.println("[CONTENT] choice not saved: NVS refused the write");
    return false;
  }
  copyChoice();
  return true;
}

void contentChoice(char *out, size_t size) {
  if (!out || size == 0) return;
  portENTER_CRITICAL(&choiceMux_);
  strlcpy(out, choiceCopy_, size);
  portEXIT_CRITICAL(&choiceMux_);
}

void contentShowFirst(const char *name) {
  Hold h;
  if (h) strlcpy(first_, name ? name : "", sizeof(first_));
}

ImagePick contentPick(bool portrait, const char *after, const char *skip, char *name, size_t size) {
  if (name && size) name[0] = '\0';
  Hold h;
  if (!h) return {IMG_PLAY, -1};
  if (!idx_) return {IMG_NO_STORAGE, -1};
  const ImagePick p = pickImage(*idx_, available_, portrait, choice_.get(), first_, after, skip);
  first_[0] = '\0';   // shown first for one turn only
  if (p.state == IMG_PLAY && p.index >= 0 && name && size)
    strlcpy(name, idx_->items[p.index].name, size);
  return p;
}

// --- Playback ----------------------------------------------------------------------

bool contentPlayOpen(const char *name, int32_t *size) {
  contentPlayClose();
  if (!available_ || !name || !storedNameValid(name)) return false;
  char path[PATH_MAX_LEN];
  imagePath(name, path);
  play_ = LittleFS.open(path, FILE_READ);
  if (!play_) return false;
  strlcpy(playName_, name, sizeof(playName_));
  if (size) *size = (int32_t)play_.size();
  return true;
}

int32_t contentPlayRead(uint8_t *buf, int32_t len) {
  if (!play_ || !buf || len <= 0) return 0;
  return (int32_t)play_.read(buf, (size_t)len);
}

int32_t contentPlaySeek(int32_t pos) {
  if (!play_ || pos < 0 || !play_.seek((uint32_t)pos, SeekSet)) return -1;
  return (int32_t)play_.position();
}

bool contentPlayValid() { return (bool)play_; }

void contentPlayClose() {
  play_.close();
  playName_[0] = '\0';
}

bool contentPlayLock() { return mutex_ && xSemaphoreTakeRecursive(mutex_, 0) == pdTRUE; }
void contentPlayUnlock() { if (mutex_) xSemaphoreGiveRecursive(mutex_); }

// --- Factory reset ----------------------------------------------------------------

// Reads [from, to) back in 1 KB steps: erased flash is all ones
static bool readsBlank(const esp_partition_t *p, size_t from, size_t to) {
  uint32_t words[256];
  for (size_t off = from; off < to; off += sizeof(words)) {
    if (esp_partition_read(p, off, words, sizeof(words)) != ESP_OK) return false;
    if (!blankFlash(words, sizeof(words) / sizeof(words[0]))) return false;
  }
  return true;
}

// The raw steps below write under LittleFS: nothing may use it afterwards.
// False when the mutex could not be had: the area may still be in use, so
// the step that asked (marker or image erase) fails. That alone does not
// stop the reset: the knob's reset without a marker still runs the others.
static bool unmount() {
  if (!mutex_) return true;            // before contentBegin(): never mounted
  Hold h;
  if (!h) return false;
  if (!mounted_) return true;
  available_ = false;
  contentPlayClose();
  // An upload writes its temp file without the mutex: dropped before the
  // filesystem goes, so no LittleFS write lands after the raw erase
  upload_.abort(store_);
  LittleFS.end();
  mounted_ = false;
  Serial.println("[CONTENT] image area unmounted for the factory reset");
  return true;
}

// The image area's last sector, where the reset marker lives
class PartitionMarker : public MarkerSector {
public:
  explicit PartitionMarker(const esp_partition_t *p) : p_(p), at_(p->size - SECTOR) {}
  bool read(uint32_t words[4]) override {
    return esp_partition_read(p_, at_, words, sizeof(RESET_MAGIC)) == ESP_OK;
  }
  bool erase() override { return esp_partition_erase_range(p_, at_, SECTOR) == ESP_OK; }
  bool write(const uint32_t words[4]) override {
    return esp_partition_write(p_, at_, words, sizeof(RESET_MAGIC)) == ESP_OK;
  }
private:
  const esp_partition_t *p_;
  size_t at_;
};

bool contentMarkReset() {
  const esp_partition_t *p = area();
  if (!p) return markResetPending(nullptr);   // older table: not marked
  if (!unmount()) return false;
  PartitionMarker sector(p);
  return markResetPending(&sector);
}

bool contentClearReset() {
  const esp_partition_t *p = area();
  if (!p) return true;
  const size_t at = p->size - SECTOR;
  return esp_partition_erase_range(p, at, SECTOR) == ESP_OK && readsBlank(p, at, p->size);
}

MarkerState contentResetMarker() {
  const esp_partition_t *p = area();
  if (!p) return MARK_ABSENT;          // older table: nowhere to keep a marker
  uint32_t back[4];
  const bool readOk = esp_partition_read(p, p->size - SECTOR, back, sizeof(back)) == ESP_OK;
  return markerFrom(back, readOk);
}

bool flashEraseVerified(const esp_partition_t *p, size_t from, size_t len) {
  for (int attempt = 1; attempt <= 3; attempt++) {
    if (esp_partition_erase_range(p, from, len) == ESP_OK && readsBlank(p, from, from + len))
      return true;
    Serial.printf("[FLASH] %s: erase attempt %d failed\n", p->label, attempt);
  }
  return false;
}

bool contentErase() {
  const esp_partition_t *p = area();
  if (!p) return true;                 // older table: no image was ever stored
  // Step 1 unmounts too, but this step runs even when step 1 failed
  if (!unmount()) return false;
  if (!flashEraseVerified(p, 0, p->size - SECTOR)) return false;
  Serial.println("[CONTENT] images erased");
  return true;
}
