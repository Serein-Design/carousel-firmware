# Changelog

All notable changes to this project. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.2.0] - 2026-10-26

### Changed
- Weather: the forecast comes from MET Norway (api.met.no, CC BY 4.0) instead of Open-Meteo,
  over HTTPS, about once an hour instead of every 15 min: never before the answer's Expires,
  at each panel's own minute of the hour, with If-Modified-Since, and a longer wait when the
  server refuses. Today's low and high come from the day's hourly temperatures and are kept
  across the day's answers. A reading counts as old after 2 h 10 min (was 35 min).
- The forecast, the city searches, update checks and the SOCIAL counters take turns in one
  background task instead of three: 24 KB more free memory, which MET Norway's secure
  connection needs once a city has been searched for.
- Location: the city is found in a table built from GeoNames data (CC BY 4.0),
  `data/places.bin`, which the panel reads in checked pieces from this repository, instead
  of Open-Meteo's plain-HTTP geocoder: no online geocoder, no account. The settings page
  suggests cities as the owner types (`GET /places`). The time zone comes with the city, as a
  POSIX rule with its daylight saving, stored under the new NVS key `zone`; at Wi-Fi setup
  the phone's zone gives it at once. A panel updated from an earlier version takes its
  place's zone from the table at its first start.
- WEATHER's settings page suggests places as the city is typed: from 2 characters, up to 5
  buttons ("Paris · Île-de-France, France") under the field, by keyboard (arrows, Enter,
  Escape) or touch, announced to screen readers. A choice posts the place's id with its
  name; without scripts the field posts the text as before.
- The Stage and Running order pages credit the data in their footer: "Weather: MET Norway
  (CC BY 4.0) · Places: GeoNames (CC BY 4.0)".
- Simulator: the examples that drew third-party characters, memes, brands or logos are gone
  (15 of 96). Fan art can still be loaded from a .json file of the community pack with Open...
  (nothing is fetched online). FOLLOWERS draws a generic follower icon.
- Coding agent installer: an agent counts as installed only when its command is on the PATH
  (Cursor: `cursor`, `cursor-agent` or the Mac app), not when its config folder exists. For an
  agent not found, the Carousel hooks an earlier run left in its file are removed.
- Ambiances: FIRE, STARS, PLASMA, LIFE and BLACKHOLE are calm now. Every colour comes from one
  ramp (dark red, orange, beige) capped at 70 % of full light, and every pace follows the
  clock, not the frame rate. FIRE has embers in the lower third with a spark now and then.
  STARS has fixed stars, each twinkling on its own cycle, and a shooting star every 20 s.
  PLASMA runs at a third of its old pace. LIFE steps four generations a second and a new
  seed dissolves in every 30 s, or at once when the colony dies out. BLACKHOLE turns about
  eight times slower, with a black core, a steady edge and no jets.
- BALL, MATRIX, RAYS and BARS are off in the default list. A panel that never saved a list
  no longer shows them. They stay on Running order, to turn on by hand.
- DATE, SYSTEM, MONEY and SOCIAL follow the panel's grid, faces and colours. DATE shows the
  weekday and month in full and no longer cuts the year, with the week and the day of the
  year at the foot. SYSTEM shows free memory with its share, the signal in bars and dBm, the
  frame rate and the uptime. MONEY shows the day's euros with the cents and the month's
  total. SOCIAL shows one counter at a time, changing every 4 s.
- TEXT scrolls its message in the body face at 24 px/s. The message keeps its French accents
  (from the Running order page, Stage, `/api/text` and MQTT `text/set`), and every printable
  ASCII character is drawn, `* " # @ < >` included. Other accented letters lose their
  accent. A message too long for its accents in 95 bytes loses the last accents first, so
  every character is kept.
- Mascot screens: the greeting and the "?" sit in bubbles in the body face, the Zs of SLEEP
  fade out in the faces, and RUN's score is in large digits at the right, with BUG! and the
  best score after a crash.
- The posters on Running order and in the docs are drawn by the firmware's own screen code.
- Panel states on the system type, colours and grid: the sign-in code in six large digits (3 + 3)
  with a line counting its time left; a locked panel says LOCKED, RESET NOT DONE, RESTART IT;
  no image says how to add one, with the panel's address in the accent colour; WEATHER and
  SOCIAL keep their last reading, dimmed, with the reason (Offline, No key, Server) in a foot
  band when it is more than two fetches old.
