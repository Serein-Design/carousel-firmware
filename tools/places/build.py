#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Builds data/places.bin, the city table the panel searches, and the firmware files that trust it.

    python3 -I tools/places/build.py --version N [--data DIR] [--zoneinfo DIR] [--out ROOT]

Reads the GeoNames dumps (geonames.org, CC BY 4.0) from --data, downloading the missing ones
into it (without --data, into a temporary folder), and the time zones from a compiled tzdata
tree (--zoneinfo, default /usr/share/zoneinfo). Writes, under --out (default the repository):

    data/places.bin                     the table, format in tools/places/FORMAT.md
    firmware/src/places_index.{h,cpp}   its version, size and the SHA-256 of its head
    firmware/src/tz_names.{h,cpp}       every IANA zone name -> its POSIX TZ string

Same input, same bytes: nothing depends on the clock, the file order of a folder or a set's
order. The deflated blocks depend on the zlib build too (FORMAT.md, Determinism).
Python 3 standard library only.
"""
import argparse
import hashlib
import io
import math
import os
import struct
import sys
import tempfile
import unicodedata
import urllib.request
import zipfile
import zlib
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GEONAMES = "https://download.geonames.org/export/dump/"
# cities500, not cities15000: the top three of a country with few large towns are small ones
DUMPS = ("cities500.zip", "countryInfo.txt", "admin1CodesASCII.txt", "alternateNamesV2.zip")
OUTPUTS = ("data/places.bin", "firmware/src/places_index.h", "firmware/src/places_index.cpp",
           "firmware/src/tz_names.h", "firmware/src/tz_names.cpp")

MAGIC, FORMAT = b"PLCS", 1
NAME_MAX = 127   # bytes of UTF-8 in a name: also below the 0x7F a suffix length holds
BLOCK = 4096            # inflated size cap of a block: the panel inflates one into a 4 KB buffer
MIN_POP = 15000
MERGE_KM = 10           # a smaller city closer than this to a kept one is left out
TOP = 3                 # largest cities kept in every country, whatever their size
ALIAS_LANGS = ("fr", "en")   # the panel's languages; a city's country's first language comes on top
# Sections of a city, historical, abandoned, destroyed: never a place of their own
SKIP_CODES = {"PPLX", "PPLH", "PPLQ", "PPLW", "PPLCH"}
TABLE_COUNTRY, TABLE_ADMIN1, TABLE_ZONE = 0, 1, 2

HEADER = struct.Struct("<4sHHIIHHIIHHHH")
NAME_ENTRY = struct.Struct("<IHH32sB")
TABLE_ENTRY = struct.Struct("<BHIH32s")
CITY = struct.Struct("<hhBBBB")

# ---------- the key: what the panel computes from what is typed ----------
# Letters NFKD does not split into a base letter and a mark
SPECIAL = {"ß": "ss", "æ": "ae", "Æ": "ae", "œ": "oe", "Œ": "oe", "ø": "o", "Ø": "o", "ł": "l", "Ł": "l",
           "đ": "d", "Đ": "d", "ð": "d", "Ð": "d", "þ": "th", "Þ": "th", "ı": "i", "ħ": "h", "Ħ": "h",
           "ŧ": "t", "Ŧ": "t", "ŀ": "l", "Ŀ": "l", "ĸ": "k", "ŉ": "n", "ƒ": "f"}


def fold(text):
    """'Saint-Étienne' -> 'saintetienne': NFKD, marks dropped, SPECIAL, lower case, a-z0-9 kept.

    NFKD mappings and combining classes of assigned characters never change (Unicode stability
    policy), so the key does not depend on the Python version.
    """
    out = []
    for ch in unicodedata.normalize("NFKD", text):
        if unicodedata.combining(ch):
            continue
        for c in SPECIAL.get(ch, ch).lower():
            if "a" <= c <= "z" or "0" <= c <= "9":
                out.append(c)
    return "".join(out)


def latin_only(text):
    """True for names the panel's Latin font can draw, apostrophes included."""
    return all(ord(ch) < 0x250 or 0x1E00 <= ord(ch) <= 0x1EFF or ch in "’‘ʻʼʿʾ" for ch in text)


