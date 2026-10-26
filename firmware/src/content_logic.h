// SPDX-License-Identifier: AGPL-3.0-only
// The parts of custom images that need no flash: names, GIF headers, space
// figures, the upload transaction, the saved choice, what the IMAGE screen
// plays next, and the order of a factory reset. Checked on the PC by
// test/test_content; content.cpp and settings.cpp bind them to the flash.
#pragma once

#include <stddef.h>
#include <stdint.h>

// The content area is found by this name in partitions.csv, never by offset.
static const char *const CONTENT_PARTITION_LABEL = "littlefs";
static const char *const IMAGE_DIR = "/img", *const UPLOAD_TEMP = "/img/.upload";
// 24 names fit the Home Assistant picker in one discovery message (MQTT
// buffer 2048 B); more needs a larger buffer.
static const int      MAX_IMAGES = 24;
// MULTIPART_SLACK: the form's boundaries and headers around the file, allowed
// on top of the cap when only the request size is known. RESERVE_BLOCKS stay
// free so LittleFS can always rewrite its metadata, a delete included.
static const uint32_t MAX_IMAGE_BYTES = 96 * 1024, MULTIPART_SLACK = 1024, RESERVE_BLOCKS = 4;
static const size_t   IMAGE_NAME_MAX = 40;
static const uint32_t TURN_MS = 10000, TURN_MAX_MS = 60000, RETRY_MS = 1000;

struct ImageInfo { char name[IMAGE_NAME_MAX + 1]; uint32_t size; bool portrait; };

// --- Headers and names -------------------------------------------------------
enum GifKind : uint8_t { KIND_GIF, KIND_PNG, KIND_JPEG, KIND_WEBP, KIND_OTHER };
struct GifHeader { GifKind kind; uint16_t width, height; };   // size: GIF only, else 0
GifHeader sniffImage(const uint8_t *b, size_t n);          // GIF needs 10 bytes, WebP 12
bool gifSizeAccepted(const GifHeader &h, bool *portrait);  // 64x32 or 32x64
// Names: [A-Za-z0-9._-] only (accents folded, anything else '-'), no leading
// dot, ends in ".gif", at most 40 characters, "image.gif" when nothing is
// left. Never a path: '/' and '\' cannot survive.
// Field contract, like the NVS keys: these rules name the files stored on
// panels and the saved choice (NVS "image"). Changing them would hide stored
// images (storedNameValid) and drop saved choices, so they change only with a
// migration. sanitize is idempotent: a stored name sent again keeps its name.
void sanitizeImageName(const char *sent, char *out, size_t size);
bool storedNameValid(const char *name);                    // what sanitize would keep as is
bool isLeftover(const char *fileName);                     // ".upload", bare or in a path
// Order: case-insensitive ASCII, ties by bytes (names are case-sensitive)
int  imageOrder(const char *a, const char *b);

// --- Space --------------------------------------------------------------
// Largest file LittleFS stores in `blocks` blocks: its CTZ skip-list spends
// 4 bytes per pointer, n*B - 4*(2(n-1) - popcount(n-1)).
uint32_t ctzCapacity(uint32_t blocks, uint32_t blockSize);
struct Space { uint32_t used, left, total; };              // bytes
uint32_t kbUp(uint32_t bytes); uint32_t kbDown(uint32_t bytes);

// The images in alphabetical order. `generation` moves on every change, so the
// screen notices a delete or a replace and picks again. It starts where the
// boot says: the scan counts the same files at every start, and the Images
// page's thumbnail addresses, kept a year by the browser, carry it.
struct ImageIndex {
  ImageInfo items[MAX_IMAGES]; int count = 0; uint32_t generation;
  explicit ImageIndex(uint32_t start = 0) : generation(start) {}
  int  find(const char *name) const;     // exact, -1 if absent
  bool put(const ImageInfo &info);       // sorted insert or replace; generation++
  bool erase(const char *name);          // generation++; false and unchanged if absent
};
// The walk of /img that builds the index at start: LittleFS on the
// panel, a fake in the tests.
enum IndexNote : uint8_t { NOTE_NOT_IMAGE, NOTE_UNREADABLE, NOTE_OVER_CAP };
class ImageDir {
public:
  // 1: an entry (its bare name, valid until the next call), 0: the end,
  // -1: the walk failed before the end
  virtual int next(const char **name, bool *isDir) = 0;
  // The file's size and its first bytes, up to 12; false: it cannot be opened
  virtual bool head(const char *name, uint32_t *size, uint8_t *bytes, size_t *n) = 0;
  virtual void note(const char *name, IndexNote why) = 0;   // for the log
  virtual ~ImageDir() {}
};
// Lists every stored image; names the sanitizer would change, directories and
// the upload leftover are skipped (neither listed nor removed). A header that
// cannot be read lists the file as landscape. True only when the walk reached
// its end and every image was opened and listed: the index is complete.
bool scanImages(ImageDir &dir, ImageIndex &idx);
// used = sum of sizes; left = what the next upload may take (reserve kept);
// total = used + left
Space spaceFigures(const ImageIndex &idx, uint32_t freeBlocks, uint32_t blockSize);