- Brightness changes ease over a quarter of a second instead of jumping; the night comes in over
  its first three minutes and goes out over its last three (a touch at night still wakes the panel
  at once). After a software restart (an update, the page's restart) the boot skips the mark and
  starts at the word; a power-on keeps the whole sequence.
- A short press on the knob acts when you let go, so a long press never also changes the
  brightness.
- A message sent to `/api/notify` or from Home Assistant is now a band across the top
  that scrolls the text twice and slides away; the screen keeps running below it, with no full
  screen and no time bar. Without a colour it uses the panel's text colour. It no longer
  restarts the rotation's count. The Feedback page can reduce it to a chip or to nothing.
- The screen's name banner on a knob turn is replaced by what you pick on the Feedback page
  (a band with the place, the neighbouring screens, dots, or nothing).
- The pages' header carries the se/de mark.
- The Wi-Fi setup screen has three steps that take turns (join the network, open the address,
  a QR code).
- Wi-Fi page: the new look, laid out like a phone's Wi-Fi settings. The saved network comes
  first and is chosen; Continue then retries it with its stored password (this replaces "Keep
  the current network and retry"). If it is out of range, the page says so. The other networks
  are folded under it (open when nothing is saved or in range), one row each, strongest first,
  with a lock when secured and bars and a word for the signal instead of dBm. One password
  field: with scripts on it shows under the chosen row, only when that row needs one; without
  scripts it sits under the list. A name field for a network not listed wins when filled. The
  confirmation names the network saved. While the panel scans, the
  page waits for the list instead of reloading itself. Its stylesheet is sent compressed once
  per firmware version and kept by the browser, instead of inside every page.
- Sign-in page: the new look, with a drawing of what the panel shows (the digits hidden: the
  page never carries the code); a wrong code marks the field. With scripts on, the code is
  typed in six one-digit boxes: each digit moves to the next box, Backspace goes back, a pasted
  code (or a phone's code suggestion) fills them all, and the sixth digit signs in; without
  scripts, one field as before. After sign-in the browser goes
  back to the page it asked for (`/`, `/screens`, `/images`, `/panel` or `/wifi`; anything
  else goes home).
- Locked panel (unfinished factory reset): every page and browser route answers the locked
  page, in the new look, with status 503, and changes nothing; before, the home page still
  showed the settings. Development builds (`-DSCREENS_TEST_CONSOLE`) lock the panel from the
  serial console, without erasing anything, until a restart: `k` as a reset the next start
  resumes, `j` as a knob reset.
- Images (`/images`): a page of its own, in the new look. Each stored GIF shows as itself,
  loaded as it scrolls into view, with its name, size and orientation, "Panel shows this" on
  the chosen one and a Delete button that names it; with none, the frame IMAGE shows instead.
  An upload answers this page, its notice above Add a GIF; Delete and Show come back to it. The
  files are served at `/images/f` only to a signed-in browser, only for a name the panel
  stores exactly, as `image/gif` that the browser may not reinterpret, at an address that
  changes with every add, replace and delete.
- Running order (`/screens`): the screen list on a page of its own, in the new look, with the
  same boxes, arrows and refusals. Each screen shows its poster; the one on show is drawn
  larger with its place among the screens playing; Save and the count of screens on stay in
  view on a phone and sit in the side column on a wide screen. IMAGE's block says which image
  the panel shows and links to Images, where its forms now are; SOCIAL's block shows the
  YouTube and GitHub counts and their last update, which leave the settings page. A save
  answers the page it came from and stores only that page's fields; a save without the list
  and the interval no longer restarts the rotation.
- Panel (`/panel`): language, night mode, updates (the firmware row, automatic updates, Check
  for updates, and Install while one is available) and Wi-Fi (the saved network, the signal in
  dBm with "good", "fair" or "weak", Change Wi-Fi), then, folded under Advanced, Home
  Assistant with its state, the Claude Code hooks with the session count, the hook address and
  Clear the session list, the local API token with Sign out every other browser, and free
  memory with the last restart and how long ago. Save says what it saves; the MQTT password is
  never sent back.
- The settings page is gone, and its inline style block with it: Stage is at `/`, Running
  order at `/screens`, Images at `/images`, Panel at `/panel`. Updates, sign-out and Clear the
  session list are on Panel. Its Status lines moved: the signal to Panel's Wi-Fi, memory
  and the last restart to Advanced, the firmware row to Updates, the YouTube and GitHub counts
  to Running order, the sessions to Stage, the hook address to Panel's hooks.
- The message, location, income, YouTube, GitHub and image settings moved under their screens
  in the screen list, now Running order.
- `POST /api/screen` answers 409 for a screen that is off (404 stays for an unknown name or
  the other orientation); MQTT `screen/set` ignores it. An image upload with `show=1`, or an
  image chosen, while IMAGE is off is stored without changing the screen.
- Factory reset also erases the images and the crash dump, in order, each read back blank,
  resumed at the next start and never reported done on failure; the panel stays locked
  (RESET FAILED, setup access point, no saved setting read) until it completes. Firmware
  slots, `otadata`, bootloader and partition table are kept: they hold no user data.
- Small text on the web pages (hints, footer) at 4.5:1 contrast or more.
- Sizes on the web pages in KB, and "Ko" in French (the free memory said "KB" there), with a
  no-break space before the unit.
- Partition table `partitions.csv`, fixed for good on 4 MB boards: two 1.75 MB OTA slots
  (the app used 83 % of the old 1.25 MB ones, 64 % now), 384 KB LittleFS for content, crash
  dump area. NVS stays in place. An existing board needs one cable flash to get it; CI fails
  a build above 80 % of a slot.
- ISS screen named "ISS DEMO" and its distance bubble alternates with "DEMO": the orbit is
  simulated, the distance was not to be taken as real.
- TLS tasks (updates, YouTube/GitHub) run at idle priority: certificate checks are seconds
  of computation and starved the idle task until the watchdog reset the board.
- DMA allocation failure at boot restarts the board instead of leaving it dark.
- First boot shows the clock instead of the Claude Code monitor; boots at brightness 100
  instead of 255; a dimmer level (10) joins the button's cycle.
- Setup access point named `CAROUSEL-XXXX` (end of the MAC, shown on the panel); opens at
  once on a panel with no saved network; opens after 3 password refusals; the panel and the
  Wi-Fi page say why the last attempt failed, and that only 2.4 GHz networks are seen.
- CI: hook tests and shellcheck run on every pull request; PlatformIO pinned; firmware
  sources build with `-Wall -Wextra -Werror`.
- WEATHER's Glance drops the degree signs from its low and high when two °F readings would
  take more than half the line.
- MONEY's month total drops its currency symbol when it does not fit (CHF from 100 000 a
  month).
- The Claude Code bridge became a coding agent bridge. Hooks call `/agent/<state>` with English
  state names (`idle`, `thinking`, `tool`, `attention`, `error`, `done`, `failed`, `end`). The
  hook is a POSIX `sh` script, so it runs on the `/bin/sh` of a stock Mac, which the bash 5
  script never did. It sends the token in a header, sends it only to a panel that proves it
  holds it, and ignores Claude Code hooks that Cursor runs (Cursor reports those sessions
  itself). The Panel page names coding agent hooks, no longer one product.
- The hook looks for the panel on the whole `/24` only on a network where it has already
  seen that panel (the address prefix and the router's hardware address). It tried every
  network before. The panel's last address and its `.local` name are still tried everywhere.

### Fixed
- SYSTEM's uptime no longer wraps after 49 days.
- The LEDs no longer flash at power-up and when the power is cut.
- CLOCK could wait up to 20 s for the time after the Wi-Fi came up. The panel now asks the
  router's own time server when the router names one, then the public ones in turn, a new one
  every 5 s; the date of the first weather answer sets the clock too if it comes first. Until
  the time is known, CLOCK shows dim dashes and no word.
- Weather: with a Wi-Fi slower than 6 s to connect, the first forecast came 15 min late.
- Panel flicker on every screen, at any brightness and clock speed: screens drew straight into
  the panel's buffer while it was on show. Each frame is now drawn off screen and only the
  pixels that changed are sent to the panel (8 KB more RAM).
- Serial console: the panel clock speeds read 10 and 20 MHz, what the ESP32 really gives;
  "16 MHz" was 20 MHz too and is gone, and the refresh rate shown follows the real clock.
- Wi-Fi setup access point: it closed within seconds when the saved network was in range
  but its password wrong. It now stays open until the saved network really connects.

### Security
- The web pages ask for a 6-digit code shown on the panel; the browser is then
  remembered by a `SameSite=Strict` cookie (CSRF). Unknown `Host` headers are refused (DNS
  rebinding). "Sign out every other browser" on Panel.
- The saved YouTube key is no longer sent back in Running order.
- HTTPS server certificates are now checked (YouTube, GitHub, updates) against Mozilla's root
  list embedded in the firmware; the YouTube key no longer goes to whoever answers.
- Serial console: a command is one letter then Enter. Single bytes ran commands, so noise
  on RX could reboot the board or change the panel timing.
- The old `/claude?e=` routes are gone: their POST loaded the whole body in memory.
- Stage's actions (`/show`, `/brightness`, `/text`) and the Wi-Fi form (`/wifi`) check the
  host, the origin, the session cookie (never the API token), the announced size (at most
  512 bytes, a negative one refused) and the lock before one byte of their body is read, then
  read at most 512 bytes into a buffer wiped and freed before the answer; a multipart body is
  refused with 415.
- A Wi-Fi password or sign-in code read from a form is wiped from memory with writes the
  compiler cannot drop (the plain wipe was optimised away). Signed-in pages are sent with
  `Cache-Control: no-store`, so the browser keeps no copy of them, the API token included.
- Hook calls need the panel's hook token, in the `X-Hook-Token` header (a 128-bit secret of
  its own, created at first start, kept by "Sign out every other browser", replaced by Renew
  and a factory reset). Without it, or with an old one, the panel answers 401 before reading
  the body, and changes nothing. Before this, anyone on the network could post a session
  state. The setting "Accept hooks without a token" under Advanced, off by default, brings the
  old open behaviour back for development.
- The hook sends its token only to an address whose `/id` names the panel and proves it holds
  the token (`/id?c=<nonce>` answers with an HMAC over the nonce and the panel's
  address), so a device that copies the panel's name gets nothing. Without a SHA-256 tool
  (`sha256sum`, `shasum` or `openssl`) the hook sends nothing and logs once what to install.
  The end of a session goes at once only to an address proven in the last 10 minutes;
  otherwise it is proven first within the same 0.7 s, or dropped.

### Added
- Status strip: on the screens drawn on the grid (CODE, CLOCK, WEATHER, DATE, SYSTEM, MONEY,
  SOCIAL, TEXT), the bottom row fills a thin line over the time until the next screen. A
  2-pixel mark at its right end shows the coding agents: the accent colour when a session
  needs you, dim while one works. The line disappears for 30 s after a touch or a request.
- A product tour (`docs/tour.md`) with every screen and web page in pictures.
- Boot sequence: the se/de mark fades in, gives way to the word Carousel with a thin line that
  fills while the panel gets ready, then the first screen opens from the centre like a stage
  curtain (at least 1.2 s). A step that fails stops the line in the alert colour and opens its
  state screen. Panel, under Advanced, has a "Boot screen" choice: Simple shows the line only,
  Detailed adds the IP under it, Auto picks Detailed on a development build.
- Long press on the knob (0.8 s): a line fills along the bottom of the panel while you hold,
  then the reach card opens with the address, then a QR code and the IP, for 20 s or until the
  knob moves. A press shorter than that acts as before.
- Styles for CODE (Auto, Cloud, List), CLOCK (Stone, Dial, Alternate, and what it shows besides
  the time: date, seconds, week, outside temperature) and WEATHER (Glance, Scene, Day), picked
  on each screen's own settings page, opened with the gear on its row in Running order. The
  row's picture follows the style.
- Feedback page, opened from the Feedback row on Panel: the transition between screens (Push,
  Dissolve, Wipe, Cut), what a brightness change shows (Dots, Gauge, Number, None), what a knob
  turn shows (Band, Neighbours, Dots, None) and, for each kind of request (Messages, Images,
  Commands, Coding agent), Show, Discreet (a small chip in a corner) or Off (only the change
  itself). A coding agent that needs you brings CODE on stage with the chosen transition.
- A second panel on the network answers at its own name, `carousel-xxxx.local` (the end of
  its MAC, as its setup access point); the first one up keeps `carousel.local`.
  The pages' header and the reach card show the name the panel answers at (`carousel.local`,
  or `carousel-xxxx.local` when another device holds it); `/id` and `/api/info` (`name`) give
  its own name. The Claude Code hook takes `CAROUSEL_HOST` to follow one panel among several.
- Stage, the home page (`/`), in the new look. It shows what the panel shows, as a picture of its
  frame (upright when the panel stands; black while a sign-in code is on the panel), the
  screen's name, its place among the screens playing and the next change; then the next
  screens of the running order with their posters, and the coding sessions the panel hears.
  With scripts on, the picture follows the panel every second and the lines every 5 s, only
  while the page is visible.
- Stage acts in one tap: Bring on shows a screen that is on and fits (the next one picked),
  Brightness dims or brightens the panel one step and says when night mode dims it, and Say
  something sends a message to the panel and stores it as the Running order field does. A
  refusal stays on Stage with its reason (screen off, other orientation, unknown, TEXT off or
  not fitting, flash refusing the save); a success goes back to Stage, so a reload never
  posts again. Each works with scripts off.
- Stage, with scripts on: Dimmer, Brighter and Send answer in place, without reloading the
  page. The steps move at each press, however fast the presses come; a refused press puts
  the panel's level back and says why. A turn of the knob shows on the steps within 5 s.
- Running order and Panel: with scripts on, Save waits for a change. Once a field, a box or
  the order differs from the page as loaded, a bar says "Unsaved changes" with Save and
  Discard (which puts the page back, without a request), and leaving the page asks first.
  With scripts off, Save stays in view as before.
- Running order: turn each screen on or off and move it up or down;
  the knob, the API and Home Assistant follow that list. A list leaving an orientation with
  no screen on is refused, and the page says what to turn on. Kept in NVS by English
  name (`scrList`), so later screens join on, after the known ones.
- Automatic rotation: "Change screen" every 10 s, 20 s, 30 s, 1, 2 or 5 minutes, or with the
  knob only (the default, `rotateSec`). A knob touch or a screen request holds it for 30 s, an
  overlay restarts its count, IMAGE is passed over without an image for the orientation, and
  an automatic change writes nothing to flash.
- Each screen's own settings under it in the Screens section (message, city, salary and hours,
  YouTube, GitHub and manual counts, images).
