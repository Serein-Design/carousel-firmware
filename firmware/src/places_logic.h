// SPDX-License-Identifier: AGPL-3.0-only
// The city table, data/places.bin (format: tools/places/FORMAT.md), without
// the hardware: the key a typed name folds to, the head and every block
// checked against the SHA-256 the firmware pins (places_index.h) before use,
// the search the settings page asks for while the owner types, and a city
// turned into the place, coordinates and POSIX zone the panel keeps. Where
// the bytes come from is the caller's (places.cpp: HTTPS range requests).
// Host-tested in test_places.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "places_index.h"

// FORMAT.md's key of text: "Saint-Étienne" -> "saintetienne". The build
// folds with Unicode's NFKD; the panel knows the Latin letters only
// (U+0000-024F and U+1E00-1EFF, the scripts the table's names are written
// in), and drops every other character, as NFKD then the a-z filter do for
// all but a few compatibility forms (fullwidth letters, ligatures). Cut to
// fit size, NUL included; returns the key's length.
size_t placesFold(const char *text, char *key, size_t size);

// Where data/places.bin is fetched: the copy tagged with this release in the
// public repository, so the head always matches the hash this firmware pins.
// A development build (not a vX.Y.Z version) reads devUrl when one is given
// (secrets.h's PLACES_URL_DEV), else main's copy.
void placesUrl(char *out, size_t size, const char *version, const char *devUrl);

// The query the settings page may send: 2 to 32 bytes of UTF-8
bool placesQueryOk(const char *q, size_t len);

// Reads len bytes of the file at offset into out; false when it could not
struct PlacesSource {
  virtual bool read(uint32_t offset, uint32_t len, uint8_t *out) = 0;
};

static const uint8_t PLACES_RESULTS = 5;      // suggestions under the city field
static const uint8_t PLACES_SCAN_BLOCKS = 3;  // name blocks read for one search
static const uint8_t PLACES_HOME_BONUS = 25;  // population classes: x10

struct PlaceHit {
  uint16_t id = 0;            // the city's number in the table
  bool     exact = false;     // its key, or its alias's, is the query's
  uint8_t  rank = 0;          // population class, plus PLACES_HOME_BONUS at home
  uint8_t  country = 0;       // country table number
  uint8_t  regionInCountry = 0;
  uint8_t  zone = 0;          // zone table number
  float    lat = 0, lon = 0;  // to 0.01 degree
  char     name[48] = "";     // as the table writes it, or the alias typed
  char     region[48] = "";   // "" when GeoNames names none
  char     countryName[48] = "";
};

enum PlacesError : uint8_t {
  PLACES_OK,
  PLACES_UNREACHABLE,   // a read failed: the network, another HTTPS session
  PLACES_REFUSED,       // a hash or the layout did not match: never used
  PLACES_NO_MEMORY,
};

// One table in use: the head, checked once, and the last name block and
// table block inflated, kept for the next call while the owner types (the
// same block, as "par" then "pari"). Not shared between tasks.
class PlacesTable {
 public:
  // headSha, headSize, fileSize: places_index.h's; others in tests only
  explicit PlacesTable(const uint8_t *headSha = PLACES_HEAD_SHA256,
                       uint32_t headSize = PLACES_HEAD_SIZE, uint32_t fileSize = PLACES_FILE_SIZE);
  ~PlacesTable();
  PlacesTable(const PlacesTable &) = delete;
  PlacesTable &operator=(const PlacesTable &) = delete;

  // Up to max cities whose key, or an alias's, starts with query's, the
  // exact ones first, then by population, x10 in the country home (country
  // table number, -1 none). lang: "fr" for French country names. Returns
  // how many, -1 on an error (error() says which).
  int search(PlacesSource &src, const char *query, int home, const char *lang, PlaceHit *out,
             int max);
  // City number id -> out, and its POSIX zone into zone
  bool city(PlacesSource &src, uint16_t id, const char *lang, PlaceHit &out, char *zone,
            size_t zoneSize);
  // The zone table's entry z
  bool zoneName(PlacesSource &src, uint8_t z, char *out, size_t size);
  // The country whose English or French name ends place ("Paris,
  // Île-de-France, France"), -1 none or unreadable
  int countryOf(PlacesSource &src, const char *place);

  PlacesError error() const { return error_; }
  bool open() const { return head_ != nullptr; }
  void close();   // frees the head and the blocks