// --- Upload ------------------------------------------------------------------
// What an upload needs from the storage: a temp file swapped in by name once
// complete. LittleFS on the panel, a map in the tests: the transaction rules
// (nothing before the signature, nothing partial left) are then tested on the PC.
class ContentStore {
public:
  virtual bool beginTemp() = 0;  virtual bool writeTemp(const uint8_t *b, size_t n) = 0;
  virtual void dropTemp() = 0;   virtual bool commitTemp(const char *name) = 0;
  virtual bool remove(const char *name) = 0;
  virtual uint32_t freeBlocks() = 0;  virtual uint32_t blockSize() = 0;
  virtual ~ContentStore() {}
};
enum UploadResult : uint8_t { UP_PENDING, UP_ADDED, UP_REPLACED, UP_NO_FILE, UP_NO_STORAGE,
  UP_TOO_BIG, UP_NOT_GIF, UP_WRONG_SIZE, UP_NO_ROOM, UP_TOO_MANY, UP_FAILED };
int uploadStatus(UploadResult r);        // HTTP status of the API answer
// "image added", "image replaced" or "image not added": the page's plain
// answer when the Images page cannot be built, sent with uploadStatus
const char *uploadText(UploadResult r);

// One upload at a time, fed chunk by chunk. Holds a 12-byte copy of the
// header, never a chunk: nothing reaches the flash before the first 12 bytes
// pass as a GIF of an accepted size, so every stored file starts with "GIF8".
// After a refusal the temp file is gone at once and later bytes are only
// counted (received() = what the image needs, for the no-room message).
class ImageUpload {
public:
  UploadResult start(ContentStore &s, const ImageIndex &idx, bool storage,
                     const char *sentName, uint32_t announced);   // announced 0 = unknown
  UploadResult write(ContentStore &s, const uint8_t *data, size_t n);
  UploadResult finish(ContentStore &s, ImageIndex &idx);
  void abort(ContentStore &s);
  const char *name() const; const char *sent() const;   // sent: cut to 60 bytes, unescaped
  uint32_t received() const; uint32_t left() const; GifHeader header() const;
  bool portrait() const;
private:
  UploadResult refuse(ContentStore &s, UploadResult r);
  UploadResult checkHeader(size_t n);
  UploadResult result_ = UP_NO_FILE;      // nothing started: write() stores nothing
  char name_[IMAGE_NAME_MAX + 1] = "";
  char sent_[61] = "";
  uint8_t head_[12] = {};
  size_t headLen_ = 0;
  bool tempOpen_ = false, portrait_ = false;
  uint32_t received_ = 0, left_ = 0;
  GifHeader header_ = {KIND_OTHER, 0, 0};
};

// --- Choice ------------------------------------------------------------------
// NVS key "image" in NVS_NAMESPACE: the image shown, "" or absent = all in turn.
class ImageChoice {
public:
  void load();  const char *get() const;  // "" = all in turn
  // Writes only on change. false: not a valid name, or NVS refused the
  // write or could not be opened (the old choice is kept). With NVS closed
  // by the lock (panelLocked(), settings.h) the choice lives in RAM.
  bool set(const char *name);
  // Stale name -> "" (one write). Call only with a complete index: a partial
  // listing would erase a choice that still exists.
  void validate(const ImageIndex &idx);
private:
  char value_[IMAGE_NAME_MAX + 1] = "";
};
bool choiceValid(const ImageIndex &idx, const char *name);   // nullptr/"" = all: valid

// --- What the IMAGE screen plays -----------------------------------------------
enum ImageState : uint8_t { IMG_PLAY, IMG_NO_IMAGE, IMG_OTHER_ONLY, IMG_NO_STORAGE };
// IMG_PLAY with index -1: the only images of this orientation are `skip`
// (broken): stay black and pick again at the next turn.
struct ImagePick { ImageState state; int index; };
// first: a just-added image, shown before anything else; choice: the
// saved one ("" = all); after: the name shown last, stored or not; skip: a
// name that failed to decode. Any of them may be nullptr.
ImagePick pickImage(const ImageIndex &idx, bool storage, bool portrait, const char *choice,
                    const char *first, const char *after, const char *skip);
bool turnOver(uint32_t shownMs, bool loopEnded, bool still);
uint32_t gifFrameDelay(int ms);                                // <= 10 ms -> 100
// imageOn: IMAGE is on in the owner's list; off, an upload is stored, never shown
bool showsAfterUpload(bool fromPage, bool showArg, bool imagePortrait, bool panelPortrait,
                      bool imageOn);
// Decoder reservation retry. `at` is the moment of the last failure,
// 0 = none: compared by difference, so millis() wrapping never blocks it.
struct Retry { uint32_t at = 0; bool due(uint32_t now) const; void failed(uint32_t now); };
// Canvas writes clipped to the canvas whatever the GIF's frame rectangle says
void canvasLine(uint16_t *canvas, int cw, int ch, int x0, int y, int w, const uint8_t *pixels,
                const uint16_t *palette, int transparent);     // -1 = no transparent index
