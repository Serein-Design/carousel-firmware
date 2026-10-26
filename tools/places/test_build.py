# SPDX-License-Identifier: AGPL-3.0-only
"""Tests of the city table generator, against tools/places/FORMAT.md.

    python3 -I tools/places/test_build.py

The committed data/places.bin is read back with a reader written here from FORMAT.md (not with
the generator's code): its hashes against firmware/src/places_index.cpp, every block, and the
lookups the panel will make. Determinism and selection run on a small GeoNames-shaped fixture.
With PLACES_DATA=<GeoNames folder> set, the committed file is also rebuilt from the real dumps
and compared byte for byte (slow: alternateNamesV2 is 750 MB).
"""
import datetime
import hashlib
import os
import re
import struct
import sys
import tempfile
import unittest
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import build  # noqa: E402  (after the path: the test runs with -I)

HEADER = struct.Struct("<4sHHIIHHIIHHHH")
NAME_ENTRY = struct.Struct("<IHH32sB")
TABLE_ENTRY = struct.Struct("<BHIH32s")
CITY = struct.Struct("<hhBBBB")
PARIS_TZ = "CET-1CEST,M3.5.0,M10.5.0/3"
NEW_YORK_TZ = "EST5EDT,M3.2.0,M11.1.0"


class Places:
    """data/places.bin read as FORMAT.md describes it, hashes checked."""

    def __init__(self, data):
        self.data = data
        (magic, fmt, self.block_size, self.version, self.head_size, n_names, n_tables,
         self.cities, self.aliases, self.countries, self.admin1s, self.zones, _) = HEADER.unpack_from(data)
        assert magic == b"PLCS" and fmt == 1, (magic, fmt)
        pos = HEADER.size
        self.names = []
        for _ in range(n_names):
            off, length, first_city, sha, klen = NAME_ENTRY.unpack_from(data, pos)
            pos += NAME_ENTRY.size
            key = data[pos:pos + klen].decode("ascii")
            pos += klen
            self.names.append(dict(off=off, len=length, first_city=first_city, sha=sha, key=key))
        self.tables = []
        for _ in range(n_tables):
            table, first, off, length, sha = TABLE_ENTRY.unpack_from(data, pos)
            pos += TABLE_ENTRY.size
            self.tables.append(dict(table=table, first=first, off=off, len=length, sha=sha))
        assert pos == self.head_size, (pos, self.head_size)

    def inflate(self, entry):
        stored = self.data[entry["off"]:entry["off"] + entry["len"]]
        assert hashlib.sha256(stored).digest() == entry["sha"], "block hash"
        raw = zlib.decompress(stored, -15)
        assert len(raw) <= self.block_size, len(raw)
        return raw

    def records(self, i):
        raw, pos, prev, city = self.inflate(self.names[i]), 0, b"", self.names[i]["first_city"]
        out = []
        while pos < len(raw):
            p, s = raw[pos], raw[pos + 1]
            alias, s = bool(s & 0x80), s & 0x7F
            name = prev[:p] + raw[pos + 2:pos + 2 + s]
            pos += 2 + s
            rec = dict(name=name.decode("utf-8"), alias=alias)
            if alias:
                (rec["target"],) = struct.unpack_from("<H", raw, pos)
                pos += 2
            else:
                rec["lat"], rec["lon"], rec["cc"], rec["a1"], rec["tz"], rec["pop"] = CITY.unpack_from(raw, pos)
                rec["ordinal"] = city
                city += 1
                pos += CITY.size
            rec["key"] = build.fold(rec["name"])
            out.append(rec)
            prev = name
        return out

    def entry(self, table, n):
        block = max((t for t in self.tables if t["table"] == table and t["first"] <= n), key=lambda t: t["first"])
        raw = self.inflate(block)
        fields, pos = [], 0
        for _ in range(n - block["first"] + 1):
            if table == 0:
                cc, first_a1 = raw[pos:pos + 2].decode("ascii"), struct.unpack_from("<H", raw, pos + 2)[0]
                pos += 4
                en_end = raw.index(b"\0", pos)
                fr_end = raw.index(b"\0", en_end + 1)
                fields = (cc, first_a1, raw[pos:en_end].decode(), raw[en_end + 1:fr_end].decode())
                pos = fr_end + 1
            else:
                end = raw.index(b"\0", pos)
                fields = raw[pos:end].decode()
                pos = end + 1
        return fields

    def city(self, ordinal):
        i = max(j for j, e in enumerate(self.names) if e["first_city"] <= ordinal)
        while True:
            for rec in self.records(i):
                if not rec["alias"] and rec["ordinal"] == ordinal:
                    return rec
            i += 1

    def lookup(self, query):
        """Every place whose key is the query's, best first, aliases resolved to their city."""
        key = build.fold(query)
        firsts = [e["key"] for e in self.names]
        # The previous block may end with this key: a block's first key is a lower bound only
        i = max(0, sum(1 for k in firsts if k < key) - 1)
        found = []
        while i < len(self.names) and self.names[i]["key"] <= key:
            for rec in self.records(i):
                if rec["key"] == key:
                    city = self.city(rec["target"]) if rec["alias"] else rec
                    cc, first_a1, country, _ = self.entry(0, city["cc"])
                    found.append(dict(name=rec["name"], city=city["name"], cc=cc, country=country,
                                      admin1=self.entry(1, first_a1 + city["a1"]), zone=self.entry(2, city["tz"]),
                                      lat=city["lat"] / 100, lon=city["lon"] / 100, pop=city["pop"]))
            i += 1
        return found


