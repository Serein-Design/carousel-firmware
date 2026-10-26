# Local API and MQTT

The panel answers HTTP on your network, and speaks MQTT when a broker is set. Everything the knob does, and a few things it
cannot, is available to scripts and home automation.

## Token

Every call carries the panel's token:

```
Authorization: Bearer <token>
```

Find it on the Panel page, under Advanced (sign in with the code the panel shows), or
type `t` then Enter on the serial console. "Sign out every other browser" on the Panel page
replaces it; so does a factory reset.

The token only opens the API. It cannot sign a browser in, so a token copied into a script or
a home automation file cannot change the panel's Wi-Fi or other settings.

Calls from a web page of another site are refused, so a token only works from scripts,
apps and home automation, not from a page you happen to visit.

The browser routes `/show`, `/brightness`, `/text` and `/wifi` are the panel's pages, not API:
they take the session cookie only and refuse the token, so scripts use `/api/...`.

## Parameters

`POST` calls take their parameters as JSON (`Content-Type: application/json`), as a form,
or in the query string: use whichever your tool makes easiest. Text is UTF-8 and may contain
accents and typographic punctuation. What the panel keeps depends on the call: a message to
`/api/notify` keeps its accents; the TEXT message (`/api/text`, MQTT `text/set`) keeps the
French letters and drops the other accents ("ñ" shows as "n"); an image's stored name drops
every accent ("été.gif" becomes `ete.gif`).

Answers are JSON: `{"ok":true}` on success, `{"error":"..."}` with an error status otherwise.

What the panel shows for a call is the owner's choice on the Feedback page (Panel), one per
kind: Messages (`/api/notify`), Images (`/api/images/show`, an upload with `show=1`),
Commands (`/api/screen`, `/api/brightness`) and the coding agent. Show is the kind's own way,
Discreet cuts it to a small chip in a corner, and Off shows nothing but the change itself. The
change is made and the answer is the same in every case.

| Status | Meaning |
| --- | --- |
| 400 | A parameter is missing; an upload that is not a GIF, not 64×32 or 32×64, or not sent as `multipart/form-data` |
| 401 | Token missing or wrong |
| 403 | Request from another web site, or to a host name the panel does not have |
| 404 | Unknown screen or image; a screen drawn only for the other orientation |
| 409 | The screen is off in the owner's list |
| 413 | Upload over 96 KB |
| 500 | The panel could not do it: a flash or settings write refused, or out of memory |
| 503 | No storage area for images (a panel on an older partition table), or the panel is locked by an unfinished factory reset |
| 507 | No room left for the image, or 24 images already stored |

A panel locked by an unfinished factory reset answers 503
`{"error":"panel locked: factory reset unfinished"}` to `/api/text` and to every image call,
and changes nothing; it unlocks once the reset completes (see [SECURITY.md](../SECURITY.md)).

## Endpoints

### `GET /api/info`

State of the panel, the list of screens and the rotation (shortened here: `screens` and
`order` name every screen of the firmware).

```json
{"device":"carousel","name":"carousel-695c","version":"v0.2.0","screen":"CLOCK",
 "orientation":"landscape",
 "brightness":3,"brightness_levels":6,
 "temp_unit":"C","currency":"EUR","clock12":false,
 "screens":[{"name":"NIMBUS","available":true,"enabled":true},
            {"name":"CAT","available":false,"enabled":true},
            {"name":"CLOCK","available":true,"enabled":true},
            {"name":"WEATHER","available":true,"enabled":false}],
 "order":["CLOCK","WEATHER","NIMBUS","CAT"],
 "rotate_every":60,"rotate_next_in":42}
```

| Field | |
| --- | --- |
| `name` | The panel's own name, the same on every network: it answers at `http://carousel.local`, or at `http://carousel-695c.local` when another device holds `carousel.local` |
| `temp_unit` | `"C"` or `"F"`: the unit of the temperatures the panel shows (Region on the Panel page) |
| `currency` | `"EUR"`, `"USD"`, `"GBP"` or `"CHF"`: the money MONEY counts in |
| `clock12` | `true` for a 12 h clock with AM/PM, `false` for 24 h |
| `screens` | Every screen, in the firmware's order |
| `available` | False for a screen drawn for the other orientation (the tilt switch picks the gallery) |
| `enabled` | False for a screen the owner turned off on the Running order page: the knob, the rotation, the API and Home Assistant skip it |
| `order` | Every screen's name, in the owner's order, off ones included |
| `rotate_every` | Seconds between automatic changes: `10`, `20`, `30`, `60`, `120` or `300`; `0` when the screen changes with the knob only |
| `rotate_next_in` | Seconds before the next automatic change, `null` when the rotation is off or no other screen can come next in this orientation. A touch on the knob or a screen request holds it for 30 s; while an overlay covers the screen (the sign-in code, an update) it shows the full interval, counted again once the screen is back |

