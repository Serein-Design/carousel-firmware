# data/places.bin, format 1

The city table the panel searches when a city is typed, with no geocoding service. It holds
about 23 000 places from GeoNames, sorted by a folded key, in 4 KB blocks the panel fetches one
at a time with HTTPS range requests from the public repository. A short head (header and
block index) lists every block with its SHA-256; the firmware pins the head's own SHA-256
(`firmware/src/places_index.cpp`), so a byte changed anywhere in the file is caught.
`tools/places/build.py` writes the file; `tools/places/test_build.py` reads it back from this
page.

## Layout

All integers are little-endian. Offsets count from the start of the file.

| Part | Bytes | Integrity |
| --- | --- | --- |
| Header | 36 | the head's SHA-256, `PLACES_HEAD_SHA256` |
| Name index | 41 + key length, per name block | the head's SHA-256 |
| Table index | 41 per table block | the head's SHA-256 |
| Name blocks, in key order | as listed | each block's SHA-256, in the index |
| Table blocks: countries, then regions, then zones | as listed | each block's SHA-256, in the index |

The head is the header and both indexes: `PLACES_HEAD_SIZE` bytes, the offset of the first
block. Blocks follow each other with no gap, and the last one ends the file
(`PLACES_FILE_SIZE`).

## Header

| Offset | Type | Field |
| --- | --- | --- |
| 0 | 4 bytes | magic `PLCS` |
| 4 | u16 | format, 1 |
| 6 | u16 | block size, 4096: no block inflates to more |
| 8 | u32 | data version, `PLACES_VERSION`, raised at every rebuild |
| 12 | u32 | head size |
| 16 | u16 | name blocks |
| 18 | u16 | table blocks |
| 20 | u32 | cities |
| 24 | u32 | aliases |
| 28 | u16 | countries |
| 30 | u16 | regions (first-level administrative divisions) |
| 32 | u16 | zones (POSIX TZ strings) |
| 34 | u16 | 0, reserved |

## Index

One entry per name block, in file order:

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u32 | block offset |
| 4 | u16 | stored (deflated) length |
| 6 | u16 | number of the block's first city: the cities in all blocks before it |
| 8 | 32 bytes | SHA-256 of the stored bytes |
| 40 | u8 | key length |
| 41 | bytes | key of the block's first record |

Then one entry per table block:

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u8 | table: 0 countries, 1 regions, 2 zones |
| 1 | u16 | number of the block's first entry in its table |
| 3 | u32 | block offset |
| 7 | u16 | stored length |
| 9 | 32 bytes | SHA-256 of the stored bytes |

## Blocks

Each block is raw deflate (RFC 1951, no zlib header), compressed on its own, and inflates to
4096 bytes at most; a record or an entry never straddles two blocks. The hash covers the
stored bytes, so a block is checked before it is inflated.

### Name blocks

Records follow each other, sorted by key, then by population (largest first), a city before
an alias, then GeoNames id, then name bytes:

| Bytes | Field |
| --- | --- |
| 1 | how many bytes the name shares with the record before it in the block (0 for the first) |
| 1 | bit 7: alias; bits 0-6: how many name bytes follow |
| n | the rest of the name, UTF-8 |
| 8 | a city: i16 latitude and i16 longitude in hundredths of a degree, u8 country, u8 region within its country, u8 zone, u8 population class |
| 2 | an alias: u16 number of its city |

A name is 127 bytes at most: the panel rebuilds it in 128, its NUL included, and refuses a
block with a longer one or a record cut short. Cities are numbered in this order from 0, aliases left out. The key is not stored: it is the
fold of the name. A city's region number is the country's first region (country table) plus
the u8. The population class is round(25 log10(population)), 0 when unknown: the population is
about 10^(class / 25), steps of about 10 %.

To find a key, start at the last block whose first key is lower than it (the block before
may end with that key, so a first key equal to it is not enough), and read on while the
first key is not above it. An alias leads to its city through the index's first-city
numbers.

### Table blocks

| Table | Entry | Order |
| --- | --- | --- |
| Countries | 2-byte ISO 3166 code, u16 first region number, English name, NUL, French name, NUL | by code, every country of `countryInfo` |
| Regions | name, NUL | by country, then GeoNames code; only regions some city is in |
| Zones | POSIX TZ string, NUL | byte order, each once |

An entry's number is the block's first entry number plus its rank in the block. A country
without a French name in GeoNames repeats the English one; a region with no name is an empty
string.

## The key

The fold of a name, which the panel applies to what is typed:

1. Unicode NFKD.
2. Drop combining marks (`é` becomes `e`).
3. Replace the letters NFKD does not split: `ß` ss, `æ` ae, `œ` oe, `ø` o, `ł` l, `đ` `ð` d,
   `þ` th, `ı` i, `ħ` h, `ŧ` t, `ŀ` l, `ĸ` k, `ŉ` n, `ƒ` f, capitals likewise (`SPECIAL` in
   build.py).
4. Lower-case, then keep `a`-`z` and `0`-`9` only.

"Saint-Étienne" and "saint etienne" both fold to `saintetienne`, "Łódź" to `lodz`.

## What is in it

From the GeoNames dumps `cities500`, `countryInfo`, `admin1CodesASCII` and `alternateNamesV2`:

- Every populated place of 15 000 people or more, plus the three largest of each country and
  its capital (feature code `PPLC`, else the capital `countryInfo` names), whatever their size.
  Sections, historical, abandoned and destroyed places (`PPLX`, `PPLH`, `PPLQ`, `PPLW`,
  `PPLCH`) are left out.
- Of two places within 10 km of each other, only the larger stays (the capitals and top three
  always stay); whoever types the smaller one's name gets nothing, but the weather 10 km away
  is the same.
- The name GeoNames gives, or its ASCII form when the name is not in Latin script.
- Aliases: the alternate names whose key differs from the city's, in French, in English and
  in the first language `countryInfo` lists for the city's country ("München" for Munich,
  "Firenze" for Florence, "Wien" for Vienna), neither colloquial nor historic, in Latin script,
  without `()/,` or digits ("New York" for New York City, "St Étienne"). A country's other
  languages are not aliases: "Florenz" finds nothing in Italy.
- Zones: the POSIX TZ string at the end of each city's zone file in the tzdata the build
  reads, a negative rule time made 0, which the panel's C library reads right (`newlib_posix`
  in build.py: Nuuk's change to summer time comes an hour late).

## Determinism

The same dumps, tzdata and `--version` give the same file, byte for byte: every order is
explicit, and nothing reads the clock. The deflated bytes also depend on the zlib build
(level 9, window 15, memory level 9): on another zlib the blocks may differ, and so the
hashes, which is why `places.bin` and `places_index.cpp` are always written and committed
together.