 private:
  struct Slot {
    int16_t  index = -1;      // the block's place in its index, -1 empty
    uint16_t size = 0;
    uint8_t *data = nullptr;  // 4096 bytes
  };
  bool load(PlacesSource &src);
  const uint8_t *nameEntry(uint16_t i) const;
  const uint8_t *tableEntry(uint16_t i) const;
  bool block(PlacesSource &src, Slot &slot, int16_t index, uint32_t offset, uint16_t stored,
             const uint8_t *sha);
  bool nameBlock(PlacesSource &src, uint16_t i);
  int16_t blockOfCity(uint16_t id) const;
  bool findCity(PlacesSource &src, uint16_t id, PlaceHit &out);
  const char *tableItem(PlacesSource &src, uint8_t table, uint16_t number, size_t &len);
  bool names(PlacesSource &src, PlaceHit *hits, int n, const char *lang);
  bool fail(PlacesError e) { error_ = e; return false; }

  const uint8_t *headSha_;
  uint32_t headSize_, fileSize_;
  uint8_t *head_ = nullptr;
  uint16_t nameBlocks_ = 0, tableBlocks_ = 0, countries_ = 0;
  uint32_t cities_ = 0;
  uint32_t tableIndex_ = 0;   // where the table index starts in the head
  Slot names_, tables_;
  PlacesError error_ = PLACES_OK;
};

// A search's answer for the settings page: [{"id":..,"n":..,"a":..,"c":..}].
// False when it does not fit size. PLACES_JSON_SIZE holds the longest:
// five hits of three 47-byte names, every byte escaped.
static const size_t PLACES_JSON_SIZE = 2 + PLACES_RESULTS * (37 + 3 * 2 * 47);
bool placesJson(const PlaceHit *hits, int n, char *out, size_t size);

// GET /place, which the settings page polls while a city is being looked up:
// {"p":"<place>"}, {"p":""} while none is found yet, {"p":"","f":1} when
// the lookup failed (a place found wins). False when it does not fit size;
// PLACE_JSON_SIZE holds a 63-byte place, every byte escaped.
static const size_t PLACE_JSON_SIZE = 2 * 63 + 20;
bool placeJson(const char *place, bool failed, char *out, size_t size);

// "Paris, Île-de-France, France", as settings.place, cut on a character
void placeLabel(const PlaceHit &h, char *out, size_t size);

// The table's head and last blocks (about 13 KB) stay in RAM while the
// owner types, PLACES_IDLE_MS between two searches at most, and never into
// an HTTPS session (httpsNext). Measured on the
// panel: a forecast's handshake to MET took the 58 KB of byte-addressable
// RAM left to the last bytes; with the table held as well it stalled for
// 120 s ("no answer -1"), the Wi-Fi buffers and the web pages starved too.
static const uint32_t PLACES_IDLE_MS = 120000;
bool placesKeepTable(uint32_t idleMs, bool httpsNext);

// What the HTTPS task looks up in the table: a city typed and not found
// yet, unless the table said it knows none (cityNotFound); or a place an
// earlier firmware found (online, with GeoNames or Open-Meteo), which kept
// no zone. Always a suggestion chosen on the settings page (chosen): the
// city may be empty then, the HTTPS task names it after the suggestion.
bool locateWanted(const char *city, const char *place, const char *zone, bool cityNotFound,
                  bool chosen);

// The location a city found in the table gives, into the settings' fields:
// all of it for a city typed or chosen (place empty); a place an earlier
// firmware found keeps its name and coordinates and takes the zone alone,
// when the city found lies within half a degree of it (the same name may
// lead elsewhere: then nothing). False when nothing was taken.
struct Location {
  char  *place;
  size_t placeSize;
  float *lat, *lon;
  char  *zone;
  size_t zoneSize;
};
bool takeLocation(const PlaceHit &h, const char *zone, Location loc);

// The phone's zone at Wi-Fi setup ("America/Argentina/Buenos_Aires") -> its
// POSIX TZ from tz_names and the city to look up ("Buenos Aires"). False for
// a name tzdata does not know, or one without a place ("UTC", "Etc/GMT+2").
bool phoneZone(const char *iana, char *city, size_t citySize, char *zone, size_t zoneSize);

// Seconds before the table is asked again after the failures-th failure in
// a row: an hour, doubling to a day, plus spread, the panel's second of the
// hour (metSlot), so that panels refused together do not come back together
uint32_t locateRetryDelay(uint8_t failures, uint16_t spread);

// UTC offset (seconds, east positive) -> POSIX TZ, which counts the other
// way: UTC+2 is "UTC-2:00", India (UTC+5:30) "UTC-5:30"
void posixZone(char *out, size_t size, int32_t seconds);