void canvasClear(uint16_t *canvas, int cw, int ch, int x, int y, int w, int h);

// --- Factory reset -------------------------------------------------------
bool blankFlash(const uint32_t *words, size_t count);          // all 0xFFFFFFFF
// Flash steps in settings.cpp, a fake in the tests
struct ResetOps {
  virtual bool markPending() = 0;   virtual bool eraseImages() = 0;
  virtual bool eraseCrashDump() = 0; virtual bool eraseSettings() = 0;
  virtual bool clearPending() = 0;
  virtual ~ResetOps() {}
};
// Marker, images, crash dump, settings, marker cleared: in that order, stops
// at the first failure so the marker stays and the next start resumes.
bool runFactoryReset(ResetOps &ops);
// The knob's reset (factoryReset()): the same run, except when the marker
// cannot be written. No later start could resume it then, so every erase
// runs, in order, whatever the earlier ones returned, and the owner's data
// goes now. True when every erase and the clear were read back, marker or not.
bool runFactoryResetNow(ResetOps &ops);
// How the knob's reset ends, from runFactoryResetNow() and whether step 1
// wrote the marker: done, failed but resumed at the next start (both
// restart), or failed with no marker, so nothing would resume it: the panel
// stays locked on this start rather than restart into a normal one.
enum KnobReset : uint8_t { KR_DONE, KR_RESUME, KR_LOCKED };
KnobReset knobResetEnd(bool done, bool marked);
enum MarkerState : uint8_t { MARK_ABSENT, MARK_PRESENT, MARK_UNREADABLE };
enum ResetBoot : uint8_t { RB_NORMAL, RB_DONE, RB_LOCKED };
ResetBoot resetAtBoot(MarkerState marker, ResetOps &ops);   // unreadable = pending
// The marker's first word reads "CRST": not "GIF8", the first bytes of every
// uploaded file, and far above LittleFS block pointers and revision counts,
// so no file or metadata block can pose as a pending reset.
static const uint32_t RESET_MAGIC[4] = {0x54535243, 0x45534552, 0x444e4550, 0x00000001};
// The first 16 bytes of the marker sector, as read (readOk: the read
// succeeded). That sector is also the image area's last block, so whatever
// LittleFS left there is MARK_ABSENT: only the exact magic is a pending reset
// and only a failed read is MARK_UNREADABLE, or a panel would wipe itself.
MarkerState markerFrom(const uint32_t *words, bool readOk);
// The marker sector's first 16 bytes, raw: content.cpp's partition, a fake in the tests
struct MarkerSector {
  virtual bool read(uint32_t words[4]) = 0;
  virtual bool erase() = 0;
  virtual bool write(const uint32_t words[4]) = 0;
  virtual ~MarkerSector() {}
};
// Step 1: the magic written and read back. A marker already there is kept
// as is: a resumed reset erasing it to write it again would leave none if
// the write failed or the power went in between, and the next start would
// run on the previous owner's settings. A sector that cannot be read is not
// erased either, for the same reason: false, nothing written. No sector
// (an older table without the littlefs partition) is not marked either: no
// later start could find the marker, so the knob's reset must not count on it.
bool markResetPending(MarkerSector *s);
enum StorageBoot : uint8_t { BOOT_MISSING, BOOT_LOCKED, BOOT_MOUNT, BOOT_MOUNTED,
                             BOOT_FORMAT_ONCE, BOOT_UNAVAILABLE };
// MOUNT_NO_FS: the mount failed over an area that holds no littlefs, the
// only case that formats. MOUNT_FAILED: it failed over one (no memory, a read
// error, damage), or what the area holds could not be read: images kept.
enum MountState : uint8_t { MOUNT_NOT_TRIED, MOUNT_OK, MOUNT_NO_FS, MOUNT_FAILED };
// littlefs writes its name at byte 8 of both blocks of its superblock pair,
// blocks 0 and 1. heads: the first SUPERBLOCK_HEAD bytes of each, read raw
// (readOk: both reads succeeded); MOUNT_NO_FS only when neither has it.
static const size_t SUPERBLOCK_HEAD = 16;
MountState mountFailure(const uint8_t *block0, const uint8_t *block1, bool readOk);
// Asked before touching the area (MOUNT_NOT_TRIED): MISSING and LOCKED are
// final and the caller neither mounts nor formats; BOOT_MOUNT: mount and ask
// again with the result, format once on BOOT_FORMAT_ONCE, mount and ask a
// last time with formatTried.
StorageBoot storageBoot(bool partitionFound, bool resetPending, MountState mount,
                        bool formatTried);
// storageBoot()'s resetPending. A locked panel counts too, whatever the
// marker reads now: a marker step that failed may have left it blank, and
// the image area of an unfinished reset is never mounted.
bool resetPending(bool locked, MarkerState marker);
