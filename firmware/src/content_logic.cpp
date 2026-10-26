// SPDX-License-Identifier: AGPL-3.0-only
#include "content_logic.h"

#include <string.h>

#include <Preferences.h>

#include "i18n.h"
#include "settings.h"

static const char *const CHOICE_KEY = "image";   // NVS, at most 15 characters

// --- Headers and names -------------------------------------------------------

GifHeader sniffImage(const uint8_t *b, size_t n) {
  static const uint8_t PNG_SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (b && n >= 10 && (memcmp(b, "GIF87a", 6) == 0 || memcmp(b, "GIF89a", 6) == 0))
    return {KIND_GIF, (uint16_t)(b[6] | b[7] << 8), (uint16_t)(b[8] | b[9] << 8)};
  if (b && n >= 8 && memcmp(b, PNG_SIG, 8) == 0) return {KIND_PNG, 0, 0};
  if (b && n >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) return {KIND_JPEG, 0, 0};
  if (b && n >= 12 && memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WEBP", 4) == 0)
    return {KIND_WEBP, 0, 0};
  return {KIND_OTHER, 0, 0};
}

bool gifSizeAccepted(const GifHeader &h, bool *portrait) {
  if (h.kind != KIND_GIF) return false;
  const bool wide = h.width == 64 && h.height == 32, tall = h.width == 32 && h.height == 64;
  if (wide || tall) *portrait = tall;
  return wide || tall;
}

static bool nameChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         c == '.' || c == '_' || c == '-';
}