# ---------- input ----------
def fetch(folder):
    for name in DUMPS:
        stem = name.rsplit(".", 1)[0]
        if any(os.path.exists(os.path.join(folder, stem + ext)) for ext in (".txt", ".zip")):
            continue
        path = os.path.join(folder, name)
        print("downloading", GEONAMES + name, file=sys.stderr)
        with urllib.request.urlopen(GEONAMES + name, timeout=120) as r, open(path + ".part", "wb") as f:
            while chunk := r.read(1 << 20):
                f.write(chunk)
        os.replace(path + ".part", path)


def open_dump(folder, stem):
    """The .txt if it is there, else the same file read straight from the .zip."""
    txt = os.path.join(folder, stem + ".txt")
    if os.path.exists(txt):
        return open(txt, encoding="utf-8")
    archive = zipfile.ZipFile(os.path.join(folder, stem + ".zip"))
    return io.TextIOWrapper(archive.open(stem + ".txt"), encoding="utf-8")


def load_cities(folder):
    rows = []
    with open_dump(folder, "cities500") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            if c[7] in SKIP_CODES:
                continue
            rows.append(dict(id=int(c[0]), name=c[1], ascii=c[2], lat=float(c[4]), lon=float(c[5]),
                             code=c[7], cc=c[8], a1=c[10], pop=int(c[14] or 0), tz=c[17]))
    return rows


def load_countries(folder):
    countries = {}
    with open_dump(folder, "countryInfo") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            c = line.rstrip("\n").split("\t")
            countries[c[0]] = dict(name=c[4], capital=c[5], gid=int(c[16]) if c[16] else 0,
                                   lang=c[15].split(",")[0].split("-")[0].lower())
    return countries


def load_admin1(folder):
    admin1 = {}
    with open_dump(folder, "admin1CodesASCII") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            admin1[c[0]] = c[1] if latin_only(c[1]) else c[2]
    return admin1


def load_alternates(folder, wanted, languages):
    """geonameid -> its names in these languages, in alternateNameId order."""
    alts = defaultdict(list)
    with open_dump(folder, "alternateNamesV2") as f:
        for line in f:
            c = line.split("\t", 8)   # 750 MB: split only as far as needed
            if len(c) < 8 or c[2] not in languages or int(c[1]) not in wanted:
                continue
            alts[int(c[1])].append(dict(aid=int(c[0]), lang=c[2], name=c[3], preferred=c[4] == "1",
                                        short=c[5] == "1", colloquial=c[6] == "1", historic=c[7].rstrip("\n") == "1"))
    for v in alts.values():
        v.sort(key=lambda a: a["aid"])
    return alts


def newlib_posix(posix):
    """posix as the panel's C library reads it right. newlib 4.1 (ESP-IDF 4.4) reads a rule's
    time with sscanf "%hu": "M3.5.0/-1" (Nuuk, DST at 01:00 UTC) becomes 65535 hours, and the
    whole winter is taken for summer. Such a time becomes 0, the closest newlib reads: the change
    comes that much later, an hour for Nuuk, once a year. Hours past 24 are read right."""
    out = []
    for i, part in enumerate(posix.split(",")):
        rule, slash, time = part.partition("/")
        out.append(rule + slash + ("0" if i and time.startswith("-") else time))
    return ",".join(out)


def read_zones(root):
    """IANA name (links included) -> POSIX TZ, the last line of each TZif v2+ file; tzdata version."""
    skip = {"posixrules", "Factory", "localtime", "+VERSION", "leapseconds", "tzdata.zi", "leap-seconds.list",
            "SECURITY"}
    zones = {}
    for dirpath, dirnames, files in os.walk(root, followlinks=True):
        dirnames[:] = sorted(d for d in dirnames if d not in ("posix", "right"))
        rel = os.path.relpath(dirpath, root)
        for name in sorted(files):
            if name in skip or name.endswith((".tab", ".zi")):
                continue
            with open(os.path.join(dirpath, name), "rb") as fh:
                data = fh.read()
            if not data.startswith(b"TZif") or data[4:5] not in (b"2", b"3", b"4"):
                continue
            posix = data.rstrip(b"\n").rsplit(b"\n", 1)[-1].decode("ascii")
            zone = name if rel == "." else rel + "/" + name
            if not posix or not all(c.isalnum() or c in "<>+-,./:" for c in posix):
                raise SystemExit("%s: no usable POSIX TZ at the end of %r" % (zone, posix))
            zones[zone] = newlib_posix(posix)
    version = "unknown"
    if os.path.exists(os.path.join(root, "+VERSION")):
        with open(os.path.join(root, "+VERSION")) as f:
            version = f.read().strip()
    elif os.path.exists(os.path.join(root, "tzdata.zi")):
        with open(os.path.join(root, "tzdata.zi")) as f:
            version = f.readline().split()[-1]
    return dict(sorted(zones.items())), version


