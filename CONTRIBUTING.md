# Contributing to Carousel

Thanks for your interest. This guide covers how to build, test and propose changes.

## Setup

- [PlatformIO Core](https://platformio.org/install/cli) for the firmware and its tests.
- Node.js 20+ for the simulator self-test and the web page checks.
- A POSIX `sh`, curl 7.66+ and Python 3 for the coding agent bridge and its tests.

```sh
cd firmware
cp include/secrets.example.h include/secrets.h   # optional, git-ignored
pio run                     # firmware
pio run -e hwtest           # standalone panel test
pio test -e native          # host unit tests
cd ..
node tools/simulator/selftest.mjs          # every simulator example, headless
sh tools/agent-bridge/test/test_hook.sh    # hook script against a fake panel (port 8099, or CAROUSEL_TEST_PORT)
sh tools/agent-bridge/test/test_install.sh # installer and setup script, in a fake home
node tools/agent-bridge/embed.mjs          # after editing a script of tools/agent-bridge: rewrites agent_scripts.cpp (--check in CI)
shellcheck -S warning tools/agent-bridge/*.sh tools/agent-bridge/test/*.sh
npm ci --prefix tools/webcheck             # once: Playwright and axe-core, development only
node tools/webcheck/webcheck.mjs           # web pages at 360 and 1280 px, after pio test
node tools/webcheck/assets.mjs             # after editing firmware/web/page.css: rewrites web_assets.cpp
```

CI runs all of the above on every pull request, except the web page checks for now. The end-to-end bridge test,
`python3 tools/agent-bridge/test/test_network.py [panel address]`, needs a real panel and the
install command run on your computer first; CI does not run it. Firmware sources build with
`-Wall -Wextra -Werror`: a warning fails the build.

## City table and time zones

The panel finds a typed city in `data/places.bin`, a table built from GeoNames, and turns the
phone's zone name into a POSIX TZ with `firmware/src/tz_names.cpp`, built from tzdata. Both come
from `tools/places/build.py` (Python 3, standard library only; format in
`tools/places/FORMAT.md`) and are never edited by hand. To rebuild them, raise the version and
commit the five files it writes together:

```sh
python3 -I tools/places/build.py --version 2 --data ~/geonames   # downloads the missing dumps, about 210 MB
python3 -I tools/places/test_build.py                            # hashes, lookups, a rebuild from a fixture
PLACES_DATA=~/geonames python3 -I tools/places/test_build.py      # also checks a rebuild gives the same bytes
```

It writes `data/places.bin`, `firmware/src/places_index.h` and `.cpp` (the version, size and
SHA-256 the firmware checks the file against) and `firmware/src/tz_names.h` and `.cpp`, from the
compiled tzdata in `/usr/share/zoneinfo` (`--zoneinfo` for another). A panel only trusts the
file its firmware was built with: commit a new `places.bin` with its `places_index.cpp`, never
apart.

## Web pages

Each page is built by a module that knows nothing of the web server: it takes a view
struct and writes HTML, so the host tests check the real markup and `web.cpp` only gathers
the data and sends it. `web_shell` (the head, brand row, tabs, footer, notices, posters,
`escapeHtml`), `web_stage` (Stage), `web_screens` (Running order), `web_images` (Images),
`web_panel` (Panel), `web_access` (Wi-Fi, sign-in, locked), `web_forms` (the capped browser
routes' gate and body) and `preview_logic` (Stage's picture). Each has its test in
`firmware/test/test_web_<name>/`; any value that does not come from the firmware is escaped
with `escapeHtml`, and each page test checks it fits 24 KB in its worst case.

`test_web_pages` writes every page in every state, in English and French, into
`firmware/.pio/webcheck/`; `node tools/webcheck/webcheck.mjs` then checks them in Chromium and
WebKit at 360 and 1280 px, with scripts on and off (overflow, 44 px targets, contrast, axe,
structure, focus, reduced motion, the live preview). A new page state goes into
`test_web_pages`.

The stylesheet's source is `firmware/web/page.css`: after editing it, run
`node tools/webcheck/assets.mjs`, which rewrites `firmware/src/web_assets.cpp` (webcheck fails
while the two differ). The screens' thumbnails are drawn by the screens themselves
(`test_poster_cells`): after changing a screen or adding one, run
`node tools/simulator/posters.mjs`, which rewrites `firmware/src/posters.cpp`. The pictures in
`docs/images/` come from `node tools/docs/shots.mjs` (after `pio test` and
`npm ci --prefix tools/webcheck`): rerun it when a page or a poster changes.

## Making a change

1. Branch from `main` (`feat/...`, `fix/...`, `docs/...`, `chore/...`).
2. Keep each pull request to one topic. Behaviour changes and refactors go in
   separate commits, or better, separate pull requests.
3. Add or update a test when you change logic that can be tested on the host
   (`firmware/test/`), the hook script (`tools/agent-bridge/test/`), or a simulator example
   (the self-test runs every example).
4. Describe in the pull request what you verified, and how: on hardware, in the
   simulator, or with which test.

Commit messages: an imperative summary line under ~70 characters, then a body that
explains *why* when it is not obvious.

## Code style

- Match the surrounding code: 2-space indentation in C++ and JavaScript, 4 in Python.
- Comments explain *why*: the constraint, the measurement, the bug they prevent. The
  code already says *what*.
- English for code, comments and logs. User-facing text goes through `firmware/src/i18n.cpp`.
- Every source file starts with `SPDX-License-Identifier: AGPL-3.0-only`.

## Adding a language

Add an entry to `Lang` in `firmware/src/i18n.h` and a table in `firmware/src/i18n.cpp`.
`pio test -e native` fails on any missing string and on any panel string wider than the
screen allows (64 px landscape, 32 px portrait). A string drawn in one of the panel's faces
(`firmware/src/type.h`) is measured in that face. It may carry the French accented letters
the face draws (ABONNÉS, FÉVRIER). A string drawn in the built-in 5x7 font counts 6 px per
character and stays upper-case ASCII, because that font has no upper-case accented letters.

## Adding a screen

1. Prototype it in `tools/simulator/simulator.html`: the API mirrors the firmware's
   (`display.*`, `drawMascot`, `isBlinking`, `drawBubble`...).
2. Port it to `firmware/src/screens/` and declare its functions in `screens/screens.h`.
3. Add one line at the end of the table in `firmware/src/screen_table.h`: its name, draw
   function, orientation, leave function and settings. At the end, so panels that stored a
   list get the new screen on, after the screens they know. The gallery, the stored list, the
   API, MQTT and the web pages all read that table.
4. Give it a name in every language (`firmware/src/i18n.cpp`).
5. If it has settings, declare them in `firmware/src/web_screens.cpp` (its fields and a
   `*_SETTINGS` entry for the table's last column); Running order draws its block under
   the screen. Otherwise `NO_SETTINGS`.
6. Give it a thumbnail for the web pages: give it a cell in
   `firmware/test/test_poster_cells` (its fixed inputs and how many frames it runs) and a
   poster line in `POSTERS` in `tools/simulator/posters.mjs`, then run
   `node tools/simulator/posters.mjs`, which rewrites `firmware/src/posters.cpp`.
   `test_posters` fails while a screen of the table has no poster.

## Things that must not change silently

- Hook state names, English and French (`idle`/`veille`, `thinking`/`reflex`, `tool`/`outil`...),
  on `/agent/<state>` and `/claude/<state>`, and the `X-Hook-Token` header: installed hooks send
  them. The French names and `/claude/` stay accepted for good.
- The install command and what it writes under `~/.config/carousel`: computers in the field
  hold a copy of the hook.
- NVS keys and the `panneau` NVS namespace: panels in the field store their settings there.
- localStorage keys in the simulator: users' saved animations and game scores live there.
- The `/img` folder of the `littlefs` partition and the rules that turn an uploaded file name
  into a stored name: panels keep users' images there.
- The NVS key `image` (the image chosen, in `panneau`).
- The NVS keys `scrList` (the owner's screen order and which are off, a string) and
  `rotateSec` (the rotation interval in seconds, a uint16), in `panneau`.
- The English screen names (`firmware/src/i18n.cpp`): the stored list and the start screen
  keep screens by name, and the API and MQTT take them. A renamed screen loses its place and
  its on/off state on every panel.
- The factory reset marker in the last sector of the `littlefs` partition: a panel that finds
  it resumes an unfinished reset at start.

## License

By contributing, you agree that your contribution is licensed under the
[GNU AGPL v3.0 only](LICENSE), like the rest of the project. Carousel is also a commercial
product: outside contributions will require signing a contributor licence agreement (CLA),
which is being set up. Please open an issue before starting significant work.