static char lowerAscii(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

void sanitizeImageName(const char *sent, char *out, size_t size) {
  if (!out || size == 0) return;
  // Accents folded as on the panel; what remains is ASCII, or '?' for the rest
  char folded[128];
  toPanelText(sent ? sent : "", folded, sizeof(folded));
  size_t len = strlen(folded);
  for (size_t i = 0; i < len; i++)
    if (!nameChar(folded[i])) folded[i] = '-';   // '/', '\' and controls included
  if (len >= 4 && folded[len - 4] == '.' && lowerAscii(folded[len - 3]) == 'g' &&
      lowerAscii(folded[len - 2]) == 'i' && lowerAscii(folded[len - 1]) == 'f')
    len -= 4;                                    // the extension, written back in lower case
  const char *stem = folded;
  while (len > 0 && *stem == '.') { stem++; len--; }   // no hidden file
  const size_t cap = size - 1 < IMAGE_NAME_MAX ? size - 1 : IMAGE_NAME_MAX;
  const size_t stemMax = cap > 4 ? cap - 4 : 0;
  if (len == 0) { stem = "image"; len = 5; }
  if (len > stemMax) len = stemMax;             // shortened before the extension
  snprintf(out, size, "%.*s.gif", (int)len, stem);
}

bool storedNameValid(const char *name) {
  if (!name || strlen(name) > IMAGE_NAME_MAX) return false;
  char clean[IMAGE_NAME_MAX + 1];
  sanitizeImageName(name, clean, sizeof(clean));
  return strcmp(clean, name) == 0;
}

bool isLeftover(const char *fileName) {
  if (!fileName) return false;
  const char *slash = strrchr(fileName, '/');
  return strcmp(slash ? slash + 1 : fileName, strrchr(UPLOAD_TEMP, '/') + 1) == 0;
}

int imageOrder(const char *a, const char *b) {
  for (size_t i = 0;; i++) {
    const char x = lowerAscii(a[i]), y = lowerAscii(b[i]);
    if (x != y) return (unsigned char)x < (unsigned char)y ? -1 : 1;
    if (!x) break;
  }
  const int c = strcmp(a, b);
  return c < 0 ? -1 : c > 0;
}

// --- Space -------------------------------------------------------------------

uint32_t ctzCapacity(uint32_t blocks, uint32_t blockSize) {
  if (blocks == 0) return 0;
  const uint32_t n1 = blocks - 1;
  return blocks * blockSize - 4 * (2 * n1 - (uint32_t)__builtin_popcount(n1));
}

uint32_t kbUp(uint32_t bytes) { return bytes / 1024 + (bytes % 1024 != 0); }
uint32_t kbDown(uint32_t bytes) { return bytes / 1024; }

int ImageIndex::find(const char *name) const {
  if (!name) return -1;
  for (int i = 0; i < count; i++)
    if (strcmp(items[i].name, name) == 0) return i;
  return -1;
}

bool ImageIndex::put(const ImageInfo &info) {
  int at = find(info.name);
  if (at < 0) {
    if (count >= MAX_IMAGES) return false;
    at = 0;
    while (at < count && imageOrder(items[at].name, info.name) < 0) at++;
    memmove(&items[at + 1], &items[at], (count - at) * sizeof(ImageInfo));
    count++;
  }
  items[at] = info;
  items[at].name[IMAGE_NAME_MAX] = '\0';
  generation++;
  return true;
}

bool ImageIndex::erase(const char *name) {
  const int at = find(name);
  if (at < 0) return false;
  memmove(&items[at], &items[at + 1], (count - at - 1) * sizeof(ImageInfo));
  count--;
  generation++;
  return true;
}

bool scanImages(ImageDir &dir, ImageIndex &idx) {
  bool complete = true;
  const char *name = nullptr;
  bool isDir = false;
  int r;
  while ((r = dir.next(&name, &isDir)) > 0) {
    if (isDir || isLeftover(name) || !storedNameValid(name)) {
      dir.note(name, NOTE_NOT_IMAGE);
      continue;
    }
    ImageInfo info = {};
    snprintf(info.name, sizeof(info.name), "%s", name);
    uint8_t head[12];
    size_t n = 0;
    // Still on the flash, maybe the saved choice: the index is incomplete
    if (!dir.head(info.name, &info.size, head, &n)) {
      dir.note(info.name, NOTE_UNREADABLE);
      complete = false;
      continue;
    }
    bool portrait = false;
    if (gifSizeAccepted(sniffImage(head, n), &portrait)) info.portrait = portrait;
    if (!idx.put(info)) {
      dir.note(info.name, NOTE_OVER_CAP);
      complete = false;
    }
  }
  return complete && r == 0;
}

Space spaceFigures(const ImageIndex &idx, uint32_t freeBlocks, uint32_t blockSize) {
  Space sp = {0, 0, 0};
  for (int i = 0; i < idx.count; i++) sp.used += idx.items[i].size;
  sp.left = freeBlocks > RESERVE_BLOCKS ? ctzCapacity(freeBlocks - RESERVE_BLOCKS, blockSize) : 0;
  sp.total = sp.used + sp.left;
  return sp;
}

// --- Upload ------------------------------------------------------------------

int uploadStatus(UploadResult r) {
  switch (r) {
    case UP_ADDED: case UP_REPLACED: return 200;
    case UP_NO_STORAGE:              return 503;
    case UP_TOO_BIG:                 return 413;
    case UP_NO_ROOM: case UP_TOO_MANY: return 507;
    case UP_FAILED:                  return 500;
    default:                         return 400;   // no file, not a GIF, wrong size, cut short
  }
}

const char *uploadText(UploadResult r) {
  return r == UP_ADDED ? "image added" : r == UP_REPLACED ? "image replaced" : "image not added";
}

UploadResult ImageUpload::refuse(ContentStore &s, UploadResult r) {
  if (tempOpen_) s.dropTemp();
  tempOpen_ = false;
  return result_ = r;
}

UploadResult ImageUpload::checkHeader(size_t n) {
  header_ = sniffImage(head_, n);
  if (header_.kind != KIND_GIF) return UP_NOT_GIF;
  if (!gifSizeAccepted(header_, &portrait_)) return UP_WRONG_SIZE;
  return UP_PENDING;
}

UploadResult ImageUpload::start(ContentStore &s, const ImageIndex &idx, bool storage,
                                const char *sentName, uint32_t announced) {
  refuse(s, UP_PENDING);   // a previous upload left open: its temp goes
  name_[0] = '\0';
  headLen_ = 0;
  received_ = left_ = 0;
  portrait_ = false;
  header_ = {KIND_OTHER, 0, 0};
  // The name as sent, for the page's messages: cut on a character boundary
  size_t n = sentName ? strnlen(sentName, sizeof(sent_)) : 0;
  if (n == sizeof(sent_)) {
    n = sizeof(sent_) - 1;
    while (n > 0 && ((uint8_t)sentName[n] & 0xC0) == 0x80) n--;
  }
  memcpy(sent_, sentName ? sentName : "", n);
  sent_[n] = '\0';

  if (!storage) return result_ = UP_NO_STORAGE;
  if (!sentName || !*sentName) return result_ = UP_NO_FILE;
  sanitizeImageName(sentName, name_, sizeof(name_));
  // 0 = unknown size: the bytes are counted instead
  if (announced > MAX_IMAGE_BYTES + MULTIPART_SLACK) return result_ = UP_TOO_BIG;
  if (idx.find(name_) < 0 && idx.count >= MAX_IMAGES) return result_ = UP_TOO_MANY;
  // A replaced image stays until the new one is complete: it frees nothing here
  left_ = spaceFigures(idx, s.freeBlocks(), s.blockSize()).left;
  return UP_PENDING;
}

UploadResult ImageUpload::write(ContentStore &s, const uint8_t *data, size_t n) {
  received_ = n > UINT32_MAX - received_ ? UINT32_MAX : received_ + (uint32_t)n;
  if (result_ != UP_PENDING) return result_;   // refused: only counted
  if (received_ > MAX_IMAGE_BYTES) return refuse(s, UP_TOO_BIG);
  if (headLen_ < sizeof(head_)) {
    const size_t take = n < sizeof(head_) - headLen_ ? n : sizeof(head_) - headLen_;
    if (take) memcpy(head_ + headLen_, data, take);
    headLen_ += take;
    data += take;
    n -= take;
    if (headLen_ < sizeof(head_)) return UP_PENDING;
    // The first flash write: only once the signature and size passed
    const UploadResult h = checkHeader(headLen_);
    if (h != UP_PENDING) return refuse(s, h);
    if (received_ > left_) return refuse(s, UP_NO_ROOM);
    if (!s.beginTemp()) return refuse(s, UP_FAILED);
    tempOpen_ = true;
    if (!s.writeTemp(head_, headLen_)) return refuse(s, UP_FAILED);
  }
  if (received_ > left_) return refuse(s, UP_NO_ROOM);
  if (n && !s.writeTemp(data, n)) return refuse(s, UP_FAILED);
  return UP_PENDING;
}

UploadResult ImageUpload::finish(ContentStore &s, ImageIndex &idx) {
  if (result_ != UP_PENDING) return result_;
  if (headLen_ < sizeof(head_)) {
    if (received_ == 0) return refuse(s, UP_NO_FILE);
    const UploadResult h = checkHeader(headLen_);
    return refuse(s, h != UP_PENDING ? h : UP_NOT_GIF);   // too short to hold a GIF
  }
  const bool replacing = idx.find(name_) >= 0;
  if (!replacing && idx.count >= MAX_IMAGES) return refuse(s, UP_TOO_MANY);
  if (!s.commitTemp(name_)) return refuse(s, UP_FAILED);
  tempOpen_ = false;
  ImageInfo info = {};
  memcpy(info.name, name_, sizeof(info.name));
  info.size = received_;
  info.portrait = portrait_;
  idx.put(info);
  return result_ = replacing ? UP_REPLACED : UP_ADDED;
}

void ImageUpload::abort(ContentStore &s) {
  refuse(s, result_ == UP_PENDING ? UP_FAILED : result_);
}

const char *ImageUpload::name() const { return name_; }
const char *ImageUpload::sent() const { return sent_; }
uint32_t ImageUpload::received() const { return received_; }
uint32_t ImageUpload::left() const { return left_; }
GifHeader ImageUpload::header() const { return header_; }
bool ImageUpload::portrait() const { return portrait_; }

// --- Choice ------------------------------------------------------------------

void ImageChoice::load() {
  String v;
  Preferences p;   // local: never shared between tasks
  if (p.begin(NVS_NAMESPACE, true)) {
    v = p.getString(CHOICE_KEY, "");
    p.end();
  }
  // Anything a name could not be reads as "all in turn"
  snprintf(value_, sizeof(value_), "%s", storedNameValid(v.c_str()) ? v.c_str() : "");
}

const char *ImageChoice::get() const { return value_; }

bool ImageChoice::set(const char *name) {
  const char *v = name ? name : "";
  if (*v && !storedNameValid(v)) return false;
  if (strcmp(v, value_) == 0) return true;   // flash written only on a change
  Preferences p;   // local: never shared between tasks
  // NVS closed by the lock (an unfinished factory reset) keeps the choice in
  // RAM only, by design. Any other refusal keeps the old one, so the panel
  // never shows a choice that the next start would not.
  if (p.begin(NVS_NAMESPACE, false)) {
    // absent = all in turn; a key already gone counts as removed
    const bool saved = *v ? p.putString(CHOICE_KEY, v) > 0
                          : p.remove(CHOICE_KEY) || !p.isKey(CHOICE_KEY);
    p.end();
    if (!saved) return false;
  } else if (!panelLocked()) {
    return false;
  }
  snprintf(value_, sizeof(value_), "%s", v);
  return true;
}

void ImageChoice::validate(const ImageIndex &idx) {
  if (*value_ && idx.find(value_) < 0) set("");
}

bool choiceValid(const ImageIndex &idx, const char *name) {
  return !name || !*name || idx.find(name) >= 0;
}

// --- What the IMAGE screen plays -----------------------------------------------

static bool playable(const ImageIndex &idx, int i, bool portrait, const char *skip) {
  return i >= 0 && idx.items[i].portrait == portrait && !(skip && strcmp(idx.items[i].name, skip) == 0);
}

ImagePick pickImage(const ImageIndex &idx, bool storage, bool portrait, const char *choice,
                    const char *first, const char *after, const char *skip) {
  if (!storage) return {IMG_NO_STORAGE, -1};
  if (idx.count == 0) return {IMG_NO_IMAGE, -1};
  bool any = false;
  for (int i = 0; i < idx.count && !any; i++) any = idx.items[i].portrait == portrait;
  if (!any) return {IMG_OTHER_ONLY, -1};
  for (const char *wanted : {first, choice}) {
    const int i = wanted && *wanted ? idx.find(wanted) : -1;
    if (playable(idx, i, portrait, skip)) return {IMG_PLAY, i};
  }
  // In turn: the next name after the last one shown, which may be gone by now
  int wrap = -1;
  for (int i = 0; i < idx.count; i++) {
    if (!playable(idx, i, portrait, skip)) continue;
    if (wrap < 0) wrap = i;
    if (imageOrder(idx.items[i].name, after ? after : "") > 0) return {IMG_PLAY, i};
  }
  return {IMG_PLAY, wrap};
}

bool turnOver(uint32_t shownMs, bool loopEnded, bool still) {
  if (shownMs >= TURN_MAX_MS) return true;
  return shownMs >= TURN_MS && (still || loopEnded);
}

// Browsers play 0-10 ms frames at 100 ms; GIFs are drawn expecting that
uint32_t gifFrameDelay(int ms) { return ms <= 10 ? 100 : (uint32_t)ms; }

bool showsAfterUpload(bool fromPage, bool showArg, bool imagePortrait, bool panelPortrait,
                      bool imageOn) {
  return imageOn && (fromPage || showArg) && imagePortrait == panelPortrait;
}

bool Retry::due(uint32_t now) const { return at == 0 || now - at >= RETRY_MS; }
void Retry::failed(uint32_t now) { at = now ? now : 1; }

void canvasLine(uint16_t *canvas, int cw, int ch, int x0, int y, int w, const uint8_t *pixels,
                const uint16_t *palette, int transparent) {
  if (!canvas || !pixels || !palette || w <= 0 || y < 0 || y >= ch) return;
  const long long from = x0 < 0 ? -(long long)x0 : 0;
  const long long to = (long long)cw - x0 < w ? (long long)cw - x0 : w;
  // Index from the row start with clipped offsets: forming canvas + x0 first is
  // undefined behaviour for x0 < 0, which a GIF's frame rectangle can give
  uint16_t *row = canvas + (long long)y * cw;
  for (long long i = from; i < to; i++)
    if (pixels[i] != transparent) row[x0 + i] = palette[pixels[i]];
}

void canvasClear(uint16_t *canvas, int cw, int ch, int x, int y, int w, int h) {
  if (!canvas || w <= 0 || h <= 0) return;
  const long long x1 = x < 0 ? 0 : x, y1 = y < 0 ? 0 : y;
  const long long x2 = (long long)x + w < cw ? (long long)x + w : cw;
  const long long y2 = (long long)y + h < ch ? (long long)y + h : ch;
  for (long long r = y1; r < y2; r++)
    for (long long c = x1; c < x2; c++) canvas[r * cw + c] = 0;
}

// --- Factory reset -------------------------------------------------------------

bool blankFlash(const uint32_t *words, size_t count) {
  for (size_t i = 0; i < count; i++)
    if (words[i] != 0xFFFFFFFF) return false;
  return true;
}

MarkerState markerFrom(const uint32_t *words, bool readOk) {
  if (!readOk || !words) return MARK_UNREADABLE;
  return memcmp(words, RESET_MAGIC, sizeof(RESET_MAGIC)) == 0 ? MARK_PRESENT : MARK_ABSENT;
}

bool markResetPending(MarkerSector *s) {
  if (!s) return false;   // no partition holds it: no later start would resume
  uint32_t back[4];
  bool readOk = s->read(back);
  if (markerFrom(back, readOk) == MARK_PRESENT) return true;
  // Unreadable may be a marker already there: never erased blind (the
  // start that cannot write one stays locked)
  if (!readOk) return false;
  if (!s->erase() || !s->write(RESET_MAGIC)) return false;
  readOk = s->read(back);
  return markerFrom(back, readOk) == MARK_PRESENT;
}

bool runFactoryReset(ResetOps &ops) {
  // The marker first, so a cut anywhere after it resumes; cleared last, so
  // the reset is never taken as done before every erase was checked
  return ops.markPending() && ops.eraseImages() && ops.eraseCrashDump() &&
         ops.eraseSettings() && ops.clearPending();
}

bool runFactoryResetNow(ResetOps &ops) {
  // With its marker, a failure stops the run and the next start resumes it
  if (ops.markPending())
    return ops.eraseImages() && ops.eraseCrashDump() && ops.eraseSettings() && ops.clearPending();
  // Without one, nothing will: every erase runs whatever the earlier ones
  // returned. The image area that refused the marker is likely to refuse
  // its erase too, and the crash dump and the settings must not survive it.
  bool ok = ops.eraseImages();
  ok = ops.eraseCrashDump() && ok;
  ok = ops.eraseSettings() && ok;
  return ok && ops.clearPending();
}

KnobReset knobResetEnd(bool done, bool marked) {
  if (done) return KR_DONE;
  return marked ? KR_RESUME : KR_LOCKED;
}

ResetBoot resetAtBoot(MarkerState marker, ResetOps &ops) {
  if (marker == MARK_ABSENT) return RB_NORMAL;
  return runFactoryReset(ops) ? RB_DONE : RB_LOCKED;   // unreadable counts as pending
}

MountState mountFailure(const uint8_t *block0, const uint8_t *block1, bool readOk) {
  if (!readOk || !block0 || !block1) return MOUNT_FAILED;
  const bool named = !memcmp(block0 + 8, "littlefs", 8) || !memcmp(block1 + 8, "littlefs", 8);
  return named ? MOUNT_FAILED : MOUNT_NO_FS;
}

bool resetPending(bool locked, MarkerState marker) {
  return locked || marker != MARK_ABSENT;
}

StorageBoot storageBoot(bool partitionFound, bool resetPending, MountState mount,
                        bool formatTried) {
  if (!partitionFound) return BOOT_MISSING;
  if (resetPending) return BOOT_LOCKED;   // the erase is not finished: no mount, no format
  if (mount == MOUNT_NOT_TRIED) return BOOT_MOUNT;
  if (mount == MOUNT_OK) return BOOT_MOUNTED;
  return mount == MOUNT_NO_FS && !formatTried ? BOOT_FORMAT_ONCE : BOOT_UNAVAILABLE;
}
