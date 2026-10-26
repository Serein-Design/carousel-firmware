// SPDX-License-Identifier: AGPL-3.0-only
// The city table without the panel: the key a typed name folds to, the
// search the settings page asks for (ranking, aliases, the answer's JSON and
// its limits), every block checked before use (a changed byte refused), a
// chosen or typed city turned into the place, its coordinates and its zone,
// and the phone's zone at setup. Read from the committed data/places.bin,
// as the panel reads it from the release: in pieces.
//
//   pio test -e native -f test_places
#include <unity.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <string>
#include <vector>

#include "../../src/places_index.cpp"
#include "../../src/places_logic.cpp"
#include "../../src/tz_names.cpp"
#include "../../src/update_logic.cpp"

// data/places.bin as the panel gets it, range by range; corrupt: a byte
// changed on the way (-1 none), fail: no answer at all
struct FileSource : PlacesSource {
  std::vector<uint8_t> bytes;
  long corrupt = -1;
  bool fail = false;
  int reads = 0;
  FileSource() {
    FILE *f = fopen("../data/places.bin", "rb");   // pio test -d firmware runs in firmware/
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "data/places.bin");
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
    fclose(f);
  }
  bool read(uint32_t offset, uint32_t len, uint8_t *out) override {
    reads++;
    if (fail || offset + len > bytes.size()) return false;
    memcpy(out, bytes.data() + offset, len);
    if (corrupt >= (long)offset && corrupt < (long)(offset + len)) out[corrupt - offset] ^= 0x01;
    return true;
  }
};

static int FRANCE, SPAIN, UK, USA;   // country table numbers, read in setUp

void setUp() {}
void tearDown() {}

static std::string fold(const char *s) {
  char key[64];
  placesFold(s, key, sizeof(key));
  return key;
}

static void test_fold() {
  // FORMAT.md's own examples
  TEST_ASSERT_EQUAL_STRING("saintetienne", fold("Saint-\xC3\x89tienne").c_str());
  TEST_ASSERT_EQUAL_STRING("saintetienne", fold("saint etienne").c_str());
  TEST_ASSERT_EQUAL_STRING("lodz", fold("\xC5\x81\xC3\xB3" "d\xC5\xBA").c_str());   // Łódź
  // The letters NFKD does not split, capitals too
  TEST_ASSERT_EQUAL_STRING("strasse", fold("Stra\xC3\x9F" "e").c_str());              // Straße
  TEST_ASSERT_EQUAL_STRING("aeroskobing", fold("\xC3\x86r\xC3\xB8sk\xC3\xB8" "bing").c_str());
  TEST_ASSERT_EQUAL_STRING("munchen", fold("M\xC3\xBCnchen").c_str());
  TEST_ASSERT_EQUAL_STRING("thorshofn", fold("\xC3\x9E\xC3\xB3rsh\xC3\xB6" "fn").c_str());
  // Vietnamese, two marks on one letter: Hồ Chí Minh
  TEST_ASSERT_EQUAL_STRING("hochiminh", fold("H\xE1\xBB\x93 Ch\xC3\xAD Minh").c_str());
  // Digits stay; other scripts, punctuation and broken UTF-8 go
  TEST_ASSERT_EQUAL_STRING("paris13", fold("Paris 13\xC2\xB0").c_str());
  TEST_ASSERT_EQUAL_STRING("", fold("\xD0\x9C\xD0\xBE\xD1\x81\xD0\xBA\xD0\xB2\xD0\xB0").c_str());
  TEST_ASSERT_EQUAL_STRING("ab", fold("a\xC3" "b\xFF").c_str());
  // Cut to fit, NUL included
  char small[4];
  TEST_ASSERT_EQUAL(3, placesFold("Lyon", small, sizeof(small)));
  TEST_ASSERT_EQUAL_STRING("lyo", small);
}

static void test_url() {
  char url[160];
  const char *dev = "http://192.168.1.20:8000/places.bin";
  // A release reads its own tag's copy, whatever secrets.h says
  placesUrl(url, sizeof(url), "v0.3.0", dev);
  TEST_ASSERT_EQUAL_STRING(
      "https://raw.githubusercontent.com/Serein-Design/carousel-firmware/v0.3.0/data/places.bin", url);
  // A development build: the override, else main's copy
  placesUrl(url, sizeof(url), "v0.2.0-4-g6136973-dirty", dev);
  TEST_ASSERT_EQUAL_STRING(dev, url);
  placesUrl(url, sizeof(url), "v0.2.0-4-g6136973", "");
  TEST_ASSERT_EQUAL_STRING(
      "https://raw.githubusercontent.com/Serein-Design/carousel-firmware/main/data/places.bin", url);
  placesUrl(url, sizeof(url), "dev", nullptr);
  TEST_ASSERT_EQUAL_STRING(
      "https://raw.githubusercontent.com/Serein-Design/carousel-firmware/main/data/places.bin", url);
}