def committed():
    with open(os.path.join(ROOT, "data", "places.bin"), "rb") as f:
        return f.read()


# ---------- the POSIX TZ string as the panel's C library reads it ----------
# newlib 4.1 (ESP-IDF 4.4), tzset_r.c and tzcalc_limits.c: an offset's sign is
# read apart, but a rule's time goes straight to sscanf "%hu", which takes
# "-1" as 65535 hours. A model of that, to check the strings the panel gets.
def newlib_hu(s, i):
    j = i + (i < len(s) and s[i] in "+-")
    k = j
    while k < len(s) and s[k].isdigit():
        k += 1
    if k == j:
        return None, i
    v = int(s[j:k])
    return (-v if s[i] == "-" else v) % 65536, k


def newlib_hms(s, i, signed):
    sign = 1
    if signed and s[i] in "+-":
        sign, i = (-1 if s[i] == "-" else 1), i + 1
    parts = []
    while len(parts) < 3:
        v, i2 = newlib_hu(s, i)
        if v is None:
            break
        parts.append(v)
        i = i2
        if len(parts) < 3 and i < len(s) and s[i] == ":":
            i += 1
        else:
            break
    if not parts:
        raise ValueError("no time at %r" % s[i:])
    parts += [0] * (3 - len(parts))
    return sign * (parts[0] * 3600 + parts[1] * 60 + parts[2]), i


def newlib_name(s, i):
    if s[i] == "<":
        return s.index(">", i) + 1
    j = i
    while j < len(s) and s[j].isalpha():
        j += 1
    if j - i < 3:
        raise ValueError("no zone name at %r" % s[i:])
    return j


def newlib_offset(posix, utc):
    """posix applied to utc (a datetime in UTC) as newlib does: the offset east of UTC, in seconds."""
    import calendar
    i = newlib_name(posix, 0)
    west_std, i = newlib_hms(posix, i, True)
    if i == len(posix):
        return -west_std
    i = newlib_name(posix, i)
    west_dst = west_std - 3600
    if i < len(posix) and posix[i] != ",":
        west_dst, i = newlib_hms(posix, i, True)
    year, changes = utc.year, []
    for west in (west_std, west_dst):   # DST starts in standard time, ends in daylight time
        if posix[i] != ",":
            raise ValueError("no rule at %r" % posix[i:])
        i += 1
        if posix[i] == "M":
            m, i = newlib_hu(posix, i + 1)
            w, i = newlib_hu(posix, i + 1)
            d, i = newlib_hu(posix, i + 1)
            first = (calendar.weekday(year, m, 1) + 1) % 7   # Sunday 0
            day = 1 + (d - first) % 7 + 7 * (w - 1)
            while day > calendar.monthrange(year, m)[1]:
                day -= 7
            midnight = calendar.timegm((year, m, day, 0, 0, 0))
        else:
            raise ValueError("a rule newlib reads otherwise: %r" % posix[i:])
        at = 7200
        if i < len(posix) and posix[i] == "/":
            at, i = newlib_hms(posix, i + 1, False)
        changes.append(midnight + at + west)
    if i != len(posix):
        raise ValueError("left over: %r" % posix[i:])
    t = calendar.timegm(utc.timetuple())
    start, end = changes
    dst = start <= t < end if start < end else (t < end or t >= start)
    return -(west_dst if dst else west_std)


