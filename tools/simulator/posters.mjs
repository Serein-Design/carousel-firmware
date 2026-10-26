// SPDX-License-Identifier: AGPL-3.0-only
// Draws the web pages' screen thumbnails and writes them, as one indexed PNG,
// to firmware/src/posters.cpp. One 64x32 cell per screen of
// firmware/src/screen_table.h, in its order, then the sign-in frame, the
// locked frame and IMAGE with nothing to play, then one cell per style of
// CODE, CLOCK and WEATHER ("CLOCK:1", posters.h).
//
//   node tools/simulator/posters.mjs           writes firmware/src/posters.cpp
//   node tools/simulator/posters.mjs --check   prints the size, writes nothing
//
// Every cell is drawn by the firmware's own screen: the host test
// firmware/test/test_poster_cells runs the real draw function on a 64x32
// frame (CAT on a 32x64 one) with fixed inputs (14:32 on Monday 19 October
// 2026, a weather, sessions, system and counters fixture, a seeded random, a
// virtual clock at 60 frames a second), and writes each cell as raw RGB565 to
// firmware/.pio/poster-cells/; this script runs it first
// (pio test -d firmware -e native -f test_poster_cells, pio from PATH or
// ~/.platformio/penv/bin). The simulator is not used.
//
// Made at development time, never on the panel; the same sources give the
// same bytes, so a stale file shows in git diff. Adding a screen: add its
// line to POSTERS below and its cell to test_poster_cells.
import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { fileURLToPath } from 'node:url';
import { deflateSync, crc32 } from 'node:zlib';

// The sheet is in flash and sent to every page: at most 12 KB. Over it, the
// first cut is MAX_COLOURS (128 saves about 1.8 KB).
const BUDGET = 12288;
const MAX_COLOURS = 256;
const CELL_W = 64, CELL_H = 32;

const here = p => fileURLToPath(new URL(p, import.meta.url));
const table = readFileSync(here('../../firmware/src/screen_table.h'), 'utf8');
const i18n = readFileSync(here('../../firmware/src/i18n.cpp'), 'utf8');
const OUT = here('../../firmware/src/posters.cpp');
const FIRMWARE = here('../../firmware');
const NATIVE_CELLS = here('../../firmware/.pio/poster-cells/');

// --- The cells the firmware's own screens draw ----------------------------------
execFileSync('pio', ['test', '-d', FIRMWARE, '-e', 'native', '-f', 'test_poster_cells'], {
  stdio: ['ignore', 'ignore', 'inherit'],
  env: { ...process.env, PATH: `${process.env.PATH}:${homedir()}/.platformio/penv/bin` },
});
// One native cell as RGB888, w x h (64x32, or 32x64 for a portrait screen):
// RGB565 little-endian, as the panel's frame holds it
function nativeCell(name, w, h) {
  const raw = readFileSync(NATIVE_CELLS + name + '.565');
  if (raw.length !== w * h * 2) throw new Error(`${name}.565: ${raw.length} bytes, not ${w}x${h}`);
  const out = new Uint8Array(w * h * 3);
  for (let i = 0; i < w * h; i++) {
    const p = raw.readUInt16LE(i * 2);
    out.set([((p >> 11) & 31) * 255 / 31, ((p >> 5) & 63) * 255 / 63, (p & 31) * 255 / 31].map(Math.round), i * 3);
  }
  return out;
}