# ---------- selection ----------
def km(a, b):
    p1, p2 = math.radians(a["lat"]), math.radians(b["lat"])
    x = (math.sin((p2 - p1) / 2) ** 2 +
         math.cos(p1) * math.cos(p2) * math.sin(math.radians(b["lon"] - a["lon"]) / 2) ** 2)
    return 12742 * math.asin(min(1, math.sqrt(x)))


def must_keep(pool, countries):
    """The TOP largest cities of every country and its capital, whatever their size."""
    by_cc = defaultdict(list)
    for r in pool:
        by_cc[r["cc"]].append(r)
    must = set()
    for cc, rows in sorted(by_cc.items()):
        rows.sort(key=lambda r: (-r["pop"], r["id"]))
        must.update(r["id"] for r in rows[:TOP])
        capitals = [r for r in rows if r["code"] == "PPLC"]
        if not capitals and cc in countries:   # a capital GeoNames does not mark PPLC
            capitals = [r for r in rows if countries[cc]["capital"] in (r["name"], r["ascii"])][:1]
        must.update(r["id"] for r in capitals)
    return must


def select(pool, countries):
    """Cities of MIN_POP or more and the must-keep ones; of two within MERGE_KM, the larger."""
    must = must_keep(pool, countries)
    cand = sorted((r for r in pool if r["pop"] >= MIN_POP or r["id"] in must),
                  key=lambda r: (r["id"] not in must, -r["pop"], r["id"]))
    cell = 0.5   # degrees: MERGE_KM is well under one cell of latitude
    grid, kept = defaultdict(list), []
    for r in cand:
        gy, gx = math.floor(r["lat"] / cell), math.floor(r["lon"] / cell)
        coslat = math.cos(math.radians(min(89.9, abs(r["lat"]) + cell)))
        nx = min(360, math.ceil(MERGE_KM / (111.32 * coslat) / cell))
        near = r["id"] not in must and any(
            km(r, k) < MERGE_KM
            for dy in (-1, 0, 1) for dx in range(-nx, nx + 1)
            for k in grid.get((gy + dy, (gx + dx) % 720), ()))
        if not near:
            kept.append(r)
            grid[(gy, gx % 720)].append(r)
    return kept


# ---------- encoding ----------
def deflate(raw):
    c = zlib.compressobj(9, zlib.DEFLATED, -15, 9, zlib.Z_DEFAULT_STRATEGY)
    return c.compress(raw) + c.flush()


def pop_class(pop):
    """About 10 % steps: the population is roughly 10 ** (class / 25); 0 unknown."""
    return 0 if pop <= 0 else min(255, round(math.log10(pop) * 25))


def shared(a, b):
    n, m = 0, min(len(a), len(b), 255)
    while n < m and a[n] == b[n]:
        n += 1
    return n


def cut(entries):
    """Entries (bytes) -> blocks of at most BLOCK bytes, an entry never split; first entry numbers."""
    blocks, firsts, cur = [], [], bytearray()
    for i, e in enumerate(entries):
        if len(e) > BLOCK:
            raise SystemExit("an entry of %d bytes does not fit a block" % len(e))
        if cur and len(cur) + len(e) > BLOCK:
            blocks.append(bytes(cur))
            cur = bytearray()
        if not cur:
            firsts.append(i)
        cur += e
    if cur:
        blocks.append(bytes(cur))
    return blocks, firsts