static void test_query_limits() {
  TEST_ASSERT_TRUE(placesQueryOk("pa", 2));
  TEST_ASSERT_TRUE(placesQueryOk("\xC3\xA9", 2));       // é: one character, two bytes
  TEST_ASSERT_FALSE(placesQueryOk("p", 1));
  TEST_ASSERT_FALSE(placesQueryOk("", 0));
  const std::string b32(32, 'a'), b33(33, 'a');
  TEST_ASSERT_TRUE(placesQueryOk(b32.c_str(), b32.size()));
  TEST_ASSERT_FALSE(placesQueryOk(b33.c_str(), b33.size()));
  // Not UTF-8: a sequence cut, a stray byte, an overlong form, a NUL inside
  TEST_ASSERT_FALSE(placesQueryOk("pa\xC3", 3));
  TEST_ASSERT_FALSE(placesQueryOk("pa\xBF", 3));
  TEST_ASSERT_FALSE(placesQueryOk("pa\xC0\xAF", 4));
  TEST_ASSERT_FALSE(placesQueryOk("pa\0is", 5));
}

static void test_search_ranks_exact_then_population() {
  FileSource src;
  PlacesTable t;
  PlaceHit h[PLACES_RESULTS];
  // Both Paris, the larger first
  TEST_ASSERT_EQUAL(2, t.search(src, "Paris", -1, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL_STRING("Paris", h[0].name);
  TEST_ASSERT_EQUAL_STRING("\xC3\x8Ele-de-France", h[0].region);
  TEST_ASSERT_EQUAL_STRING("France", h[0].countryName);
  TEST_ASSERT_TRUE(h[0].exact);
  TEST_ASSERT_EQUAL_STRING("Texas", h[1].region);
  TEST_ASSERT_EQUAL_STRING("United States", h[1].countryName);
  // A prefix: five at most, by population
  TEST_ASSERT_EQUAL(PLACES_RESULTS, t.search(src, "pari", -1, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL_STRING("Paris", h[0].name);
  TEST_ASSERT_EQUAL_STRING("Parintins", h[1].name);
  for (int i = 1; i < PLACES_RESULTS; i++) TEST_ASSERT_TRUE(h[i - 1].rank >= h[i].rank);
  // The exact name before a larger one it starts
  TEST_ASSERT_EQUAL(3, t.search(src, "bath", -1, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL_STRING("Bath", h[0].name);
  TEST_ASSERT_EQUAL_STRING("Bathinda", h[1].name);
  TEST_ASSERT_TRUE(h[0].rank < h[1].rank);
  // Fewer asked, fewer given
  TEST_ASSERT_EQUAL(1, t.search(src, "pari", -1, "en", h, 1));
  TEST_ASSERT_EQUAL_STRING("Paris", h[0].name);
  // None: an empty answer, not an error
  TEST_ASSERT_EQUAL(0, t.search(src, "Xqzv", -1, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL(0, t.search(src, "zzzz", -1, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL(0, t.search(src, "--", -1, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL(PLACES_OK, t.error());
  // A two-letter start spans more blocks than are read: an answer all the same
  TEST_ASSERT_EQUAL(PLACES_RESULTS, t.search(src, "sa", -1, "en", h, PLACES_RESULTS));
}

static void test_home_country_counts_ten_times() {
  FileSource src;
  PlacesTable t;
  PlaceHit h[PLACES_RESULTS];
  // Venezuela's is larger; in Spain, Spain's comes first
  TEST_ASSERT_EQUAL(3, t.search(src, "valencia", -1, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL_STRING("Venezuela", h[0].countryName);
  TEST_ASSERT_EQUAL(3, t.search(src, "valencia", SPAIN, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL_STRING("Spain", h[0].countryName);
  TEST_ASSERT_EQUAL_STRING("Venezuela", h[1].countryName);
  // Ten times is not a hundred: Paris, Texas stays behind Paris in the US
  TEST_ASSERT_EQUAL(2, t.search(src, "paris", USA, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL_STRING("France", h[0].countryName);
  // The country of the place, in English or French as it was found
  TEST_ASSERT_EQUAL(SPAIN, t.countryOf(src, "Madrid, Madrid, Spain"));
  TEST_ASSERT_EQUAL(SPAIN, t.countryOf(src, "Madrid, Madrid, Espagne"));
  TEST_ASSERT_EQUAL(USA, t.countryOf(src, "Austin, Texas, \xC3\x89tats-Unis"));
  TEST_ASSERT_EQUAL(-1, t.countryOf(src, "Atlantis, Ocean"));
  TEST_ASSERT_EQUAL(-1, t.countryOf(src, ""));
}

static void test_aliases_lead_to_their_city() {
  FileSource src;
  PlacesTable t;
  PlaceHit h[PLACES_RESULTS];
  // The French name, shown as typed, the city's region and country
  TEST_ASSERT_EQUAL(1, t.search(src, "Londres", -1, "fr", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL_STRING("Londres", h[0].name);
  TEST_ASSERT_EQUAL_STRING("England", h[0].region);
  TEST_ASSERT_EQUAL_STRING("Royaume-Uni", h[0].countryName);
  TEST_ASSERT_EQUAL(UK, h[0].country);
  // "New York" is an alias of New York City, which "new york" also starts:
  // the city once, by the exact name
  TEST_ASSERT_EQUAL(1, t.search(src, "new york", -1, "en", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL_STRING("New York", h[0].name);
  TEST_ASSERT_TRUE(h[0].exact);
  TEST_ASSERT_EQUAL_STRING("United States", h[0].countryName);
  // London and Londres are one city: once, under its name
  const int n = t.search(src, "lond", -1, "en", h, PLACES_RESULTS);
  TEST_ASSERT_EQUAL(PLACES_RESULTS, n);
  TEST_ASSERT_EQUAL_STRING("London", h[0].name);
  TEST_ASSERT_EQUAL(UK, h[0].country);
  for (int i = 1; i < n; i++) TEST_ASSERT_NOT_EQUAL(h[0].id, h[i].id);
}

static void test_blocks_are_checked() {
  PlaceHit h[PLACES_RESULTS];
  {
    // A byte changed in the block Paris is in: refused, never read. The
    // block: the last whose first key is below "paris", from the head
    FileSource bad;
    const uint8_t *e = bad.bytes.data() + 36;
    uint32_t parisAt = 0;
    for (int i = 0; i < 91; i++) {
      const uint8_t klen = e[40];
      if (std::string((const char *)e + 41, klen) < "paris")
        parisAt = e[0] | e[1] << 8 | e[2] << 16 | (uint32_t)e[3] << 24;
      e += 41 + klen;
    }
    PlacesTable t2;
    bad.corrupt = parisAt + 100;
    TEST_ASSERT_EQUAL(-1, t2.search(bad, "paris", -1, "en", h, PLACES_RESULTS));
    TEST_ASSERT_EQUAL(PLACES_REFUSED, t2.error());
    // The other blocks are still read
    TEST_ASSERT_EQUAL(1, t2.search(bad, "Abidjan", -1, "en", h, 1));
  }
  {
    // A byte changed in the head: the whole table refused
    FileSource bad;
    bad.corrupt = 2000;
    PlacesTable t;
    TEST_ASSERT_EQUAL(-1, t.search(bad, "paris", -1, "en", h, PLACES_RESULTS));
    TEST_ASSERT_EQUAL(PLACES_REFUSED, t.error());
    TEST_ASSERT_FALSE(t.open());
  }
  {
    // A head that is not the one this firmware pins
    FileSource src;
    uint8_t other[32];
    memcpy(other, PLACES_HEAD_SHA256, 32);
    other[31] ^= 0x80;
    PlacesTable t(other);
    TEST_ASSERT_EQUAL(-1, t.search(src, "paris", -1, "en", h, PLACES_RESULTS));
    TEST_ASSERT_EQUAL(PLACES_REFUSED, t.error());
  }
  {
    // No answer: unreachable, and asked again next time
    FileSource src;
    src.fail = true;
    PlacesTable t;
    TEST_ASSERT_EQUAL(-1, t.search(src, "paris", -1, "en", h, PLACES_RESULTS));
    TEST_ASSERT_EQUAL(PLACES_UNREACHABLE, t.error());
    src.fail = false;
    TEST_ASSERT_EQUAL(2, t.search(src, "paris", -1, "en", h, PLACES_RESULTS));
  }
}

// A table made here, every hash consistent with its bytes: what a build bug
// or a hostile name would publish, which only the parser can refuse. One
// name block, then a country and a region block, each a stored (not
// compressed) deflate block; city: a city record's 8 bytes, of country 0.
struct BuiltTable : PlacesSource {
  std::vector<uint8_t> bytes;
  uint8_t headSha[32];
  uint32_t headSize = 0;
  static std::vector<uint8_t> stored(const std::string &raw) {
    const uint16_t n = (uint16_t)raw.size();
    std::vector<uint8_t> b = {0x01, (uint8_t)n, (uint8_t)(n >> 8), (uint8_t)~n, (uint8_t)(~n >> 8)};
    b.insert(b.end(), raw.begin(), raw.end());
    return b;
  }
  static void put(std::vector<uint8_t> &b, uint32_t v, int bytes) {
    for (int i = 0; i < bytes; i++) b.push_back((uint8_t)(v >> (8 * i)));
  }
  static void sha(const std::vector<uint8_t> &b, uint8_t *out) {
    mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), b.data(), b.size(), out);
  }
  BuiltTable(const std::string &names, const std::string &countries, uint16_t cities,
             uint16_t countryCount) {
    const std::vector<uint8_t> blocks[3] = {stored(names), stored(countries), stored(std::string("Region\0", 7))};
    headSize = 36 + 41 + 1 + 2 * 41;
    std::vector<uint8_t> h(MAGIC, MAGIC + 4);
    put(h, 1, 2), put(h, 4096, 2), put(h, 1, 4), put(h, headSize, 4), put(h, 1, 2), put(h, 2, 2);
    put(h, cities, 4), put(h, 0, 4), put(h, countryCount, 2), put(h, 1, 2), put(h, 0, 2), put(h, 0, 2);
    uint8_t digest[32];
    uint32_t at = headSize;
    put(h, at, 4), put(h, blocks[0].size(), 2), put(h, 0, 2);
    sha(blocks[0], digest);
    h.insert(h.end(), digest, digest + 32);
    h.push_back(1), h.push_back('a');
    for (int i = 1; i < 3; i++) {
      at += blocks[i - 1].size();
      h.push_back(i == 1 ? 0 : 1), put(h, 0, 2), put(h, at, 4), put(h, blocks[i].size(), 2);
      sha(blocks[i], digest);
      h.insert(h.end(), digest, digest + 32);
    }
    TEST_ASSERT_EQUAL(headSize, h.size());
    sha(h, headSha);
    bytes = h;
    for (const auto &b : blocks) bytes.insert(bytes.end(), b.begin(), b.end());
  }
  bool read(uint32_t offset, uint32_t len, uint8_t *out) override {
    if (offset + len > bytes.size()) return false;
    memcpy(out, bytes.data() + offset, len);
    return true;
  }
};

static const std::string CITY_OF_0("\x00\x00\x00\x00\x00\x00\x00\x00", 8);
static const std::string ONE_COUNTRY("XX\x00\x00Xland\0Xlande\0", 17);

static std::string record(uint8_t kept, const std::string &rest, const std::string &tail) {
  return std::string(1, (char)kept) + (char)rest.size() + rest + tail;
}

static void test_names_never_overflow() {
  PlaceHit h[PLACES_RESULTS];
  {
    // Made right, the table is read: the builder is not what refuses below
    BuiltTable b(record(0, std::string(127, 'a'), CITY_OF_0), ONE_COUNTRY, 1, 1);
    PlacesTable t(b.headSha, b.headSize, b.bytes.size());
    TEST_ASSERT_EQUAL(1, t.search(b, "aaa", -1, "fr", h, PLACES_RESULTS));
    TEST_ASSERT_EQUAL_STRING("Xlande", h[0].countryName);
    TEST_ASSERT_EQUAL_STRING("Region", h[0].region);
  }
  {
    // 127 bytes kept and 100 more: past the 128-byte name, the block refused
    BuiltTable b(record(0, std::string(127, 'a'), CITY_OF_0) +
                     record(127, std::string(100, 'a'), CITY_OF_0), ONE_COUNTRY, 2, 1);
    PlacesTable t(b.headSha, b.headSize, b.bytes.size());
    TEST_ASSERT_EQUAL(-1, t.search(b, "aaa", -1, "en", h, PLACES_RESULTS));
    TEST_ASSERT_EQUAL(PLACES_REFUSED, t.error());
  }
  {
    // A record cut short at the block's end: refused too, not taken as the end
    BuiltTable b(record(0, "abc", CITY_OF_0) + std::string("\x01\x05" "ab", 4), ONE_COUNTRY, 2, 1);
    PlacesTable t(b.headSha, b.headSize, b.bytes.size());
    TEST_ASSERT_EQUAL(-1, t.search(b, "ab", -1, "en", h, PLACES_RESULTS));
    TEST_ASSERT_EQUAL(PLACES_REFUSED, t.error());
  }
}

static void test_a_country_needs_both_names() {
  // The country block ends right after the English name, 4096 bytes in:
  // the French one would be read past the block. Country 0 fills the rest.
  std::string pad("AA\x00\x00", 4);
  pad += std::string(4096 - 10 - 4 - 3, 'A') + '\0' + "B" + '\0';
  const std::string cut = pad + std::string("XX\x00\x00Xland\0", 10);
  TEST_ASSERT_EQUAL(4096, cut.size());
  const std::string city("\x00\x00\x00\x00\x01\x00\x00\x00", 8);   // of country 1
  PlaceHit h[PLACES_RESULTS];
  BuiltTable b(record(0, "abc", city), cut, 1, 2);
  PlacesTable t(b.headSha, b.headSize, b.bytes.size());
  TEST_ASSERT_EQUAL(-1, t.search(b, "abc", -1, "fr", h, PLACES_RESULTS));
  TEST_ASSERT_EQUAL(PLACES_REFUSED, t.error());
  TEST_ASSERT_EQUAL(-1, t.countryOf(b, "Abc, Nowhere"));
}

static void test_typing_reads_less_each_time() {
  FileSource src;
  PlacesTable t;
  PlaceHit h[PLACES_RESULTS];
  TEST_ASSERT_TRUE(t.search(src, "pari", -1, "en", h, PLACES_RESULTS) > 0);
  const int first = src.reads;
  // The head and the name block held: the table blocks, at most
  TEST_ASSERT_EQUAL(2, t.search(src, "paris", -1, "en", h, PLACES_RESULTS));
  const int second = src.reads - first;
  TEST_ASSERT_TRUE(second < first);
  // Closed, it starts over with the head and the name block
  t.close();
  TEST_ASSERT_FALSE(t.open());
  const int before = src.reads;
  TEST_ASSERT_EQUAL(2, t.search(src, "paris", -1, "en", h, PLACES_RESULTS));
  TEST_ASSERT_TRUE(src.reads - before >= second + 2);
}

static void test_json() {
  PlaceHit h[2];
  h[0].id = 15412;
  strcpy(h[0].name, "Paris");
  strcpy(h[0].region, "\xC3\x8Ele-de-France");
  strcpy(h[0].countryName, "France");
  h[1].id = 7;
  strcpy(h[1].name, "A \"B\" \\ C");
  strcpy(h[1].countryName, "D");
  char out[256];
  TEST_ASSERT_TRUE(placesJson(h, 2, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING(
      "[{\"id\":15412,\"n\":\"Paris\",\"a\":\"\xC3\x8Ele-de-France\",\"c\":\"France\"},"
      "{\"id\":7,\"n\":\"A \\\"B\\\" \\\\ C\",\"a\":\"\",\"c\":\"D\"}]",
      out);
  TEST_ASSERT_TRUE(placesJson(h, 0, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("[]", out);
  // Too small: refused, never a cut answer
  char small[40];
  TEST_ASSERT_FALSE(placesJson(h, 2, small, sizeof(small)));
  // Five of the longest names fit the route's buffer
  PlaceHit big[PLACES_RESULTS];
  for (PlaceHit &b : big) {
    b.id = 65535;
    memset(b.name, '"', sizeof(b.name) - 1);
    memset(b.region, '\\', sizeof(b.region) - 1);
    memset(b.countryName, 'x', sizeof(b.countryName) - 1);
  }
  char route[PLACES_JSON_SIZE];
  TEST_ASSERT_TRUE(placesJson(big, PLACES_RESULTS, route, sizeof(route)));
}

// GET /place: the place found, nothing yet, or not found
static void test_place_json() {
  char out[PLACE_JSON_SIZE];
  TEST_ASSERT_TRUE(placeJson("Innsbruck, Tyrol, Austria", false, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("{\"p\":\"Innsbruck, Tyrol, Austria\"}", out);
  TEST_ASSERT_TRUE(placeJson("", false, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("{\"p\":\"\"}", out);
  TEST_ASSERT_TRUE(placeJson("", true, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("{\"p\":\"\",\"f\":1}", out);
  // A place found wins over an old failure; quotes are escaped
  TEST_ASSERT_TRUE(placeJson("A \"B\" \\ C", true, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("{\"p\":\"A \\\"B\\\" \\\\ C\"}", out);
  // Too small: refused; the longest place (63 bytes, every one escaped) fits
  char small[8];
  TEST_ASSERT_FALSE(placeJson("Innsbruck", false, small, sizeof(small)));
  char longest[64];
  memset(longest, '"', sizeof(longest) - 1);
  longest[sizeof(longest) - 1] = '\0';
  TEST_ASSERT_TRUE(placeJson(longest, true, out, sizeof(out)));
}

static void test_a_chosen_or_typed_city() {
  FileSource src;
  PlacesTable t;
  PlaceHit h[PLACES_RESULTS];
  TEST_ASSERT_EQUAL(2, t.search(src, "paris", -1, "en", h, PLACES_RESULTS));
  const uint16_t paris = h[0].id, texas = h[1].id;
  // cid: the suggestion chosen
  PlaceHit c;
  char zone[48];
  TEST_ASSERT_TRUE(t.city(src, texas, "en", c, zone, sizeof(zone)));
  TEST_ASSERT_EQUAL_STRING("Paris", c.name);
  TEST_ASSERT_EQUAL_STRING("Texas", c.region);
  TEST_ASSERT_EQUAL_STRING("CST6CDT,M3.2.0,M11.1.0", zone);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 33.66f, c.lat);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -95.56f, c.lon);
  TEST_ASSERT_TRUE(t.city(src, paris, "fr", c, zone, sizeof(zone)));
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", zone);
  char label[64];
  placeLabel(c, label, sizeof(label));
  TEST_ASSERT_EQUAL_STRING("Paris, \xC3\x8Ele-de-France, France", label);
  // A number the table does not have: not found, not an error
  TEST_ASSERT_FALSE(t.city(src, 65535, "en", c, zone, sizeof(zone)));
  TEST_ASSERT_EQUAL(PLACES_OK, t.error());
  // The last city of all
  TEST_ASSERT_TRUE(t.city(src, 22939, "en", c, zone, sizeof(zone)));
  // city: the best match, accents or not
  TEST_ASSERT_EQUAL(1, t.search(src, "saint etienne", -1, "en", h, 1));
  TEST_ASSERT_TRUE(t.zoneName(src, h[0].zone, zone, sizeof(zone)));
  TEST_ASSERT_EQUAL_STRING("Saint-\xC3\x89tienne", h[0].name);
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", zone);
  // A region with no name: "City, Country"
  PlaceHit bare;
  strcpy(bare.name, "Monaco");
  strcpy(bare.countryName, "Monaco");
  placeLabel(bare, label, sizeof(label));
  TEST_ASSERT_EQUAL_STRING("Monaco, Monaco", label);
  // Cut on a character, never inside one
  strcpy(bare.name, "Llanfairpwllgwyngyll");
  strcpy(bare.region, "Isle of Anglesey and Wales");
  strcpy(bare.countryName, "Cymru \xC3\xA9");
  char cut[57 + 1];   // room for the é's first byte only
  placeLabel(bare, cut, sizeof(cut));
  TEST_ASSERT_EQUAL_STRING("Llanfairpwllgwyngyll, Isle of Anglesey and Wales, Cymru ", cut);
}

static void test_what_a_found_city_changes() {
  PlaceHit h;
  strcpy(h.name, "Paris");
  strcpy(h.region, "\xC3\x8Ele-de-France");
  strcpy(h.countryName, "France");
  h.lat = 48.85f;
  h.lon = 2.35f;
  char place[64] = "", zone[48] = "";
  float lat = 0, lon = 0;
  const Location loc = {place, sizeof(place), &lat, &lon, zone, sizeof(zone)};
  // Typed or chosen: everything
  TEST_ASSERT_TRUE(takeLocation(h, "CET-1CEST,M3.5.0,M10.5.0/3", loc));
  TEST_ASSERT_EQUAL_STRING("Paris, \xC3\x8Ele-de-France, France", place);
  TEST_ASSERT_EQUAL_FLOAT(48.85f, lat);
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", zone);
  // An earlier firmware's place, close by: its zone alone
  strcpy(place, "Paris, Ile-de-France, France");
  lat = 48.8534f;
  lon = 2.3488f;
  zone[0] = '\0';
  TEST_ASSERT_TRUE(takeLocation(h, "CET-1CEST,M3.5.0,M10.5.0/3", loc));
  TEST_ASSERT_EQUAL_STRING("Paris, Ile-de-France, France", place);
  TEST_ASSERT_EQUAL_FLOAT(48.8534f, lat);
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", zone);
  // The same name elsewhere: nothing
  strcpy(place, "Paris, Texas, United States");
  lat = 33.66f;
  lon = -95.56f;
  zone[0] = '\0';
  TEST_ASSERT_FALSE(takeLocation(h, "CET-1CEST,M3.5.0,M10.5.0/3", loc));
  TEST_ASSERT_EQUAL_STRING("", zone);
  TEST_ASSERT_EQUAL_FLOAT(33.66f, lat);
}

static void test_what_is_looked_up() {
  // A city typed, not looked up yet
  TEST_ASSERT_TRUE(locateWanted("Lyon", "", "", false, false));
  TEST_ASSERT_TRUE(locateWanted("Lyon", "", "CET-1CEST,M3.5.0,M10.5.0/3", false, false));
  // A place an earlier firmware found, without a zone
  TEST_ASSERT_TRUE(locateWanted("Paris", "Paris, \xC3\x8Ele-de-France, France", "", false, false));
  // Found, zone and all; never found; nothing typed
  TEST_ASSERT_FALSE(locateWanted("Paris", "Paris, France", "CET-1CEST,M3.5.0,M10.5.0/3", false, false));
  TEST_ASSERT_FALSE(locateWanted("Atlantis", "", "", true, false));
  TEST_ASSERT_FALSE(locateWanted("", "", "", false, false));
  // A suggestion chosen: looked up even with no city stored (a panel set up
  // with "UTC", a city cleared), or one the table once had no answer for
  TEST_ASSERT_TRUE(locateWanted("", "", "", false, true));
  TEST_ASSERT_TRUE(locateWanted("Atlantis", "", "", true, true));
}

static void test_table_kept_while_typing_never_into_a_forecast() {
  // Kept between two keystrokes, for PLACES_IDLE_MS
  TEST_ASSERT_TRUE(placesKeepTable(0, false));
  TEST_ASSERT_TRUE(placesKeepTable(PLACES_IDLE_MS - 1, false));
  TEST_ASSERT_FALSE(placesKeepTable(PLACES_IDLE_MS, false));
  // Freed before the HTTPS task's HTTPS session, used a moment ago or not
  TEST_ASSERT_FALSE(placesKeepTable(0, true));
  TEST_ASSERT_FALSE(placesKeepTable(PLACES_IDLE_MS - 1, true));
}

static void test_phone_zone_at_setup() {
  char city[48], zone[48];
  TEST_ASSERT_TRUE(phoneZone("Europe/Paris", city, sizeof(city), zone, sizeof(zone)));
  TEST_ASSERT_EQUAL_STRING("Paris", city);
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", zone);
  TEST_ASSERT_TRUE(phoneZone("America/Argentina/Buenos_Aires", city, sizeof(city), zone, sizeof(zone)));
  TEST_ASSERT_EQUAL_STRING("Buenos Aires", city);
  TEST_ASSERT_EQUAL_STRING("<-03>3", zone);
  // A link, as phones may still report
  TEST_ASSERT_TRUE(phoneZone("US/Eastern", city, sizeof(city), zone, sizeof(zone)));
  TEST_ASSERT_EQUAL_STRING("EST5EDT,M3.2.0,M11.1.0", zone);
  // A zone with no place in its name: the zone alone
  TEST_ASSERT_TRUE(phoneZone("UTC", city, sizeof(city), zone, sizeof(zone)));
  TEST_ASSERT_EQUAL_STRING("", city);
  TEST_ASSERT_EQUAL_STRING("UTC0", zone);
  TEST_ASSERT_TRUE(phoneZone("Etc/GMT+2", city, sizeof(city), zone, sizeof(zone)));
  TEST_ASSERT_EQUAL_STRING("", city);
  // Unknown to tzdata, or nothing: nothing
  strcpy(zone, "kept");
  TEST_ASSERT_FALSE(phoneZone("Mars/Olympus_Mons", city, sizeof(city), zone, sizeof(zone)));
  TEST_ASSERT_FALSE(phoneZone("", city, sizeof(city), zone, sizeof(zone)));
  TEST_ASSERT_EQUAL_STRING("kept", zone);
  // The city found from it: the phone's zone city is in the table
  FileSource src;
  PlacesTable t;
  PlaceHit h;
  phoneZone("America/Argentina/Buenos_Aires", city, sizeof(city), zone, sizeof(zone));
  TEST_ASSERT_EQUAL(1, t.search(src, city, -1, "en", &h, 1));
  TEST_ASSERT_EQUAL_STRING("Argentina", h.countryName);
  phoneZone("Asia/Ho_Chi_Minh", city, sizeof(city), zone, sizeof(zone));
  TEST_ASSERT_EQUAL(1, t.search(src, city, -1, "en", &h, 1));
  TEST_ASSERT_EQUAL_STRING("Ho Chi Minh City", h.name);
}

static void test_retry_delay() {
  // An hour, doubling to a day, plus the panel's own seconds
  TEST_ASSERT_EQUAL_UINT32(3600, locateRetryDelay(1, 0));
  TEST_ASSERT_EQUAL_UINT32(3600 + 1234, locateRetryDelay(1, 1234));
  TEST_ASSERT_EQUAL_UINT32(7200, locateRetryDelay(2, 0));
  TEST_ASSERT_EQUAL_UINT32(16 * 3600, locateRetryDelay(5, 0));
  TEST_ASSERT_EQUAL_UINT32(24 * 3600, locateRetryDelay(6, 0));
  TEST_ASSERT_EQUAL_UINT32(24 * 3600 + 3599, locateRetryDelay(255, 3599));
  TEST_ASSERT_EQUAL_UINT32(3600 + 1, locateRetryDelay(1, 3601));
}

// The local hour and minute at t in the zone tz, as "HH:MM"
static std::string localAt(const char *tz, time_t t) {
  setenv("TZ", tz, 1);
  tzset();
  struct tm tm;
  localtime_r(&t, &tm);
  char s[8];
  snprintf(s, sizeof(s), "%02d:%02d", tm.tm_hour, tm.tm_min);
  return s;
}

static void test_posix_zone() {
  char tz[16];
  posixZone(tz, sizeof(tz), 7200);
  TEST_ASSERT_EQUAL_STRING("UTC-2:00", tz);
  posixZone(tz, sizeof(tz), 19800);
  TEST_ASSERT_EQUAL_STRING("UTC-5:30", tz);
  posixZone(tz, sizeof(tz), -12600);
  TEST_ASSERT_EQUAL_STRING("UTC+3:30", tz);
  posixZone(tz, sizeof(tz), 0);
  TEST_ASSERT_EQUAL_STRING("UTC-0:00", tz);
  // The C library reads it the way it is meant: 12:00 UTC is 14:00 at UTC+2
  TEST_ASSERT_EQUAL_STRING("14:00", localAt("UTC-2:00", 1782561600).c_str());
  TEST_ASSERT_EQUAL_STRING("17:30", localAt("UTC-5:30", 1782561600).c_str());
}

int main() {
  UNITY_BEGIN();
  {
    FileSource src;
    PlacesTable t;
    FRANCE = t.countryOf(src, "Paris, France");
    SPAIN = t.countryOf(src, "Madrid, Spain");
    UK = t.countryOf(src, "London, United Kingdom");
    USA = t.countryOf(src, "Austin, United States");
  }
  RUN_TEST(test_fold);
  RUN_TEST(test_url);
  RUN_TEST(test_query_limits);
  RUN_TEST(test_search_ranks_exact_then_population);
  RUN_TEST(test_home_country_counts_ten_times);
  RUN_TEST(test_aliases_lead_to_their_city);
  RUN_TEST(test_blocks_are_checked);
  RUN_TEST(test_names_never_overflow);
  RUN_TEST(test_a_country_needs_both_names);
  RUN_TEST(test_typing_reads_less_each_time);
  RUN_TEST(test_json);
  RUN_TEST(test_place_json);
  RUN_TEST(test_a_chosen_or_typed_city);
  RUN_TEST(test_what_a_found_city_changes);
  RUN_TEST(test_what_is_looked_up);
  RUN_TEST(test_table_kept_while_typing_never_into_a_forecast);
  RUN_TEST(test_phone_zone_at_setup);
  RUN_TEST(test_retry_delay);
  RUN_TEST(test_posix_zone);
  return UNITY_END();
}