// --- The firmware's screens and English panel strings --------------------------
const english = i18n.slice(0, i18n.indexOf('.code =', i18n.indexOf('.code =') + 1));
function str(field) {
  const m = new RegExp(`\\.${field} = "([^"]*)"`).exec(english);
  if (!m) throw new Error(`i18n.cpp: no English .${field}`);
  return m[1];
}
const screens = [...table.matchAll(/^\s*SCREEN\((\w+),\s*\w+,\s*(\w+),/gm)]
  .map(([, field, orientation]) => ({ name: str(field), portrait: orientation === 'PORTRAIT' }));
if (!screens.length) throw new Error('screen_table.h: no SCREEN line found');

// --- One line per screen of screen_table.h, by English name --------------------
// The cell test_poster_cells writes for it: its name and style, "CLOCK-0".
// A screen with styles shows its default one.
const POSTERS = {
  NIMBUS: 'NIMBUS-0', CODE: 'CODE-1', DANCE: 'DANCE-0', SLEEP: 'SLEEP-0', RUN: 'RUN-0',
  LIFE: 'LIFE-0', BALL: 'BALL-0', MATRIX: 'MATRIX-0', PLASMA: 'PLASMA-0', FIRE: 'FIRE-0',
  STARS: 'STARS-0', TEXT: 'TEXT-0', RAYS: 'RAYS-0', BARS: 'BARS-0', BLACKHOLE: 'BLACKHOLE-0',
  CAT: 'CAT-0', 'ISS DEMO': 'ISS-0', CLOCK: 'CLOCK-0', WEATHER: 'WEATHER-0', DATE: 'DATE-0',
  SYSTEM: 'SYSTEM-0', MONEY: 'MONEY-0', SOCIAL: 'SOCIAL-0', IMAGE: 'IMAGE-0',   // IMAGE: a stand-in GIF
};
const FRAMES = [
  { name: '@SIGNIN', native: 'SIGNIN-0' },    // its digits drawn as dashes
  { name: '@LOCKED', native: 'LOCKED-0' },
  { name: '@NOIMAGE', native: 'NOIMAGE-0' },  // IMAGE with nothing stored
];
// One cell per style, as screen_style.h numbers them; CODE's Auto is the cloud
const STYLES = [['CODE', [1, 2]], ['CLOCK', [0, 1, 2]], ['WEATHER', [0, 1, 2]]]
  .flatMap(([screen, styles]) => styles.map(v => ({ name: `${screen}:${v}`, native: `${screen}-${v}` })));

const cells = [...screens.map(s => {
  const native = POSTERS[s.name];
  if (!native) throw new Error(`${s.name}: no poster line in posters.mjs (add one to POSTERS)`);
  return { name: s.name, portrait: s.portrait, native };
}), ...FRAMES, ...STYLES];
for (const name of Object.keys(POSTERS)) {
  if (!screens.some(s => s.name === name)) throw new Error(`${name}: in POSTERS but not in screen_table.h`);
}

// One cell as RGB888, 64x32. A portrait screen is drawn upright at half size, centred.
function render(cell) {
  if (!cell.portrait) return nativeCell(cell.native, CELL_W, CELL_H);
  const w = CELL_H, h = CELL_W;   // 32x64
  const fb = nativeCell(cell.native, w, h);
  const out = new Uint8Array(CELL_W * CELL_H * 3);
  const x0 = (CELL_W - w / 2) / 2;
  for (let y = 0; y < h / 2; y++) for (let x = 0; x < w / 2; x++) {
    for (let k = 0; k < 3; k++) {
      let sum = 0;
      for (const [dx, dy] of [[0, 0], [1, 0], [0, 1], [1, 1]]) sum += fb[((2 * y + dy) * w + 2 * x + dx) * 3 + k];
      out[(y * CELL_W + x0 + x) * 3 + k] = Math.round(sum / 4);
    }
  }
  return out;
}

// --- Indexed PNG: popularity palette, nearest colour, deflate level 9 ----------
function png(pixels, width, height) {
  const count = new Map();
  for (let i = 0; i < pixels.length; i += 3) {
    const c = (pixels[i] << 16) | (pixels[i + 1] << 8) | pixels[i + 2];
    count.set(c, (count.get(c) || 0) + 1);
  }
  const palette = [...count].sort((a, b) => b[1] - a[1] || a[0] - b[0]).slice(0, MAX_COLOURS).map(([c]) => c);
  const index = new Map(palette.map((c, i) => [c, i]));
  const nearest = c => {
    let best = 0, dist = Infinity;
    palette.forEach((p, i) => {
      const d = ((p >> 16) - (c >> 16)) ** 2 + (((p >> 8) & 255) - ((c >> 8) & 255)) ** 2 + ((p & 255) - (c & 255)) ** 2;
      if (d < dist) { dist = d; best = i; }
    });
    index.set(c, best);
    return best;
  };
  const raw = Buffer.alloc(height * (width + 1));
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {   // filter byte 0 (none), the best one for indexed rows
      const i = (y * width + x) * 3, c = (pixels[i] << 16) | (pixels[i + 1] << 8) | pixels[i + 2];
      raw[y * (width + 1) + 1 + x] = index.has(c) ? index.get(c) : nearest(c);
    }
  }
  const chunk = (type, data) => {
    const head = Buffer.alloc(8);
    head.writeUInt32BE(data.length);
    head.write(type, 4, 'latin1');
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4), data])));
    return Buffer.concat([head, data, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width); ihdr.writeUInt32BE(height, 4);
  ihdr.set([8, 3, 0, 0, 0], 8);   // 8 bits, indexed, deflate, adaptive filters, no interlace
  const plte = Buffer.from(palette.flatMap(c => [c >> 16, (c >> 8) & 255, c & 255]));
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    chunk('IHDR', ihdr), chunk('PLTE', plte),
    chunk('IDAT', deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0)),
  ]);
}

const sheet = new Uint8Array(cells.length * CELL_W * CELL_H * 3);
cells.forEach((cell, i) => sheet.set(render(cell), i * CELL_W * CELL_H * 3));
const bytes = png(sheet, CELL_W, CELL_H * cells.length);

console.log(`posters: ${cells.length} cells, ${bytes.length} / ${BUDGET} bytes`);
if (bytes.length > BUDGET) {
  console.log('  FAILED  over budget: lower MAX_COLOURS first');
  process.exit(1);
}
if (process.argv.includes('--check')) process.exit(0);

const hex = [];
for (let i = 0; i < bytes.length; i += 16) {
  hex.push('  ' + [...bytes.subarray(i, i + 16)].map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ') + ',');
}
writeFileSync(OUT, `// SPDX-License-Identifier: AGPL-3.0-only
// Generated by tools/simulator/posters.mjs; do not edit.
#include "posters.h"

const char *const POSTER_NAMES[] = {
${cells.map(c => `  "${c.name}",`).join('\n')}
};

const uint8_t POSTERS_PNG[] = {
${hex.join('\n')}
};
const size_t POSTERS_PNG_LEN = sizeof(POSTERS_PNG);
`);