def table(data_dir, zones):
    countries = load_countries(data_dir)
    admin1 = load_admin1(data_dir)
    pool = load_cities(data_dir)
    cities = select(pool, countries)

    records, skipped = [], []
    for r in cities:
        name = r["name"] if latin_only(r["name"]) and fold(r["name"]) else r["ascii"]
        key = fold(name)
        if not key:
            skipped.append(r["name"])
            continue
        if r["cc"] not in countries:
            raise SystemExit("%s: country %r missing from countryInfo" % (r["name"], r["cc"]))
        if r["tz"] not in zones:
            raise SystemExit("%s: zone %r missing from tzdata" % (r["name"], r["tz"]))
        records.append(dict(r, display=name.encode("utf-8"), key=key))
    if skipped:
        print("left out, no Latin name:", ", ".join(sorted(skipped)), file=sys.stderr)

    ccs = sorted(countries)
    cc_index = {cc: i for i, cc in enumerate(ccs)}
    used_a1 = sorted({(cc_index[r["cc"]], r["a1"]) for r in records})
    a1_first, a1_local = {}, {}
    for n, (ci, a1) in enumerate(used_a1):
        a1_first.setdefault(ci, n)
        a1_local[(ci, a1)] = n - a1_first[ci]
        if a1_local[(ci, a1)] > 255:
            raise SystemExit("%s: more than 256 regions" % ccs[ci])
    posix = sorted({zones[r["tz"]] for r in records})
    zone_index = {p: i for i, p in enumerate(posix)}

    wanted = {r["id"] for r in records} | {countries[cc]["gid"] for cc in ccs}
    native = {cc: countries[cc]["lang"] for cc in ccs if countries[cc]["lang"]}
    alts = load_alternates(data_dir, wanted, set(ALIAS_LANGS) | set(native.values()))

    def fr_name(cc):
        names = sorted((a for a in alts.get(countries[cc]["gid"], ())
                        if a["lang"] == "fr" and not a["colloquial"] and not a["historic"]),
                       key=lambda a: (not a["preferred"], a["short"], a["aid"]))
        return names[0]["name"] if names else countries[cc]["name"]

    country_entries = []
    for i, cc in enumerate(ccs):
        first = a1_first.get(i, sum(1 for ci, _ in used_a1 if ci < i))
        country_entries.append(cc.encode("ascii") + struct.pack("<H", first) +
                               countries[cc]["name"].encode() + b"\0" + fr_name(cc).encode() + b"\0")
    admin1_entries = [admin1.get("%s.%s" % (ccs[ci], a1), "").encode() + b"\0" for ci, a1 in used_a1]
    zone_entries = [p.encode("ascii") + b"\0" for p in posix]

    items = []
    for rec in records:
        items.append(dict(rec=rec, alias=False, key=rec["key"], display=rec["display"]))
        seen = {rec["key"]}
        for a in alts.get(rec["id"], ()):
            if a["lang"] not in ALIAS_LANGS and a["lang"] != native.get(rec["cc"]):
                continue
            if a["colloquial"] or a["historic"] or not latin_only(a["name"]) or any(ch in a["name"] for ch in "()/,0123456789"):
                continue
            k = fold(a["name"])
            if k and k not in seen:
                seen.add(k)
                items.append(dict(rec=rec, alias=True, key=k, display=a["name"].encode("utf-8")))
    items.sort(key=lambda x: (x["key"], -x["rec"]["pop"], x["alias"], x["rec"]["id"], x["display"]))
    ordinal = 0
    for x in items:
        if not x["alias"]:
            x["rec"]["ordinal"] = ordinal
            ordinal += 1
    if ordinal > 0xFFFF:
        raise SystemExit("%d cities: more than a u16 numbers" % ordinal)

    def fields(rec):
        lat, lon = round(rec["lat"] * 100), round(rec["lon"] * 100)
        ci = cc_index[rec["cc"]]
        return CITY.pack(lat, lon, ci, a1_local[(ci, rec["a1"])], zone_index[zones[rec["tz"]]], pop_class(rec["pop"]))

    def encode(x, prev):
        # The firmware rebuilds each name in 128 bytes, its NUL included
        # (Records::name in places_logic.cpp), and refuses a block that would
        # not fit them: a longer name would make its block unreadable
        if len(x["display"]) > NAME_MAX:
            raise SystemExit("%r: name too long" % x["display"])
        p = shared(prev, x["display"])
        suffix = x["display"][p:]
        tail = struct.pack("<H", x["rec"]["ordinal"]) if x["alias"] else fields(x["rec"])
        return bytes([p, len(suffix) | (0x80 if x["alias"] else 0)]) + suffix + tail

    name_blocks, cur, prev, first_city, city = [], bytearray(), b"", 0, 0
    for x in items:
        e = encode(x, prev)
        if cur and len(cur) + len(e) > BLOCK:
            name_blocks.append((first_key, first_city, bytes(cur)))
            cur, prev = bytearray(), b""
            e = encode(x, prev)
        if not cur:
            first_key, first_city = x["key"], city
        cur += e
        prev = x["display"]
        city += not x["alias"]
    name_blocks.append((first_key, first_city, bytes(cur)))

    tables = []
    for kind, entries in ((TABLE_COUNTRY, country_entries), (TABLE_ADMIN1, admin1_entries), (TABLE_ZONE, zone_entries)):
        blocks, firsts = cut(entries)
        tables += [(kind, first, raw) for first, raw in zip(firsts, blocks)]

    stored_names = [(k, fc, deflate(raw)) for k, fc, raw in name_blocks]
    stored_tables = [(kind, first, deflate(raw)) for kind, first, raw in tables]
    head_size = (HEADER.size + sum(NAME_ENTRY.size + len(k) for k, _, _ in stored_names) +
                 TABLE_ENTRY.size * len(stored_tables))
    return dict(records=records, aliases=len(items) - len(records), ccs=ccs, admin1=len(used_a1), posix=posix,
                stored_names=stored_names, stored_tables=stored_tables, head_size=head_size)