`GET /id` needs no token and answers the same `device`, `name` and `version`, plus `mac`,
`reset` (the last restart's cause) and `uptime` in seconds: scripts that cannot use mDNS
find a panel by asking each address of the network for it. With `?c=<nonce>` (16 to 32
hexadecimal characters) it adds `proof`, the first 32 hexadecimal characters of HMAC-SHA256
keyed with the hook token's 32 characters over the nonce, a colon and the panel's own IPv4
address as the request reached it (`<nonce>:192.168.1.50`). The agent hook checks it with
the address it connected to before it sends the token there, so a device that copies the
panel's name gets nothing, and neither does one that passes the nonce on to the panel. At
most 4 proofs a second are made, one more every 250 ms: beyond, `/id` answers without
`proof`. Any other `c` is ignored. Without `c`, `/id` is unchanged.

### `POST /api/screen`

| Parameter | |
| --- | --- |
| `screen` | A name from `/api/info`, any case |

404 `{"error":"no such screen in this orientation, see /api/info"}` for an unknown name or a
screen drawn only for the other orientation; 409 `{"error":"screen is off, see /api/info"}`
for a screen the owner turned off (tilting the panel would not show it either). Nothing
changes in both cases. A screen shown this way holds the rotation for 30 s.

### `POST /api/brightness`

| Parameter | |
| --- | --- |
| `level` | `0` (dimmest) to `brightness_levels - 1` |

Night mode, when set, still dims the panel at night.

### `POST /api/notify`

A message in a band across the top of the screen, which keeps running below it: the band
slides down with the sender's icon, scrolls the text twice (or for `seconds`, when that is
longer), then slides away. Accents are kept. With Messages set to Discreet on the Feedback
page, the panel shows a small chip in a corner instead of the band; set to Off, it shows
nothing, and the call still answers `{"ok":true}`.

| Parameter | |
| --- | --- |
| `text` | The message |
| `color` | `#rrggbb`; without it, the panel's own text colour |
| `seconds` | 1 to 600, 10 by default: the least time on show |

### `POST /api/text`

