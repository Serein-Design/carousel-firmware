// SPDX-License-Identifier: AGPL-3.0-only
#include "places_logic.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <new>

#include <mbedtls/md.h>
#include <rom/miniz.h>   // the ESP32 ROM's inflater: no flash spent on one

#include "tz_names.h"
#include "update_logic.h"

// --- The key -----------------------------------------------------------------

// What each code point folds to, from U+00A0 and from U+1E00, made with
// build.py's fold(): a letter or digit, '.' for nothing, or a capital
// standing for two letters (PAIRS)
static const char LATIN[] =
    "..........a.......23.....1o.QHR.aaaaaaAceeeeiiiidnooooo.ouuuuyTS"
    "aaaaaaAceeeeiiiidnooooo.ouuuuyTyaaaaaaccccccccddddeeeeeeeeeegggg"
    "gggghhhhiiiiiiiiiiIIjjkkkllllllllllnnnnnnn..ooooooOOrrrrrrssssss"
    "ssttttttuuuuuuuuuuuuwwyyyzzzzzzs..................f............."
    "oo.............uu...................DDDLLLNNNaaiioouuuuuuuuuu.aa"
    "aaAA..ggkkoooo..jDDDgg..nnaaAAooaaaaeeeeiiiioooorrrruuuusstt..hh"
    "......aaeeooooooooyy............................";
static const char VIET[] =
    "aabbbbbbccddddddddddeeeeeeeeeeffgghhhhhhhhhhiiiikkkkkkllllllllmm"
    "mmmmnnnnnnnnoooooooopppprrrrrrrrssssssssssttttttttuuuuuuuuuuvvvv"
    "wwwwwwwwwwxxxxyyzzzzzzhtwyas....aaaaaaaaaaaaaaaaaaaaaaaaeeeeeeee"
    "eeeeeeeeiiiioooooooooooooooooooooooouuuuuuuuuuuuuuyyyyyyyy......";
static_assert(sizeof(LATIN) - 1 == 0x250 - 0xA0 && sizeof(VIET) - 1 == 0x100, "one per code point");
static const char PAIRS[] = "AaeTthSssIijOoeDdzLljNnjQ14H12R34";

// The code point at p and its length; 0 for bytes that are not UTF-8
// (a sequence cut or overlong, a surrogate, past U+10FFFF)
static int utf8Next(const uint8_t *p, uint32_t &cp) {
  const uint8_t b = p[0];
  if (b < 0x80) { cp = b; return 1; }
  const int len = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC2 ? 2 : 0;
  if (!len || b > 0xF4) return 0;
  cp = b & (0x3F >> (len - 1));
  for (int i = 1; i < len; i++) {
    if ((p[i] & 0xC0) != 0x80) return 0;
    cp = cp << 6 | (p[i] & 0x3F);
  }
  if ((len == 3 && (cp < 0x800 || (cp >= 0xD800 && cp < 0xE000))) ||
      (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)))
    return 0;
  return len;
}

size_t placesFold(const char *text, char *key, size_t size) {
  size_t n = 0;
  auto put = [&](char c) { if (n + 1 < size) key[n++] = c; };
  for (const uint8_t *p = (const uint8_t *)text; *p;) {
    uint32_t cp = 0;
    const int len = utf8Next(p, cp);
    p += len ? len : 1;
    if (!len) continue;
    char c = '.';
    if (cp < 0x80) c = isalnum((int)cp) ? (char)tolower((int)cp) : '.';
    else if (cp >= 0xA0 && cp < 0x250) c = LATIN[cp - 0xA0];
    else if (cp >= 0x1E00 && cp < 0x1F00) c = VIET[cp - 0x1E00];
    if (c == '.') continue;
    if (c >= 'A' && c <= 'Z') {
      const char *pair = strchr(PAIRS, c);
      put(pair[1]);
      put(pair[2]);
    } else {
      put(c);
    }
  }
  if (size) key[n] = '\0';
  return n;
}

void placesUrl(char *out, size_t size, const char *version, const char *devUrl) {
  if (!isReleaseVersion(version) && devUrl && devUrl[0]) {
    snprintf(out, size, "%s", devUrl);
    return;
  }
  snprintf(out, size,
           "https://raw.githubusercontent.com/Serein-Design/carousel-firmware/%s/data/places.bin",
           isReleaseVersion(version) ? version : "main");
}