def places_file(t, version):
    head = bytearray(HEADER.pack(MAGIC, FORMAT, BLOCK, version, t["head_size"], len(t["stored_names"]),
                                 len(t["stored_tables"]), len(t["records"]), t["aliases"], len(t["ccs"]),
                                 t["admin1"], len(t["posix"]), 0))
    off = t["head_size"]
    for key, first_city, data in t["stored_names"]:
        head += NAME_ENTRY.pack(off, len(data), first_city, hashlib.sha256(data).digest(), len(key)) + key.encode()
        off += len(data)
    for kind, first, data in t["stored_tables"]:
        head += TABLE_ENTRY.pack(kind, first, off, len(data), hashlib.sha256(data).digest())
        off += len(data)
    assert len(head) == t["head_size"]
    body = b"".join(d for _, _, d in t["stored_names"]) + b"".join(d for _, _, d in t["stored_tables"])
    return bytes(head) + body


# ---------- firmware files ----------
GENERATED = "// Generated by tools/places/build.py: rebuild it (CONTRIBUTING.md), do not edit.\n"


def hex_lines(data, indent="  "):
    return "\n".join(indent + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ","
                     for i in range(0, len(data), 16))


def places_index(blob, version):
    head_size = HEADER.unpack_from(blob)[4]
    h = ("// SPDX-License-Identifier: AGPL-3.0-only\n" + GENERATED +
         "// What the panel trusts of data/places.bin, the city table it fetches in pieces from the\n"
         "// public repository (format: tools/places/FORMAT.md): its head (header and block index),\n"
         "// refused unless it hashes to PLACES_HEAD_SHA256, holds the SHA-256 of every block.\n"
         "#pragma once\n\n#include <stdint.h>\n\n"
         "static const uint32_t PLACES_VERSION = %d;\n"
         "static const uint32_t PLACES_HEAD_SIZE = %d;\n"
         "static const uint32_t PLACES_FILE_SIZE = %d;\n"
         "extern const uint8_t PLACES_HEAD_SHA256[32];\n") % (version, head_size, len(blob))
    cpp = ("// SPDX-License-Identifier: AGPL-3.0-only\n" + GENERATED +
           '#include "places_index.h"\n\n'
           "const uint8_t PLACES_HEAD_SHA256[32] = {\n%s\n};\n") % hex_lines(hashlib.sha256(blob[:head_size]).digest())
    return h, cpp