- Local API: `/api/info` gives each screen's `enabled`, the owner's `order`, `rotate_every`
  and `rotate_next_in`. The Home Assistant screen picker lists the screens that are on, in the
  owner's order, and follows every save.
- Images: upload your own GIF, still or animated, of 64×32 or 32×64 from the Images page
  (up to 24 images, 96 KB each); the page lists them with the space used and left, deletes
  them and picks the one shown, or all in turn. They stay through restarts and updates.
  Panels on the partition table before the final one need one cable flash for images.
- IMAGE screen: plays the chosen image, or every image of the panel's orientation in turn,
  with the GIF's own frame delays; says so when there is no image, none for this
  orientation, or no storage area. GIFs are decoded by the firmware's own decoder, fuzzed on
  the host.
- Image API (`docs/api.md`): `GET /api/images`, `POST /api/images` (upload, `curl -F`),
  `/api/images/delete` and `/api/images/show`, with statuses 413, 503 and 507. Home Assistant
  uploads go through a `shell_command` running `curl` (README).
- MQTT: an "Image" picker in Home Assistant ("All in turn" or one image), following the
  images on the panel; `carousel/<id>/image/set`, and `image` in the state message.
- MQTT with Home Assistant discovery (ESP-IDF's own client, no new library): set a broker on
  Panel and a "Carousel <ID>" device appears with a screen picker, a brightness
  slider, the TEXT message, a notify entity and the firmware version. State published on
  every change, last will, discovery sent again when Home Assistant restarts. Checked against
  Home Assistant 2026.10.1; a screenshot is in the README.
- Local API (`docs/api.md`): `GET /api/info`, `POST /api/screen`, `/api/brightness`,
  `/api/notify` (a message in a band across the top for a while) and `/api/text`, with a Bearer
  token shown on Panel (or `t` on the serial console). The token has its own
  secret: it opens the API only and cannot be turned into a browser session. JSON, form or query
  parameters; cross-site browser calls refused.
- Typographic punctuation typed by phones (’ “ ” – — … and non-breaking spaces) is
  folded to ASCII on the panel instead of showing `?`.
- Web pages link to the source code (AGPL-3.0 section 13), with the running version.
- Signed updates: panels check this repository's GitHub releases every night around 4 h and
  install a newer image only if its ECDSA P-256 signature matches the built-in public key;
  "Check for updates" and an auto-update switch on Panel; `u` on the serial
  console. A version that gets rolled back is never retried. Release workflow on `vX.Y.Z`
  tags: build, sign, publish `firmware.bin`, `manifest.json` and a factory image.
- Night mode: from the web page, hours and a night brightness (or off); pressing the knob
  at night brings the day brightness back for a minute.
- Factory reset: hold the knob at boot; after 3 s the panel turns red and counts down to
  10 s, letting go cancels. Erases every setting, Wi-Fi password included.
- The panel remembers its screen and brightness across restarts.
- Location: a city on the web page (found with Open-Meteo's geocoder, region shown) drives
  the weather, the time zone and the ISS distance. With no city typed, the phone's time zone
  sent with the Wi-Fi setup gives a first one ("Europe/Paris" -> Paris). The UTC offset comes
  with each weather update, daylight saving included (up to 15 min late on the change).
- Wi-Fi setup screen: a QR code to join the panel's access point with the phone camera,
  until a phone joins.
- TEXT screen: the scrolling message is set on the web page.
- Update rollback: a firmware fresh from an update runs on trial and is confirmed once
  Wi-Fi and time are up (30 s in); if it crashes or restarts before, the bootloader goes
  back to the previous one.
- Loop watchdog: a display loop stuck for 5 s restarts the board (paused during an OTA).
- Last restart reason (power, software, crash, watchdog, brownout...) on Panel
  and in `/id`, with the uptime.
- Error pages in the new look: an unknown address (404), a refused request (403), a form too
  long (413) or not received whole (400, 415) and a panel short of memory (503) answer a page
  that says what happened, what to do and links back to Stage or Images, with the same status.
  The API, the hooks, `/preview`, `/id`, the stylesheet, the posters and the image files keep
  their plain answers.
- Panel > Region: the temperature in °C or °F, the currency (€, $, £ or CHF) and a 24 h or
  12 h clock. The currency is a label: the salary is never converted.
- WEATHER (Glance, Scene and Day) and CLOCK's outside temperature in °F.
- A 12 h clock with a small AM or PM on Stone, Alternate and portrait. WEATHER's Day style
  writes its hours as "3P" or "12A".
- MONEY in the chosen currency: $ and £ before the amount, € and CHF after it, with the
  language's decimal mark. The salary field and the Running order summary name the
  currency's code.
- `/api/info` gives `temp_unit`, `currency` and `clock12`.
- Wi-Fi setup presets the region from the phone: °F where the phone's region uses it (the
  US and its territories, Liberia, Myanmar, the Bahamas, Belize, the Cayman Islands,
  Palau), the US dollar, the pound or the Swiss franc where that is the money (else the
  euro), and 12 h when the phone's clock is. It fills only what no save has stored yet,
  never a choice made on Panel. Without scripts, the panel keeps °C, euros and 24 h.
- Codex CLI, Cursor and Gemini CLI drive the CODE screen and Stage, as Claude Code does. An
  agent shows the states its hooks report: Claude Code all of them, Codex CLI and Gemini CLI
  no failed tool and no error, Cursor no "needs you".
- One install command on the Panel page (Advanced, coding agent hooks), with its token and a
  Copy button: `curl -sS --compressed --noproxy '*' "http://<panel>/install?k=<token>" | sh`. Run on a
  computer on the same network, it finds the agents installed there and writes their hooks,
  keeping every other setting and hook of theirs, with a `.carousel-backup` copy of each file
  taken before each change. Run again, it changes nothing, byte for byte. A file it changes
  is written out again in its own indent: lists on one line are spread out and, where only
  node or osascript is there (no python3), integers past 2^53 are rounded.
  `sh ~/.config/carousel/setup.sh uninstall` removes every hook it added, the agent files it
  created, and `~/.config/carousel`, panel or not; the `.carousel-backup` copies stay, and it
  names them. Codex asks once to approve the new hooks:
  `/hooks` in Codex. No npm, no Node.
- Under Advanced, the Panel page shows the hook address, whether hooks without a token are
  refused, a Renew button for the token, and how many calls were refused since the panel
  started, with what to do. When the panel cannot store a renewed token, it keeps the old
  one and the page says it was not saved.

### Compatibility
- NVS: the new key `zone` holds the place's time zone (POSIX TZ). The old `utcOffset` key is
  read at start until `zone` is found, and no longer written. A panel without a city keeps
  the build's default zone.
- The default screen list has BALL, MATRIX, RAYS and BARS off. Only panels that never saved a
  list change: a saved list keeps them as its owner left them. On a panel without a saved
  list, `screen/set BALL` and `/api/screen` with BALL are refused until BALL is turned on.
- A TEXT message saved with accents shows wrong characters after going back to an older
  firmware, until it is saved again there.
- `/id` and `/api/info`: `name` is now the panel's own name (`carousel-695c`), no longer
  `carousel`. A script that matched `carousel` there should read `device`, which stays
  `carousel`.
- Region: three new settings. A panel updated from v0.1.0 has none of them and shows °C,
  euros and 24 h, as before, until they are chosen on Panel.
- The old bash hook (`~/.claude/hooks/carousel.sh` and the `hooks` block in
  `~/.claude/settings.json`) is gone from the repository, and the panel refuses its calls: they
  carry no token. After updating, run the install command once on each computer: it replaces
  the old entries and deletes the old script. Until then the CODE screen does not follow the
  agent, and the Panel page counts the refused calls. The `/claude/<state>` routes and the
  French state names (`veille`, `reflex`, `outil`, `action`, `erreur`, `fini`, `echec`, `fin`)
  are still accepted, with the token.
- `tools/claude-bridge/` is now `tools/agent-bridge/`; links to `carousel-hook.sh` and
  `hooks-claude-code.json` no longer resolve.

## [0.1.0] - 2026-10-09

First structured release of the firmware and tools.

### Added
- English and French, on the panel and the web pages, with a language picker; tests check every
  language is complete and every panel string fits the screen.
- Firmware updates over Wi-Fi (OTA), password-protected (`pio run -e carousel-ota`).
- Zero-config panel discovery for the Claude Code hook: last known IP, then `carousel.local`,
  then a parallel scan of the local network for `GET /id`.
- `GET /id` identifies the panel and reports its firmware version.
- Serial console key `x` reboots the board without touching EN.
- Host unit tests (`pio test -e native`), simulator self-test, hook tests; CI runs them all.
- Simulator word clock in English, French kept as an option.
- `CONTRIBUTING.md`, `SECURITY.md`, `CODE_OF_CONDUCT.md`, `THIRD_PARTY_NOTICES.md`,
  issue and pull request templates.

### Changed
- Firmware split from one 4,200-line file into modules; codebase, tools and simulator in English.
- Network identity is `carousel` (mDNS, setup access point, OTA); settings stored on existing
  panels are kept.
- The simulator's animation API uses the firmware's names; the French names still work.
- Claude Code hook: `~/.claude/hooks/carousel.sh`, runs on macOS (bash 5), works in a private
  per-user directory.
- License: AGPL-3.0-only.

### Fixed
- Weather never displayed: the parser read Open-Meteo's unit strings instead of the values.
- OTA uploads failing about half the time (1 s receive timeout).
- Text overflowing the panel (HORS HEURES, PROCHE 1999 KM, several portrait labels).
- SOCIAL live dot drawn on the next row; TROUNOIR jets always leaning left; DATE label
  off-centre; possible division by zero in ARGENT.
- Hook work directory in a shared `/tmp` could be pre-created by another local user.
