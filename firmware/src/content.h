// SPDX-License-Identifier: AGPL-3.0-only
// Custom images on the panel: the littlefs partition mounted by label, the
// index of /img, the saved choice, the upload and the playback file, plus the
// factory reset's raw flash steps. The rules live in content_logic (host
// tested); this module binds them to the flash.
//
// Thread safety: one recursive mutex guards the index, the choice and the
// playback file. Calls from loop() wait at most 20 ms for it and report busy
// (below); other tasks wait for it, and every holder keeps it for one bounded
// step (a frame, a copy, a delete, a rename). Not thread-safe where noted.
#pragma once

#include <Arduino.h>
#include <esp_partition.h>

#include "content_logic.h"

void contentBegin();                        // setup only, after displayBegin() and loadSettings()
bool contentAvailable();                    // false: missing, locked or unmountable
// Alphabetical copy of the index; -1: busy (loop task only), try again
int  contentList(ImageInfo *out, int max);
Space contentSpace();                       // {0, 0, 0}: not available or busy
uint32_t contentGeneration();               // moves on every add, replace and delete
// Storage available and at least one image drawn for that orientation: the
// rotation skips IMAGE otherwise. Never waits (an atomic kept with the index).
bool contentPlayable(bool portrait);
// The stored file, block by block (512 bytes), each block read under the area's lock;
// false: not stored or unreadable (nothing sent). write returns false when the client is gone.
// Also false when cut short after some blocks: the client gone, or the index
// changed meanwhile (a replace would otherwise mix two files). Web task only.
bool contentFileSend(const char *name, bool (*write)(const uint8_t *data, size_t n, void *ctx), void *ctx);

// One upload at a time, web task only. Start and finish hold the mutex; the
// chunks in between write the temp file only and never take it.
UploadResult contentUploadStart(const char *sentName, uint32_t announced);
UploadResult contentUploadWrite(const uint8_t *data, size_t n);
UploadResult contentUploadFinish();
void contentUploadAbort();
const ImageUpload &contentUpload();         // the last upload, for the page's notice

bool contentDelete(const char *name);       // false: unknown or not available
bool contentChoose(const char *name);       // nullptr or "" = all; false: unknown, refused or busy
void contentChoice(char *out, size_t size); // "" = all in turn; never waits
void contentShowFirst(const char *name);    // a just-added image, shown first for one turn
// What the IMAGE screen plays next (consumes the image of contentShowFirst);
// name receives the image when state is IMG_PLAY and index >= 0. Busy (loop
// task only): {IMG_PLAY, -1}, as for a broken image: pick again later.
ImagePick contentPick(bool portrait, const char *after, const char *skip, char *name, size_t size);

// Playback: display loop only, every call between contentPlayLock() and
// contentPlayUnlock(). Delete and replace close the file first when it is
// their target (esp_littlefs refuses to unlink or rename an open file), so
// contentPlayValid() turns false and the screen picks again.
bool contentPlayOpen(const char *name, int32_t *size);
int32_t contentPlayRead(uint8_t *buf, int32_t len);   // bytes read; 0 once closed
int32_t contentPlaySeek(int32_t pos);                 // new position; -1 on failure
bool contentPlayValid();
void contentPlayClose();
bool contentPlayLock();                     // try-lock, never waits
void contentPlayUnlock();

// Factory reset marker: the last 4 KB sector of the littlefs partition,
// written raw. It survives the NVS erase and is erased last; the image area
// proper is everything before it. A board whose table has no littlefs
// partition has no images and no marker: contentMarkReset() then fails (the
// knob's reset runs every erase and locks on a failure), the others succeed.
// contentMarkReset is for setup() only, before webBegin(): an upload that
// arrived meanwhile is dropped, and a busy mutex refuses the marker.
bool contentMarkReset();                    // unmounts the area first; kept if there, else written and read back
bool contentClearReset();                   // erased and read back blank
MarkerState contentResetMarker();           // raw read, no LittleFS, no NVS; read error = UNREADABLE
// The image area at the flash level, unmounted first, read back blank (3 attempts)
bool contentErase();
// Erases [from, from + len) of a partition and reads it back blank, 3 attempts.
// Shared by the image area and the factory reset's crash dump erase.
bool flashEraseVerified(const esp_partition_t *p, size_t from, size_t len);