def tz_names(zones, version):
    posix = sorted(set(zones.values()))
    index = {p: i for i, p in enumerate(posix)}
    names, prev = bytearray(), b""
    for zone in zones:   # sorted by read_zones
        b = zone.encode("ascii")
        p = shared(prev, b)
        names += bytes([p, len(b) - p]) + b[p:] + bytes([index[zones[zone]]])
        prev = b
    if len(posix) > 256:
        raise SystemExit("%d POSIX strings: more than a byte numbers" % len(posix))
    longest = max(len(z) for z in zones)
    h = ("// SPDX-License-Identifier: AGPL-3.0-only\n" + GENERATED +
         "// Every IANA time zone name, links included (\"US/Eastern\"), and its POSIX TZ string, from\n"
         "// tzdata (public domain): the zone a phone reports at setup becomes the TZ the C library\n"
         "// applies, daylight saving included, without asking any web service.\n"
         "#pragma once\n\n#include <stdint.h>\n\n"
         "static const char TZ_NAMES_VERSION[] = \"%s\";\n"
         "static const uint16_t TZ_NAMES_COUNT = %d;\n"
         "static const uint8_t TZ_NAME_MAX = %d;\n"
         "static const uint32_t TZ_NAMES_BYTES = %d;   // both tables in flash\n\n"
         "// \"Europe/Paris\" -> \"CET-1CEST,M3.5.0,M10.5.0/3\"; nullptr for a name tzdata does not know\n"
         "const char *tzPosix(const char *name);\n") % (
        version, len(zones), longest, len(names) + sum(len(p) + 1 for p in posix) + 1)
    cpp = ("// SPDX-License-Identifier: AGPL-3.0-only\n" + GENERATED + "// tzdata %s.\n" % version +
           '#include "tz_names.h"\n\n#include <string.h>\n\n'
           "// The names in byte order, front-coded: how many bytes the name shares with the one\n"
           "// before, how many follow, those bytes, then the number of its POSIX string.\n"
           "static const uint8_t NAMES[] = {\n%s\n};\n\n"
           "// The POSIX strings, each once, in byte order, each ended by a NUL\n"
           "static const char POSIX[] =\n%s;\n\n"
           "const char *tzPosix(const char *name) {\n"
           "  if (!name) return nullptr;\n"
           "  char cur[TZ_NAME_MAX + 1];\n"
           "  size_t pos = 0;\n"
           "  for (uint16_t i = 0; i < TZ_NAMES_COUNT; i++) {\n"
           "    uint8_t kept = NAMES[pos], len = NAMES[pos + 1];\n"
           "    memcpy(cur + kept, NAMES + pos + 2, len);\n"
           "    cur[kept + len] = '\\0';\n"
           "    uint8_t zone = NAMES[pos + 2 + len];\n"
           "    pos += 3 + len;\n"
           "    int order = strcmp(cur, name);\n"
           "    if (order > 0) break;   // sorted: it is not further on\n"
           "    if (order < 0) continue;\n"
           "    const char *p = POSIX;\n"
           "    for (uint8_t z = 0; z < zone; z++) p += strlen(p) + 1;\n"
           "    return p;\n"
           "  }\n"
           "  return nullptr;\n"
           "}\n") % (hex_lines(names), "\n".join('  "%s\\0"' % p for p in posix))
    return h, cpp


def write(root, rel, data):
    path = os.path.join(root, *rel.split("/"))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data if isinstance(data, bytes) else data.encode("utf-8"))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--version", type=int, required=True, help="data version, raised at every rebuild")
    ap.add_argument("--data", help="folder of GeoNames dumps; missing ones are downloaded into it")
    ap.add_argument("--zoneinfo", default="/usr/share/zoneinfo", help="compiled tzdata tree")
    ap.add_argument("--out", default=ROOT, help="where data/ and firmware/src/ are written")
    args = ap.parse_args(argv)
    if not 0 < args.version <= 0xFFFFFFFF:
        ap.error("--version: 1 to 4294967295")

    zones, tz_version = read_zones(args.zoneinfo)
    with tempfile.TemporaryDirectory() as tmp:
        data_dir = args.data or tmp
        os.makedirs(data_dir, exist_ok=True)
        fetch(data_dir)
        t = table(data_dir, zones)
    blob = places_file(t, args.version)
    idx_h, idx_cpp = places_index(blob, args.version)
    tz_h, tz_cpp = tz_names(zones, tz_version)
    for rel, data in zip(OUTPUTS, (blob, idx_h, idx_cpp, tz_h, tz_cpp)):
        write(args.out, rel, data)
    print("places.bin: %d bytes, %d cities, %d aliases, %d name blocks, %d table blocks, head %d bytes" % (
        len(blob), len(t["records"]), t["aliases"], len(t["stored_names"]), len(t["stored_tables"]), t["head_size"]),
        file=sys.stderr)
    print("tz_names: %d names, %d POSIX strings, tzdata %s" % (len(zones), len(set(zones.values())), tz_version),
          file=sys.stderr)


if __name__ == "__main__":
    main()