bool placesQueryOk(const char *q, size_t len) {
  if (len < 2 || len > 32 || strlen(q) != len) return false;
  for (const uint8_t *p = (const uint8_t *)q; *p;) {
    uint32_t cp;
    const int n = utf8Next(p, cp);
    if (!n) return false;
    p += n;
  }
  return true;
}

// --- Reading the file --------------------------------------------------------

static const uint8_t MAGIC[4] = {'P', 'L', 'C', 'S'};
static const uint16_t BLOCK = 4096;
static const uint32_t HEADER = 36, NAME_ENTRY = 41, TABLE_ENTRY = 41;
enum : uint8_t { TABLE_COUNTRIES, TABLE_REGIONS, TABLE_ZONES };

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t *p) {
  return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool sha256Is(const uint8_t *data, size_t len, const uint8_t *want) {
  uint8_t got[32];
  return mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), data, len, got) == 0 &&
         memcmp(got, want, 32) == 0;
}

PlacesTable::PlacesTable(const uint8_t *headSha, uint32_t headSize, uint32_t fileSize)
    : headSha_(headSha), headSize_(headSize), fileSize_(fileSize) {}

PlacesTable::~PlacesTable() { close(); }

void PlacesTable::close() {
  free(head_);
  free(names_.data);
  free(tables_.data);
  head_ = nullptr;
  names_ = Slot();
  tables_ = Slot();
}

// The head, once: read, its hash checked, then its layout, which the hash
// vouches for but this firmware's code must agree with
bool PlacesTable::load(PlacesSource &src) {
  error_ = PLACES_OK;
  if (head_) return true;
  uint8_t *h = (uint8_t *)malloc(headSize_);
  if (!h) return fail(PLACES_NO_MEMORY);
  if (!src.read(0, headSize_, h)) {
    free(h);
    return fail(PLACES_UNREACHABLE);
  }
  bool ok = sha256Is(h, headSize_, headSha_) && headSize_ >= HEADER &&
            memcmp(h, MAGIC, 4) == 0 && u16(h + 4) == 1 && u16(h + 6) == BLOCK &&
            u32(h + 12) == headSize_;
  uint32_t at = HEADER;
  const uint16_t count = ok ? u16(h + 16) : 0;
  for (uint16_t i = 0; ok && i < count; i++) {
    ok = at + NAME_ENTRY <= headSize_;
    if (ok) at += NAME_ENTRY + h[at + 40];
  }
  const uint16_t tables = ok ? u16(h + 18) : 0;
  if (!ok || at + (uint32_t)tables * TABLE_ENTRY != headSize_ || !count) {
    free(h);
    return fail(PLACES_REFUSED);
  }
  head_ = h;
  nameBlocks_ = count;
  tableBlocks_ = tables;
  tableIndex_ = at;
  cities_ = u32(h + 20);
  countries_ = u16(h + 28);
  return true;
}

const uint8_t *PlacesTable::nameEntry(uint16_t i) const {
  const uint8_t *e = head_ + HEADER;
  while (i--) e += NAME_ENTRY + e[40];
  return e;
}

const uint8_t *PlacesTable::tableEntry(uint16_t i) const {
  return head_ + tableIndex_ + (uint32_t)i * TABLE_ENTRY;
}

