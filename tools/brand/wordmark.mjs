// SPDX-License-Identifier: AGPL-3.0-only
// The word "Carousel" in the brand's faces, as the anti-aliased bitmaps the
// panel draws at boot. Chromium (the Playwright tools/webcheck installs)
// renders the word on a canvas at SS times the panel's scale; each panel pixel
// keeps the share of its SS x SS samples the ink covers, in `levels` steps.
//
//   node tools/brand/wordmark.mjs                writes firmware/src/wordmark_data.cpp
//   ... --check                                  prints the bitmaps, writes nothing
//   ... --png <file>                             also draws them as LEDs, 6x, named
//   ... --brand <dir>                            the brand repository (default ../branding)
//
// One word to try, printed: --face chillax|switzer --weight 400-700 --cap <px>
// --track <px> --text <word> --levels 2|16, e.g.
//   node tools/brand/wordmark.mjs --face switzer --weight 600 --cap 8 --track 1 --text carousel
//
// Only the rendered word is kept, never a font: the faces' licence (ITF FFL
// 2.0, section 01) allows wordmarks and images made with them, and forbids
// redistributing the fonts. Made at development time, never on the panel; the
// same fonts give the same bytes.
import { readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const SS = 16;        // samples per pixel side
const LEVELS = 16;    // 4 bits a pixel: the panel shows 6 bits a channel, the text role's
                      // beige at full light leaves about 15 visible steps
const FACES = { chillax: 'chillax/Chillax-Variable.woff2', switzer: 'switzer/Switzer-Variable.woff2' };

// Each word: its face and weight, its lines, the cap height, the tracking and
// the gap between lines, in pixels, and its levels (2: one bit, no grey).
// The cap top and the baseline fall on pixel edges, so the C and the l stay
// crisp at both ends; the tracking keeps the letters apart where, untracked,
// their anti-aliased edges would merge into one grey run. Landscape: 60 px of
// the 64, in Chillax Semibold, one bit: the owner judged it on the panel, grey
// edges and the lighter weights read worse on the LEDs. Portrait: 28 px of the
// 32 allow a 6 px cap, the most the word can take there; in Medium with
// grey, as one bit at that size fills the a and the e in.
const CHILLAX = { face: 'chillax', gap: 0 };
const WORDS = [
  { ...CHILLAX, name: 'WIDE', weight: 600, lines: ['Carousel'], cap: 9, track: 0.4, levels: 2 },
  { ...CHILLAX, name: 'TALL', weight: 500, lines: ['Carou', 'sel'], cap: 6, track: 0.3, gap: 4, levels: LEVELS },
];

const args = process.argv.slice(2);
const option = (flag, fallback) => {
  const at = args.indexOf(flag);
  return at >= 0 ? args[at + 1] : fallback;
};
const png = option('--png', null);
const brand = option('--brand', fileURLToPath(new URL('../../../branding', import.meta.url)));
const fonts = Object.fromEntries(Object.entries(FACES).map(([face, file]) =>
  [face, readFileSync(`${brand}/fonts/${file}`).toString('base64')]));
const trial = args.includes('--face') && {
  name: 'TRIAL', face: option('--face'), weight: +option('--weight', 500), cap: +option('--cap', 9),
  track: +option('--track', 0), lines: [option('--text', 'Carousel')], gap: 0, levels: +option('--levels', LEVELS),
};
const words = trial ? [trial] : WORDS;

// Playwright lives in tools/webcheck (npm ci --prefix tools/webcheck)
const require = createRequire(new URL('../webcheck/package.json', import.meta.url));
const { chromium } = require('playwright');
const browser = await chromium.launch();
const page = await browser.newPage();

// In the page: the word's coverage, one row of 0-1 values per panel pixel
// row, from the cap top of the first line to the baseline of the last
const rasterise = async ({ fonts, ss, word, phase }) => {
  window.faces ??= {};
  if (!window.faces[word.face]) {
    const face = new FontFace(word.face, `url(data:font/woff2;base64,${fonts[word.face]})`, { weight: '100 900' });
    document.fonts.add(window.faces[word.face] = await face.load());
  }
  const probe = document.createElement('canvas').getContext('2d');
  probe.font = `${word.weight} 1000px ${word.face}`;
  const capRatio = probe.measureText('H').actualBoundingBoxAscent / 1000;
  const size = word.cap * ss / capRatio;
  const ctx0 = document.createElement('canvas').getContext('2d');
  ctx0.font = `${word.weight} ${size}px ${word.face}`;
  ctx0.letterSpacing = `${word.track * ss}px`;
  const widths = word.lines.map(l => ctx0.measureText(l).width - word.track * ss);
  const wide = Math.ceil(Math.max(...widths) / ss) + 2;
  const tall = word.lines.length * word.cap + (word.lines.length - 1) * word.gap;
  const canvas = document.createElement('canvas');
  canvas.width = wide * ss;
  canvas.height = (tall + 1) * ss;   // one spare row for anything under the baseline
  const ctx = canvas.getContext('2d');
  ctx.font = ctx0.font;
  ctx.letterSpacing = ctx0.letterSpacing;
  ctx.textBaseline = 'alphabetic';
  ctx.fillStyle = '#fff';
  word.lines.forEach((l, k) => {
    const x = (wide * ss - widths[k]) / 2;
    ctx.fillText(l, Math.round(x / ss) * ss + phase, (k + 1) * word.cap * ss + k * word.gap * ss);
  });
  const a = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
  const rows = [];
  for (let py = 0; py < tall + 1; py++) {
    const row = [];
    for (let px = 0; px < wide; px++) {
      let sum = 0;
      for (let sy = 0; sy < ss; sy++)
        for (let sx = 0; sx < ss; sx++) sum += a[((py * ss + sy) * canvas.width + px * ss + sx) * 4 + 3];
      row.push(sum / (255 * ss * ss));
    }
    rows.push(row);
  }
  return rows;
};

// A rendered word's coverage: the horizontal phase, in samples, that leaves
// the fewest half-lit pixels, so the stems land on whole LEDs where they can
const rendered = async word => {
  let cover = null, best = Infinity;
  for (let phase = 0; phase < SS; phase++) {
    const c = await page.evaluate(rasterise, { fonts, ss: SS, word, phase });
    const blur = c.flat().reduce((n, v) => n + v * (1 - v), 0);
    if (blur < best) { best = blur; cover = c; }
  }
  return cover;
};

const results = [];
for (const word of words) {
  const cover = await rendered(word);
  // The densest pixel at full light: at a 6 px cap no stroke covers a whole
  // LED, and the portrait word would otherwise never reach the text role.
  // One bit: lit where the ink covers half the densest pixel.
  const top = Math.max(...cover.flat());
  const level = word.levels === 2
    ? c => (c / top >= 0.5 ? LEVELS - 1 : 0)
    : c => Math.round(c / top * (LEVELS - 1));
  let rows = cover.map(r => r.map(level));
  // Trimmed to the ink: the bitmap's top is the first lit row, its left the first lit column
  const lit = r => r.some(v => v);
  while (rows.length && !lit(rows[0])) rows.shift();
  while (rows.length && !lit(rows[rows.length - 1])) rows.pop();
  const cols = rows[0].map((_, x) => rows.some(r => r[x]));
  const x0 = cols.indexOf(true), x1 = cols.lastIndexOf(true);
  rows = rows.map(r => r.slice(x0, x1 + 1));
  results.push({ ...word, rows, w: x1 - x0 + 1, h: rows.length });
}

const shades = ' .:-=+*#%@';
for (const { name, rows, w, h } of results) {
  console.log(rows.map(r => r.map(v => shades[Math.ceil(v * (shades.length - 1) / (LEVELS - 1))]).join('')).join('\n'));
  const used = new Set(rows.flat()).size;
  console.log(`${name}: ${w}x${h}, ${used} levels, ${Math.ceil(w / 2) * h} bytes`);
}

// The LEDs as the panel would show them, the text role's beige at each
// level, each word under its name
if (png) {
  const url = await page.evaluate(({ words, levels }) => {
    const S = 6, pad = 2, caption = 18;
    const canvas = document.createElement('canvas');
    canvas.width = Math.max(64, ...words.map(w => w.w + 2 * pad)) * S;
    canvas.height = words.reduce((n, w) => n + (w.h + 2 * pad) * S + caption, 0);
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    let top = 0;
    words.forEach(w => {
      ctx.fillStyle = '#8b9797';
      ctx.font = '13px sans-serif';
      ctx.fillText(`${w.name}  (${w.w}x${w.h})`, 8, top + 14);
      top += caption;
      w.rows.forEach((r, y) => r.forEach((v, x) => {
        const l = v / (levels - 1);
        ctx.fillStyle = v ? `rgb(${232 * l},${201 * l},${160 * l})` : '#111';
        ctx.beginPath();
        ctx.arc((x + pad + 0.5) * S, top + (y + pad + 0.5) * S, S * 0.4, 0, 2 * Math.PI);
        ctx.fill();
      }));
      top += (w.h + 2 * pad) * S;
    });
    return canvas.toDataURL('image/png');
  }, { words: results, levels: LEVELS });
  writeFileSync(png, Buffer.from(url.split(',')[1], 'base64'));
  console.log(`wrote ${png}`);
}
await browser.close();
if (args.includes('--check') || trial) process.exit(0);

// Rows packed two pixels a byte, the left one in the high nibble
const table = ({ name, rows, w }) => {
  const lines = rows.map(r => '  ' + Array.from({ length: Math.ceil(w / 2) },
    (_, b) => '0x' + ((r[2 * b] << 4) | (r[2 * b + 1] ?? 0)).toString(16).toUpperCase().padStart(2, '0')).join(', ') + ',');
  return `static const uint8_t ${name}_LEVELS[] = {\n${lines.join('\n')}\n};`;
};
writeFileSync(fileURLToPath(new URL('../../firmware/src/wordmark_data.cpp', import.meta.url)), `// SPDX-License-Identifier: AGPL-3.0-only
// Generated by tools/brand/wordmark.mjs from the brand's title face: run it
// again, never edit this file. "Carousel" in Chillax, 4 bits a pixel, one row
// per line: ${results.map(r => `${r.name} ${r.w}x${r.h}${r.levels === 2 ? ' (one bit)' : ''}`).join(', ')}.
#include "wordmark.h"

${results.map(table).join('\n\n')}

${results.map(r => `const Wordmark WORDMARK_${r.name} = {${r.name}_LEVELS, ${r.w}, ${r.h}};`).join('\n')}
`);
console.log('wrote firmware/src/wordmark_data.cpp');