Sets the message of the TEXT screen (saved, like on the Running order page and Stage's Say something). 500
`{"error":"message not saved"}` when the panel refuses the write.

| Parameter | |
| --- | --- |
| `text` | The message |

The message holds 95 bytes. A plain character takes one, and a French accented letter (é, è,
à, ç, œ and the others, capitals included) takes two. Other accented letters lose their
accent, typographic quotes, dashes and the ellipsis become `'`, `"`, `-` and `...`, and any
other symbol becomes `?`. If a message is too long for its accents, the last accents lose
their mark first, so every character is kept. Past 95 characters, the message is cut.

### `GET /api/images`

The stored images in alphabetical order, the space figures in bytes and the image chosen.

```json
{"images":[{"name":"sunset.gif","orientation":"landscape","size":18342}],
 "count":1,"max":24,"used":18342,"left":266240,"total":284582,"choice":"all"}
```

`left` is exactly what the next upload may take; a part of the area stays free so a delete
always succeeds. `choice` is a stored name, or `all` when the IMAGE screen shows every image
in turn. 503 `{"error":"no storage area"}` on a panel without the storage area.

### `POST /api/images`

Uploads one GIF, still or animated, as `multipart/form-data`. Only the first file part is
stored; a body in any other format is refused with 400.

- Exactly 64×32 (landscape) or 32×64 (portrait) pixels, at most 96 KB.
- Refused before a byte is written when the token is wrong or the announced size is over
  96 KB: the panel answers, then closes the connection; curl shows the answer.
- At most 24 images, within the space `left`.

The stored name comes from the file name: letters, digits, `-`, `_` and `.` only, accents
folded ("été.gif" becomes `ete.gif`), any other character becomes `-`, no leading dot,
always ending in `.gif`, cut at 36 characters before the `.gif` (40 in all), `image.gif` when
nothing is left. A name never becomes a path. An upload whose stored name matches an image
already there replaces it once the new file is complete and valid, and the answer says
`"replaced":true`; the new file needs room of its own while it is written.

| Parameter | |
| --- | --- |
| `file` | The GIF (the first file part is the image, whatever its field name) |
| `show` | `1`, in the query string: switch to the IMAGE screen and show the image first, when it is drawn for the panel's current orientation and IMAGE is on. Without it the screen does not change |

With IMAGE off in the owner's list, an upload is stored as usual and the answer is the same,
but the screen does not change, `show=1` or not.

```json
{"ok":true,"name":"sunset.gif","replaced":false,"orientation":"landscape","size":18342}
```

| Refusal | Status | Answer |
| --- | --- | --- |
| Not a GIF | 400 | `{"error":"not a GIF: PNG"}` (PNG, JPEG or WebP named when recognised) |
| Wrong size | 400 | `{"error":"wrong size: 128x64, the panel takes 64x32 or 32x64"}` |
| Over 96 KB | 413 | `{"error":"over 96 KB"}` |
| No room | 507 | `{"error":"no room","needed":40960,"left":8192}` |
| 24 images | 507 | `{"error":"24 images already: delete one first"}` |
| No storage area | 503 | `{"error":"no storage area"}` |
| No file part, or not `multipart/form-data` | 400 | `{"error":"send one GIF as multipart/form-data: curl -F file=@image.gif"}` |
| Write failed, or upload cut short | 500 | `{"error":"upload failed: nothing kept"}` |

A refused or interrupted upload leaves nothing behind.

### `POST /api/images/delete`

| Parameter | |
| --- | --- |
| `name` | A stored name, as `/api/images` lists it |

404 when no image has that name (nothing changes); 500 when the flash refuses the delete.
Deleting the chosen image returns the choice to all in turn.

### `POST /api/images/show`

Saves the choice and switches to the IMAGE screen.

| Parameter | |
| --- | --- |
| `name` | A stored name, or `all` to show every image in turn |

404 for an unknown name; 500 when the choice cannot be saved. A chosen image drawn for the
other orientation is not shown while the panel is turned that way: the images of the
current orientation play in turn instead. With IMAGE off in the owner's list, the choice is
saved and the answer is `{"ok":true}`, but the screen does not change.

## Hooks

Coding agents (Claude Code, Codex CLI, Cursor, Gemini CLI) tell the panel what a session is
doing, and the CODE screen and Stage show it. The install command on the Panel page sets this
up; this section is the contract it relies on. Hook calls use their own token, not the API
token, and they store nothing on the panel.

| Route | Use |
| --- | --- |
| `GET /agent/<state>?s=<session>&n=<name>&t=<time>` | The hook script's call: `s` the session id (required), `n` the project folder name, `t` a number that grows with each event of a session (events older than the last one are ignored) |
| `POST /agent/<state>` | An agent's http hook: the JSON payload is the body, the panel reads `session_id` and `cwd` from its first 1.5 KB and ignores the rest |
| `/claude/<state>` | The same two calls under the first route's name, kept for installed hooks |

The state is in the path, in English or in French, with the same effect:

| English | `idle` | `thinking` | `tool` | `attention` | `error` | `done` | `failed` | `end` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| French | `veille` | `reflex` | `outil` | `action` | `erreur` | `fini` | `echec` | `fin` |

`attention` says the agent needs you: it brings CODE on stage and holds the rotation, and
writes no screen, brightness or setting. `end` closes the session; a session that never says
`end` leaves the list after 15 minutes.

Every call carries the hook token in a header, never in the address:

```text
X-Hook-Token: <token>
```

| Answer | When |
| --- | --- |
| 200, empty body | Applied |
| 400 | Unknown state (`unknown state`), or no session id |
| 401 | No token, or a wrong one (`token`), before any body byte is read. Nothing changes, and the refusal is counted for the Panel page |
| 403 | A browser page of another site (`Origin`, `Sec-Fetch-Site`), checked before the token |

The token is a 128-bit secret of its own, created at the panel's first start. "Sign out every
other browser" keeps it; Renew on the Panel page (Advanced, coding agent hooks) and a factory
reset replace it, and the computers set up with the old one are refused until they run the
new command. "Accept hooks without a token" on the same fold, off by default, makes the panel
apply every call with or without a token: for development only.

```sh
P=http://carousel.local     # or the panel's IP
H='X-Hook-Token: <token>'

curl -H "$H" "$P/agent/attention?s=test&n=essai"        # CODE turns orange
curl -H "$H" "$P/claude/action?s=test&n=essai"          # the same, with the French name
curl -H "$H" "$P/agent/end?s=test"                      # closes the test session
curl "$P/agent/attention?s=test"                        # 401, nothing changes
```

An agent's own http hook form works too, with the header in its configuration. It posts the
whole payload, tool output included, and the agent waits for the answer, which is why the
install command sets up a script instead:

```json
{"type": "http", "url": "http://<panel IP>/agent/thinking", "headers": {"X-Hook-Token": "<token>"}}
```

`GET /install?k=<token>` serves the installer that the Panel page's command pipes into `sh`:

```sh
curl -sS --compressed --noproxy '*' "http://<panel IP>/install?k=<token>" | sh
```

The answer is a POSIX `sh` script, gzip-compressed, with the token, the panel's IP and its
name in front (`Cache-Control: no-store`). `--noproxy '*'` keeps a proxy set on the computer
(`http_proxy`) from receiving the token in the address. It needs `--compressed`: a request that does not
accept gzip gets 406, and a wrong or missing `k`, an unknown host or a request from another
site gets 403, each with a short script that prints the reason on stderr and exits 1, so the
pipe never runs anything else. 503 means the panel is locked after an unfinished factory
reset. The script runs nothing unless it arrived whole. To remove its hooks and its folder
(the `.carousel-backup` copies stay): `sh ~/.config/carousel/setup.sh uninstall`.

## Examples

```sh
P=http://carousel.local     # or the panel's IP
T=<token>

curl -H "Authorization: Bearer $T" $P/api/info
curl -H "Authorization: Bearer $T" -d screen=clock $P/api/screen
curl -H "Authorization: Bearer $T" -H 'Content-Type: application/json' \
     -d '{"text":"Washing machine done","color":"#00ff88","seconds":30}' $P/api/notify

curl -H "Authorization: Bearer $T" -F file=@sunset.gif "$P/api/images?show=1"
curl -H "Authorization: Bearer $T" $P/api/images
curl -H "Authorization: Bearer $T" -d name=sunset.gif $P/api/images/show
curl -H "Authorization: Bearer $T" -d name=all $P/api/images/show
curl -H "Authorization: Bearer $T" -d name=sunset.gif $P/api/images/delete
```

Home Assistant, in `configuration.yaml` (token in `secrets.yaml`):

```yaml
rest_command:
  carousel_notify:
    url: http://carousel.local/api/notify
    method: post
    headers:
      Authorization: !secret carousel_token
    content_type: application/json
    payload: '{"text":"{{ text }}","color":"{{ color | default("#ffffff") }}","seconds":{{ seconds | default(10) }}}'
```

with `carousel_token: "Bearer <token>"` in `secrets.yaml`, then call the
`rest_command.carousel_notify` action with `text`, and optionally `color` and `seconds`.

`rest_command` sends text only: an image upload from Home Assistant is a `shell_command`
running `curl`, shown in the [README](../README.md#images-from-home-assistant).

## MQTT and Home Assistant

Set the broker (host, port, optional user and password) on the Panel page (Advanced, Home
Assistant). The panel connects with the client id `carousel_<id>`, where `<id>` is the end of its MAC address, the
same 4 characters as its setup network `CAROUSEL-<ID>`.

![The Carousel device in Home Assistant](images/home-assistant.png)

Home Assistant finds it by itself through
[MQTT discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery): a
"Carousel <ID>" device with a screen picker (the screens that are on, in the owner's order,
sent again whenever the list is saved), an image picker, a brightness slider, the TEXT
message, a notify entity and the firmware version. Discovery is sent again when Home Assistant
restarts, and the image picker's within 5 s of an image being added, replaced or deleted.

| Topic | Direction | Payload |
| --- | --- | --- |
| `carousel/<id>/status` | panel → | `online`, or `offline` (last will); retained |
| `carousel/<id>/state` | panel → | `{"screen":"CLOCK","brightness":3,"text":"...","version":"v0.2.0","image":"All in turn"}`, on every change; retained. `image`: the chosen image's name, or `All in turn` |
| `carousel/<id>/screen/set` | → panel | a screen name |
| `carousel/<id>/brightness/set` | → panel | a level |
| `carousel/<id>/text/set` | → panel | the TEXT message |
| `carousel/<id>/image/set` | → panel | a stored image name, or `All in turn`; switches to the IMAGE screen when it is on |
| `carousel/<id>/notify` | → panel | plain text, or `{"text":"...","color":"#rrggbb","seconds":10}` |

```sh
mosquitto_pub -h broker -t carousel/695c/notify -m "Front door open"
mosquitto_pub -h broker -t carousel/695c/screen/set -m weather
mosquitto_pub -h broker -t carousel/695c/image/set -m sunset.gif
```

Screens drawn for the other orientation are ignored until the panel is tilted; a screen
the owner turned off is ignored on `screen/set` (the picker does not offer it). An image name
the panel does not have is ignored. MQTT only chooses among the stored images: adding,
replacing and deleting go through the HTTP API, with the token. Image names are visible to
anyone who can read the broker.