NUUK = "<-02>2<-01>,M3.5.0/-1,M10.5.0/0"   # tzdata since 2023d: DST at 01:00 UTC, -1 local


class CommittedFile(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = committed()
        cls.places = Places(cls.data)

    def test_head_hash_size_and_version_are_the_firmware_s(self):
        with open(os.path.join(ROOT, "firmware", "src", "places_index.cpp")) as f:
            cpp = f.read()
        with open(os.path.join(ROOT, "firmware", "src", "places_index.h")) as f:
            h = f.read()
        pinned = bytes(int(b, 16) for b in re.findall(r"0x([0-9a-f]{2})", cpp))
        self.assertEqual(pinned, hashlib.sha256(self.data[:self.places.head_size]).digest())
        self.assertIn("PLACES_HEAD_SIZE = %d;" % self.places.head_size, h)
        self.assertIn("PLACES_VERSION = %d;" % self.places.version, h)
        self.assertIn("PLACES_FILE_SIZE = %d;" % len(self.data), h)

    def test_blocks_hash_inflate_and_cover_the_file(self):
        entries = sorted(self.places.names + self.places.tables, key=lambda e: e["off"])
        self.assertEqual(entries[0]["off"], self.places.head_size)
        for a, b in zip(entries, entries[1:]):
            self.assertEqual(a["off"] + a["len"], b["off"])
        self.assertEqual(entries[-1]["off"] + entries[-1]["len"], len(self.data))
        for e in entries:
            self.places.inflate(e)

    def test_records_are_in_key_order_and_counts_match(self):
        keys, cities, aliases = [], 0, 0
        for i, e in enumerate(self.places.names):
            recs = self.places.records(i)
            self.assertEqual(recs[0]["key"], e["key"])
            self.assertEqual(e["first_city"], cities)
            for r in recs:
                keys.append(r["key"])
                if r["alias"]:
                    aliases += 1
                    self.assertLess(r["target"], self.places.cities)
                else:
                    cities += 1
        self.assertEqual(keys, sorted(keys))
        self.assertEqual((cities, aliases), (self.places.cities, self.places.aliases))

    def test_paris_is_the_french_capital_first(self):
        found = self.places.lookup("Paris")
        self.assertEqual((found[0]["cc"], found[0]["admin1"], found[0]["zone"]), ("FR", "Île-de-France", PARIS_TZ))
        self.assertAlmostEqual(found[0]["lat"], 48.85, delta=0.02)
        self.assertIn("US", [f["cc"] for f in found[1:]])

    def test_saint_etienne_folds_accent_and_hyphen(self):
        found = self.places.lookup("saint etienne")
        self.assertEqual((found[0]["name"], found[0]["cc"]), ("Saint-Étienne", "FR"))

    def test_munich(self):
        found = self.places.lookup("munich")
        self.assertEqual((found[0]["cc"], found[0]["country"], found[0]["zone"]),
                         ("DE", "Germany", "CET-1CEST,M3.5.0,M10.5.0/3"))
        self.assertAlmostEqual(found[0]["lat"], 48.14, delta=0.02)

    def test_native_names_are_found(self):
        for query, city, cc in (("munchen", "Munich", "DE"), ("koln", "Köln", "DE"),
                                ("firenze", "Florence", "IT"), ("sevilla", "Sevilla", "ES")):
            found = self.places.lookup(query)
            self.assertTrue(found, query)
            self.assertEqual((found[0]["city"], found[0]["cc"]), (city, cc), query)

    def test_size_stays_under_the_cap(self):
        self.assertLess(len(self.data), 450 * 1024)

    def test_new_york_through_its_alias(self):
        found = self.places.lookup("new york")
        self.assertEqual((found[0]["city"], found[0]["cc"], found[0]["zone"]), ("New York City", "US", NEW_YORK_TZ))

    def test_springfield_lists_the_largest_first(self):
        found = self.places.lookup("springfield")
        self.assertGreaterEqual(len(found), 3)
        self.assertEqual({f["cc"] for f in found}, {"US"})
        self.assertEqual(found[0]["admin1"], "Missouri")
        self.assertEqual([f["pop"] for f in found], sorted((f["pop"] for f in found), reverse=True))

    def test_country_names_in_english_and_french(self):
        countries = {c[0]: c[2:] for c in (self.places.entry(0, n) for n in range(self.places.countries))}
        self.assertEqual(countries["DE"], ("Germany", "Allemagne"))
        self.assertEqual(countries["US"], ("United States", "États-Unis"))

    @unittest.skipUnless(os.environ.get("PLACES_DATA"), "PLACES_DATA not set")
    def test_rebuild_from_the_dumps_gives_the_same_bytes(self):
        with tempfile.TemporaryDirectory() as out:
            build.main(["--data", os.environ["PLACES_DATA"], "--version", str(self.places.version), "--out", out])
            with open(os.path.join(out, "data", "places.bin"), "rb") as f:
                self.assertEqual(f.read(), self.data)


# ---------- fixture: a few GeoNames rows, real where they matter ----------
def city_row(gid, name, ascii_name, lat, lon, code, cc, a1, pop, tz):
    return "\t".join([str(gid), name, ascii_name, "", str(lat), str(lon), "P", code, cc, "", a1, "", "", "",
                      str(pop), "", "", tz, "2026-01-01"])


CITIES = [
    city_row(2988507, "Paris", "Paris", 48.85341, 2.3488, "PPLC", "FR", "11", 2138551, "Europe/Paris"),
    # 9 km from Paris: merged into it
    city_row(3031137, "Boulogne-Billancourt", "Boulogne-Billancourt", 48.83545, 2.24128, "PPLA3", "FR", "11", 121334, "Europe/Paris"),
    city_row(2980291, "Saint-Étienne", "Saint-Etienne", 45.43389, 4.39, "PPLA2", "FR", "84", 176280, "Europe/Paris"),
    city_row(2995469, "Marseille", "Marseille", 43.29695, 5.38107, "PPLA", "FR", "93", 870731, "Europe/Paris"),
    city_row(2996944, "Lyon", "Lyon", 45.74846, 4.84671, "PPLA", "FR", "84", 522250, "Europe/Paris"),
    # under 15 000 and not among France's top three: left out
    city_row(3000000, "Petitbourg", "Petitbourg", 46.0, 1.0, "PPL", "FR", "75", 900, "Europe/Paris"),
    # a section of a city: never a place of its own
    city_row(3000001, "Quartier", "Quartier", 44.0, 1.0, "PPLX", "FR", "75", 50000, "Europe/Paris"),
    city_row(4717560, "Paris", "Paris", 33.66094, -95.55551, "PPLA2", "US", "TX", 24782, "America/Chicago"),
    city_row(5128581, "New York City", "New York City", 40.71427, -74.00597, "PPL", "US", "NY", 8804190, "America/New_York"),
    city_row(4409896, "Springfield", "Springfield", 37.21533, -93.29824, "PPLA2", "US", "MO", 170188, "America/Chicago"),
    city_row(4951788, "Springfield", "Springfield", 42.10148, -72.58981, "PPLA2", "US", "MA", 154341, "America/New_York"),
    city_row(4140963, "Washington", "Washington", 38.89511, -77.03637, "PPLC", "US", "DC", 689545, "America/New_York"),
    city_row(2867714, "Munich", "Munich", 48.13743, 11.57549, "PPLA", "DE", "02", 1505005, "Europe/Berlin"),
    city_row(3164603, "Florence", "Florence", 43.77925, 11.24626, "PPLA", "IT", "16", 372039, "Europe/Rome"),
    city_row(2761369, "Vienna", "Vienna", 48.20849, 16.37208, "PPLC", "AT", "09", 1691468, "Europe/Vienna"),
    city_row(2950159, "Berlin", "Berlin", 52.52437, 13.41053, "PPLC", "DE", "16", 3426354, "Europe/Berlin"),
    city_row(3093133, "Łódź", "Lodz", 51.75, 19.46667, "PPLA", "PL", "74", 768755, "Europe/Warsaw"),
    city_row(2624906, "Ærøskøbing", "Aeroskobing", 54.88803, 10.41117, "PPL", "DK", "21", 950, "Europe/Copenhagen"),
    # a tiny country: its towns are its top three, its capital stays whatever its size
    city_row(3042030, "Vaduz", "Vaduz", 47.14151, 9.52154, "PPLC", "LI", "11", 5197, "Europe/Vaduz"),
    city_row(3042041, "Schaan", "Schaan", 47.16498, 9.50867, "PPLA", "LI", "07", 5748, "Europe/Vaduz"),
]
COUNTRIES = [   # code, name, capital, GeoNames id, languages
    ("AT", "Austria", "Vienna", 2782113, "de-AT,hr,hu,sl"),
    ("DE", "Germany", "Berlin", 2921044, "de"), ("DK", "Denmark", "Copenhagen", 2623032, "da-DK,en,fo,de-DK"),
    ("FR", "France", "Paris", 3017382, "fr-FR,frp,br,co,ca,eu,oc"), ("IT", "Italy", "Rome", 3175395, "it-IT,de-IT,fr-IT,sc,ca,co,sl"),
    ("LI", "Liechtenstein", "Vaduz", 3042058, "de-LI"),
    ("PL", "Poland", "Warsaw", 798544, "pl"), ("US", "United States", "Washington", 6252001, "en-US,es-US,haw,fr"),
]
ADMIN1 = [("FR.11", "Île-de-France"), ("FR.84", "Auvergne-Rhône-Alpes"), ("FR.93", "Provence-Alpes-Côte d'Azur"),
          ("FR.75", "Nouvelle-Aquitaine"), ("US.TX", "Texas"), ("US.NY", "New York"), ("US.MO", "Missouri"),
          ("US.MA", "Massachusetts"), ("US.DC", "Washington, D.C."), ("DE.02", "Bavaria"), ("DE.16", "Berlin"), ("IT.16", "Tuscany"), ("AT.09", "Vienna"),
          ("PL.74", "Łódź Voivodeship"), ("DK.21", "South Denmark"), ("LI.11", "Vaduz"), ("LI.07", "Schaan")]
ALTERNATES = [
    (1, 5128581, "en", "New York", "1", "1", "", ""), (2, 5128581, "en", "Big Apple", "", "", "1", ""),
    (3, 5128581, "fr", "New York", "", "", "", ""), (4, 2980291, "fr", "St Étienne", "", "1", "", ""),
    (5, 2867714, "fr", "Munich", "", "", "", ""), (6, 2867714, "de", "München", "", "", "", ""),
    (7, 2921044, "fr", "Allemagne", "1", "", "", ""), (8, 3017382, "fr", "France", "1", "", "", ""),
    (9, 6252001, "fr", "États-Unis", "1", "", "", ""), (10, 2988507, "fr", "Lutèce", "", "", "", "1"),
    (11, 2624906, "en", "Aeroskobing (old)", "", "", "", ""),
    (12, 2867714, "it", "Monaco di Baviera", "", "", "", ""), (13, 2867714, "de", "Muenchen", "", "", "", ""),
    (14, 3164603, "it", "Firenze", "1", "", "", ""), (15, 3164603, "de", "Florenz", "", "", "", ""),
    (16, 2761369, "de", "Wien", "", "", "", ""),
]
ZONES = {"Europe/Paris": PARIS_TZ, "Europe/Berlin": PARIS_TZ, "Europe/Warsaw": PARIS_TZ,
         "Europe/Copenhagen": PARIS_TZ, "Europe/Vaduz": PARIS_TZ, "America/New_York": NEW_YORK_TZ,
         "America/Chicago": "CST6CDT,M3.2.0,M11.1.0", "Europe/Rome": PARIS_TZ, "Europe/Vienna": PARIS_TZ, "US/Eastern": NEW_YORK_TZ, "UTC": "UTC0"}


def write_fixture(root):
    data, zi = os.path.join(root, "geonames"), os.path.join(root, "zoneinfo")
    os.makedirs(data)
    with open(os.path.join(data, "cities500.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(CITIES) + "\n")
    with open(os.path.join(data, "countryInfo.txt"), "w", encoding="utf-8") as f:
        f.write("#ISO\tISO3\t...\n")
        for cc, name, capital, gid, languages in COUNTRIES:
            f.write("\t".join([cc, cc + "X", "0", cc, name, capital, "0", "0", "EU", "", "", "", "", "", "",
                               languages, str(gid), "", ""]) + "\n")
    with open(os.path.join(data, "admin1CodesASCII.txt"), "w", encoding="utf-8") as f:
        f.write("".join("%s\t%s\t%s\t0\n" % (code, name, name) for code, name in ADMIN1))
    with open(os.path.join(data, "alternateNamesV2.txt"), "w", encoding="utf-8") as f:
        f.write("".join("\t".join(map(str, a)) + "\t\t\n" for a in ALTERNATES))
    for name, posix in ZONES.items():
        path = os.path.join(zi, *name.split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"TZif2" + bytes(39) + b"\nNOT-THE-LAST-LINE\n" + b"\n" + posix.encode() + b"\n")
    with open(os.path.join(zi, "zone.tab"), "w") as f:
        f.write("# not a zone\n")
    with open(os.path.join(zi, "+VERSION"), "w") as f:
        f.write("2099z\n")
    return data, zi


class Fixture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.data, cls.zi = write_fixture(cls.tmp.name)
        cls.outs = []
        for run in ("a", "b"):
            out = os.path.join(cls.tmp.name, run)
            build.main(["--data", cls.data, "--zoneinfo", cls.zi, "--version", "7", "--out", out])
            cls.outs.append(out)
        with open(os.path.join(cls.outs[0], "data", "places.bin"), "rb") as f:
            cls.places = Places(f.read())

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_same_input_same_bytes(self):
        for rel in build.OUTPUTS:
            with open(os.path.join(self.outs[0], rel), "rb") as a, open(os.path.join(self.outs[1], rel), "rb") as b:
                self.assertEqual(a.read(), b.read(), rel)

    def test_selection(self):
        names = {r["name"] for i in range(len(self.places.names)) for r in self.places.records(i) if not r["alias"]}
        self.assertIn("Vaduz", names)            # capital and top three of a small country
        self.assertIn("Schaan", names)
        self.assertIn("Ærøskøbing", names)       # Denmark's only town in the fixture
        self.assertNotIn("Boulogne-Billancourt", names)   # 9 km from Paris
        self.assertNotIn("Petitbourg", names)    # small, not in France's top three
        self.assertNotIn("Quartier", names)      # PPLX
        self.assertEqual(self.places.version, 7)

    def test_aliases_french_and_english_only(self):
        self.assertEqual(self.places.lookup("new york")[0]["city"], "New York City")
        self.assertEqual(self.places.lookup("st etienne")[0]["city"], "Saint-Étienne")
        self.assertEqual(self.places.lookup("munchen")[0]["city"], "Munich")   # Germany's language
        self.assertEqual(self.places.lookup("monacodibaviera"), [])   # Italian: not Germany's
        self.assertEqual(self.places.lookup("big apple"), [])    # colloquial
        self.assertEqual(self.places.lookup("lutece"), [])       # historic
        self.assertEqual(self.places.lookup("firenze")[0]["city"], "Florence")   # Italy's language
        self.assertEqual(self.places.lookup("florenz"), [])      # German name of an Italian city
        self.assertEqual(self.places.lookup("wien")[0]["city"], "Vienna")   # "de-AT" counts as de
        self.assertEqual(self.places.aliases, 6)

    def test_fold_special_letters(self):
        self.assertEqual(build.fold("Łódź"), "lodz")
        self.assertEqual(build.fold("Ærøskøbing"), "aeroskobing")
        self.assertEqual(build.fold("Straße"), "strasse")
        self.assertEqual(build.fold("Saint-Étienne"), "saintetienne")
        self.assertEqual(build.fold("L'Haÿ-les-Roses ﬁ 2"), "lhaylesrosesfi2")
        self.assertEqual(self.places.lookup("lodz")[0]["admin1"], "Łódź Voivodeship")

    def test_tables(self):
        found = self.places.lookup("paris")
        self.assertEqual([(f["cc"], f["admin1"], f["zone"]) for f in found],
                         [("FR", "Île-de-France", PARIS_TZ), ("US", "Texas", "CST6CDT,M3.2.0,M11.1.0")])
        self.assertEqual(self.places.lookup("munich")[0]["country"], "Germany")
        self.assertEqual(self.places.entry(0, 3)[2:], ("France", "France"))
        self.assertEqual(self.places.entry(0, 7)[2:], ("United States", "États-Unis"))
        self.assertEqual(self.places.entry(0, 2)[2:], ("Denmark", "Denmark"))   # no French name: English

    def test_firmware_files(self):
        with open(os.path.join(self.outs[0], "firmware", "src", "tz_names.cpp")) as f:
            tz = f.read()
        self.assertIn("2099z", tz)
        self.assertIn('"CST6CDT,M3.2.0,M11.1.0\\0"', tz)
        self.assertEqual(tz.count(NEW_YORK_TZ), 1)    # US/Eastern shares New York's string
        with open(os.path.join(self.outs[0], "firmware", "src", "tz_names.h")) as f:
            self.assertIn("TZ_NAMES_COUNT = %d;" % len(ZONES), f.read())
        for rel in build.OUTPUTS:
            if rel.endswith((".h", ".cpp")):
                with open(os.path.join(self.outs[0], rel)) as f:
                    self.assertTrue(f.readline().startswith("// SPDX-License-Identifier: AGPL-3.0-only"), rel)


class NewlibZones(unittest.TestCase):
    JANUARY, JULY = datetime.datetime(2027, 1, 15, 12), datetime.datetime(2027, 7, 15, 12)

    def test_the_model_reads_a_known_zone(self):
        self.assertEqual(newlib_offset(PARIS_TZ, self.JANUARY), 3600)
        self.assertEqual(newlib_offset(PARIS_TZ, self.JULY), 7200)
        self.assertEqual(newlib_offset("<+0545>-5:45", self.JULY), 5 * 3600 + 45 * 60)
        # Nuuk as tzdata writes it: newlib reads -1 as 65535 hours, so the
        # winter is taken for summer
        self.assertEqual(newlib_offset(NUUK, self.JANUARY), -3600)

    def test_nuuk_as_the_panel_gets_it(self):
        posix = build.newlib_posix(NUUK)
        self.assertEqual(newlib_offset(posix, self.JANUARY), -7200)
        self.assertEqual(newlib_offset(posix, self.JULY), -3600)
        self.assertNotIn("/-", posix)
        self.assertEqual(build.newlib_posix(PARIS_TZ), PARIS_TZ)

    def test_every_committed_zone_reads_whole_with_no_negative_time(self):
        with open(os.path.join(ROOT, "firmware", "src", "tz_names.cpp")) as f:
            zones = re.findall(r'^  "(.*)\\0"$', f.read(), re.M)
        places = Places(committed())
        zones += [places.entry(2, z) for z in range(HEADER.unpack_from(places.data)[11])]
        self.assertGreater(len(zones), 150)
        for z in zones:
            self.assertEqual(build.newlib_posix(z), z, z)
            for when in (self.JANUARY, self.JULY):
                newlib_offset(z, when)   # raises on what newlib would not read whole


class Refusals(unittest.TestCase):
    def test_a_name_longer_than_the_firmware_holds(self):
        # Records::name holds 127 bytes and a NUL: a 128-byte alias, of which
        # only 123 follow the "Paris" before it, is refused all the same
        with tempfile.TemporaryDirectory() as tmp:
            data, zi = write_fixture(tmp)
            with open(os.path.join(data, "alternateNamesV2.txt"), "a", encoding="utf-8") as f:
                f.write("\t".join(map(str, (17, 2988507, "fr", "Paris" + "e" * 123, "", "", "", ""))) + "\t\t\n")
            with self.assertRaisesRegex(SystemExit, "name too long"):
                build.main(["--data", data, "--zoneinfo", zi, "--version", "7", "--out", os.path.join(tmp, "out")])


if __name__ == "__main__":
    unittest.main()