// The block into slot, unless it holds it already: read, checked against
// its hash before a byte of it is inflated
bool PlacesTable::block(PlacesSource &src, Slot &slot, int16_t index, uint32_t offset,
                        uint16_t stored, const uint8_t *sha) {
  if (slot.index == index) return true;
  slot.index = -1;
  if (offset + stored > fileSize_) return fail(PLACES_REFUSED);
  if (!slot.data && !(slot.data = (uint8_t *)malloc(BLOCK))) return fail(PLACES_NO_MEMORY);
  uint8_t *in = (uint8_t *)malloc(stored);
  if (!in) return fail(PLACES_NO_MEMORY);
  if (!src.read(offset, stored, in)) {
    free(in);
    return fail(PLACES_UNREACHABLE);
  }
  if (!sha256Is(in, stored, sha)) {
    free(in);
    return fail(PLACES_REFUSED);
  }
  // About 11 KB, for the inflating only
  tinfl_decompressor *d = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
  if (!d) {
    free(in);
    return fail(PLACES_NO_MEMORY);
  }
  tinfl_init(d);
  size_t inSize = stored, outSize = BLOCK;
  const tinfl_status st = tinfl_decompress(d, in, &inSize, slot.data, slot.data, &outSize,
                                           TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  free(d);
  free(in);
  if (st != TINFL_STATUS_DONE) return fail(PLACES_REFUSED);
  slot.index = index;
  slot.size = (uint16_t)outSize;
  return true;
}

bool PlacesTable::nameBlock(PlacesSource &src, uint16_t i) {
  const uint8_t *e = nameEntry(i);
  return block(src, names_, (int16_t)i, u32(e), u16(e + 4), e + 8);
}

// A name block's records, one by one: the name rebuilt from the one before
namespace {
struct Records {
  const uint8_t *p, *end;
  char name[128];
  uint8_t len = 0;
  bool alias = false;
  const uint8_t *data = nullptr;   // 8 bytes of a city, 2 of an alias
  Records(const uint8_t *b, uint16_t size) : p(b), end(b + size) { name[0] = '\0'; }
  // False at the block's end, and at a record that does not fit it or the
  // name (the build refuses longer names): then broken() holds
  bool next() {
    if (end - p < 2) return false;
    const uint8_t kept = p[0], n = p[1] & 0x7F;
    alias = p[1] & 0x80;
    if (kept > len || kept + n >= (int)sizeof(name) || end - p < 2 + n + (alias ? 2 : 8))
      return false;
    memcpy(name + kept, p + 2, n);
    len = kept + n;
    name[len] = '\0';
    data = p + 2 + n;
    p = data + (alias ? 2 : 8);
    return true;
  }
  bool broken() const { return p != end; }
};
}  // namespace

static void copyCut(char *out, size_t size, const char *s);

static void fromCity(PlaceHit &h, uint16_t id, const uint8_t *d, const char *name) {
  h.id = id;
  h.lat = (int16_t)u16(d) / 100.0f;
  h.lon = (int16_t)u16(d + 2) / 100.0f;
  h.country = d[4];
  h.regionInCountry = d[5];
  h.zone = d[6];
  h.rank = d[7];
  copyCut(h.name, sizeof(h.name), name);
  h.region[0] = h.countryName[0] = '\0';
}

// The block city id is in: the last whose first city is not above it
int16_t PlacesTable::blockOfCity(uint16_t id) const {
  int16_t found = -1;
  const uint8_t *e = head_ + HEADER;
  for (uint16_t i = 0; i < nameBlocks_; i++, e += NAME_ENTRY + e[40])
    if (u16(e + 6) <= id) found = (int16_t)i;
  return found;
}

bool PlacesTable::findCity(PlacesSource &src, uint16_t id, PlaceHit &out) {
  const int16_t b = id < cities_ ? blockOfCity(id) : -1;
  if (b < 0) return false;
  if (!nameBlock(src, (uint16_t)b)) return false;
  uint16_t n = u16(nameEntry((uint16_t)b) + 6);
  Records r(names_.data, names_.size);
  while (r.next()) {
    if (r.alias) continue;
    if (n++ == id) {
      fromCity(out, id, r.data, r.name);
      return true;
    }
  }
  return fail(PLACES_REFUSED);   // the index promised it here
}

// Entry number of table, as a pointer into its inflated block; len: the
// bytes left in the block from there
const char *PlacesTable::tableItem(PlacesSource &src, uint8_t table, uint16_t number,
                                   size_t &len) {
  int16_t found = -1;
  for (uint16_t i = 0; i < tableBlocks_; i++) {
    const uint8_t *e = tableEntry(i);
    if (e[0] == table && u16(e + 1) <= number) found = (int16_t)i;
  }
  if (found < 0) return nullptr;
  const uint8_t *e = tableEntry((uint16_t)found);
  if (!block(src, tables_, found, u32(e + 3), u16(e + 7), e + 9)) return nullptr;
  const char *p = (const char *)tables_.data, *end = p + tables_.size;
  // An entry: a country's 4 bytes then two names, else one name
  const int strings = table == TABLE_COUNTRIES ? 2 : 1;
  for (uint16_t skip = number - u16(e + 1); p < end; skip--) {
    const char *q = p + (table == TABLE_COUNTRIES ? 4 : 0);
    // Every string ended inside the block, or the entry is refused: frName
    // reads the second right after the first
    int found = 0;
    for (; found < strings && q < end; found++) {
      const char *nul = (const char *)memchr(q, '\0', end - q);
      if (!nul) break;
      q = nul + 1;
    }
    if (found < strings) break;
    if (!skip) {
      len = q - p;
      return p;
    }
    p = q;
  }
  fail(PLACES_REFUSED);
  return nullptr;
}

// Country and region names for hits: every country first, then every
// region, so that one table block held serves the next hit
static const char *frName(const char *en) { return en + strlen(en) + 1; }

bool PlacesTable::names(PlacesSource &src, PlaceHit *hits, int n, const char *lang) {
  uint16_t region[PLACES_RESULTS];
  if (n > PLACES_RESULTS) n = PLACES_RESULTS;
  const bool fr = lang && strcmp(lang, "fr") == 0;
  for (int i = 0; i < n; i++) {
    size_t len;
    const char *c = tableItem(src, TABLE_COUNTRIES, hits[i].country, len);
    if (!c) return false;
    copyCut(hits[i].countryName, sizeof(hits[i].countryName), fr ? frName(c + 4) : c + 4);
    region[i] = u16((const uint8_t *)c + 2) + hits[i].regionInCountry;
  }
  for (int i = 0; i < n; i++) {
    size_t len;
    const char *r = tableItem(src, TABLE_REGIONS, region[i], len);
    if (!r) return false;
    copyCut(hits[i].region, sizeof(hits[i].region), r);
  }
  return true;
}

// a before b: the exact name first, then the larger
static bool better(const PlaceHit &a, const PlaceHit &b) {
  return a.exact != b.exact ? a.exact : a.rank > b.rank;
}

// h into the best n of max, once per city
static void insert(PlaceHit *out, int &n, int max, const PlaceHit &h) {
  for (int j = 0; j < n; j++) {
    if (out[j].id != h.id) continue;
    if (!better(h, out[j])) return;
    memmove(out + j, out + j + 1, (n - j - 1) * sizeof(PlaceHit));
    n--;
    break;
  }
  int at = n;
  while (at > 0 && better(h, out[at - 1])) at--;
  if (at >= max) return;
  if (n == max) n--;
  memmove(out + at + 1, out + at, (n - at) * sizeof(PlaceHit));
  out[at] = h;
  n++;
}

static bool keyBelow(const uint8_t *key, uint8_t len, const char *q) {
  const size_t ql = strlen(q);
  const int c = memcmp(key, q, std::min((size_t)len, ql));
  return c < 0 || (c == 0 && len < ql);
}

int PlacesTable::search(PlacesSource &src, const char *query, int home, const char *lang,
                        PlaceHit *out, int max) {
  if (!load(src)) return -1;
  char q[72];
  const size_t ql = placesFold(query, q, sizeof(q));
  if (!ql || max <= 0) return 0;
  if (max > PLACES_RESULTS) max = PLACES_RESULTS;
  // From the last block whose first key is below the query's: the one
  // before may end with that key, a first key equal to it is not enough
  uint16_t start = 0;
  const uint8_t *e = head_ + HEADER;
  for (uint16_t i = 0; i < nameBlocks_; i++, e += NAME_ENTRY + e[40])
    if (keyBelow(e + 41, e[40], q)) start = i;
  // Aliases found: their city is looked up after, in key order the exact
  // ones come first
  struct Alias { uint16_t city; bool exact; char name[48]; };
  Alias aliases[PLACES_RESULTS];
  int n = 0, na = 0;
  bool past = false;
  // A deliberate ceiling: a two-letter start ("sa") spans more blocks than
  // the three read, so a large city further on may be missed until the
  // next letter; reading on would cost a range request per 4 KB.
  for (uint16_t b = start; !past && b < nameBlocks_ && b < start + PLACES_SCAN_BLOCKS; b++) {
    const uint8_t *entry = nameEntry(b);
    if (b > start && (entry[40] < ql || memcmp(entry + 41, q, ql) != 0)) break;
    if (!nameBlock(src, b)) return -1;
    uint16_t city = u16(entry + 6);
    Records r(names_.data, names_.size);
    while (r.next()) {
      const uint16_t id = r.alias ? 0 : city++;
      char k[160];
      placesFold(r.name, k, sizeof(k));
      const int c = strncmp(k, q, ql);
      if (c > 0) { past = true; break; }   // sorted: nothing further on
      if (c < 0) continue;
      const bool exact = k[ql] == '\0';
      if (r.alias) {
        if (na == PLACES_RESULTS) continue;
        Alias &a = aliases[na++];
        a.city = u16(r.data);
        a.exact = exact;
        copyCut(a.name, sizeof(a.name), r.name);
        continue;
      }
      PlaceHit h;
      fromCity(h, id, r.data, r.name);
      h.exact = exact;
      if (h.country == home) h.rank += PLACES_HOME_BONUS;
      insert(out, n, max, h);
    }
    if (!past && r.broken()) {
      fail(PLACES_REFUSED);
      return -1;
    }
  }
  // By city, so that one name block held serves the next (five at most:
  // an insertion sort, without std::sort's kilobyte of code)
  for (int i = 1; i < na; i++)
    for (int j = i; j > 0 && aliases[j].city < aliases[j - 1].city; j--)
      std::swap(aliases[j], aliases[j - 1]);
  for (int i = 0; i < na; i++) {
    PlaceHit h;
    if (!findCity(src, aliases[i].city, h)) return -1;
    h.exact = aliases[i].exact;
    memcpy(h.name, aliases[i].name, sizeof(h.name));
    if (h.country == home) h.rank += PLACES_HOME_BONUS;
    insert(out, n, max, h);
  }
  // A city found once by its name and once by an alias: the better kept
  if (!names(src, out, n, lang)) return -1;
  return n;
}

bool PlacesTable::city(PlacesSource &src, uint16_t id, const char *lang, PlaceHit &out,
                       char *zone, size_t zoneSize) {
  if (!load(src) || !findCity(src, id, out) || !names(src, &out, 1, lang)) return false;
  return zoneName(src, out.zone, zone, zoneSize);
}

bool PlacesTable::zoneName(PlacesSource &src, uint8_t z, char *out, size_t size) {
  if (!load(src)) return false;
  size_t len;
  const char *p = tableItem(src, TABLE_ZONES, z, len);
  if (!p || strlen(p) >= size) return p ? fail(PLACES_REFUSED) : false;
  memcpy(out, p, strlen(p) + 1);
  return true;
}

int PlacesTable::countryOf(PlacesSource &src, const char *place) {
  const char *comma = strrchr(place, ',');
  const char *name = comma ? comma + 1 : place;
  while (*name == ' ') name++;
  if (!*name || !load(src)) return -1;
  for (uint16_t number = 0; number < countries_; number++) {
    size_t len;
    const char *c = tableItem(src, TABLE_COUNTRIES, number, len);
    if (!c) return -1;
    if (strcmp(c + 4, name) == 0 || strcmp(frName(c + 4), name) == 0) return number;
  }
  return -1;
}

// --- The answers -------------------------------------------------------------

// Drops a character cut short at the end of s: a cut to fit can fall inside
// a UTF-8 sequence, whose first half would be stored and drawn
static void dropCutChar(char *s) {
  size_t lead = strlen(s);
  while (lead > 0 && ((uint8_t)s[lead - 1] & 0xC0) == 0x80) lead--;
  if (!lead) return;
  const uint8_t b = (uint8_t)s[--lead];
  const size_t len = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;
  if (strlen(s + lead) < len) s[lead] = '\0';
}

static void copyCut(char *out, size_t size, const char *s) {
  snprintf(out, size, "%s", s);
  dropCutChar(out);
}

bool placesJson(const PlaceHit *hits, int n, char *out, size_t size) {
  size_t at = 0;
  bool fits = true;
  auto put = [&](char c) {
    if (at + 1 < size) out[at++] = c;
    else fits = false;
  };
  auto text = [&](const char *s) {
    put('"');
    for (; *s; s++) {
      if ((uint8_t)*s < 0x20) continue;   // none in the table; never sent raw
      if (*s == '"' || *s == '\\') put('\\');
      put(*s);
    }
    put('"');
  };
  put('[');
  for (int i = 0; i < n; i++) {
    char id[16];
    snprintf(id, sizeof(id), "%s{\"id\":%u", i ? "," : "", (unsigned)hits[i].id);
    for (const char *p = id; *p; p++) put(*p);
    for (const char *p = ",\"n\":"; *p; p++) put(*p);
    text(hits[i].name);
    for (const char *p = ",\"a\":"; *p; p++) put(*p);
    text(hits[i].region);
    for (const char *p = ",\"c\":"; *p; p++) put(*p);
    text(hits[i].countryName);
    put('}');
  }
  put(']');
  if (size) out[at] = '\0';
  return fits;
}

bool placeJson(const char *place, bool failed, char *out, size_t size) {
  size_t at = 0;
  bool fits = true;
  auto put = [&](char c) {
    if (at + 1 < size) out[at++] = c;
    else fits = false;
  };
  for (const char *p = "{\"p\":\""; *p; p++) put(*p);
  for (const char *p = place; *p; p++) {
    if ((uint8_t)*p < 0x20) continue;
    if (*p == '"' || *p == '\\') put('\\');
    put(*p);
  }
  put('"');
  if (failed && !place[0]) {
    for (const char *p = ",\"f\":1"; *p; p++) put(*p);
  }
  put('}');
  if (size) out[at] = '\0';
  return fits;
}

void placeLabel(const PlaceHit &h, char *out, size_t size) {
  // The region too: it is how one notices the wrong Springfield
  if (h.region[0]) snprintf(out, size, "%s, %s, %s", h.name, h.region, h.countryName);
  else             snprintf(out, size, "%s, %s", h.name, h.countryName);
  dropCutChar(out);
}

bool placesKeepTable(uint32_t idleMs, bool httpsNext) {
  return !httpsNext && idleMs < PLACES_IDLE_MS;
}

bool locateWanted(const char *city, const char *place, const char *zone, bool cityNotFound,
                  bool chosen) {
  return chosen || (city[0] && !cityNotFound && (!place[0] || !zone[0]));
}

bool takeLocation(const PlaceHit &h, const char *zone, Location loc) {
  if (loc.place[0]) {
    if (fabsf(*loc.lat - h.lat) > 0.5f || fabsf(*loc.lon - h.lon) > 0.5f) return false;
  } else {
    placeLabel(h, loc.place, loc.placeSize);
    *loc.lat = h.lat;
    *loc.lon = h.lon;
  }
  snprintf(loc.zone, loc.zoneSize, "%s", zone);
  return true;
}

bool phoneZone(const char *iana, char *city, size_t citySize, char *zone, size_t zoneSize) {
  const char *posix = tzPosix(iana);
  if (!posix) return false;
  snprintf(zone, zoneSize, "%s", posix);
  // "Europe/Paris": the city; "UTC", "Etc/GMT+2": a zone with no place
  const char *slash = strrchr(iana, '/');
  if (!slash || strncmp(iana, "Etc/", 4) == 0) {
    if (citySize) city[0] = '\0';
    return true;
  }
  snprintf(city, citySize, "%s", slash + 1);
  for (char *c = city; *c; c++)
    if (*c == '_') *c = ' ';
  return true;
}

uint32_t locateRetryDelay(uint8_t failures, uint16_t spread) {
  const uint32_t day = 24 * 3600, jitter = spread % 3600;
  const uint8_t n = failures ? failures - 1 : 0;
  return (n >= 5 ? day : std::min(day, 3600u << n)) + jitter;
}

// POSIX's offset: west positive. "-2:00" for UTC+2
void posixZone(char *out, size_t size, int32_t seconds) {
  const long a = labs((long)seconds);
  snprintf(out, size, "UTC%c%ld:%02ld", seconds < 0 ? '+' : '-', a / 3600, a % 3600 / 60);
}
