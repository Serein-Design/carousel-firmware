// SPDX-License-Identifier: AGPL-3.0-only
// Screenshots for docs/tour.md and the README, written to docs/images/:
//   - the web pages (English, fixtures only: fake networks, fake sessions),
//     from the HTML files the host test test_web_pages writes, served as the
//     panel would, in WebKit: a phone at 390 px (device scale 2, first
//     screen; Running order also full length) and Stage at 1280 px;
//   - every frame of the poster sheet (firmware/src/posters.cpp) drawn as
//     LEDs, one PNG per screen, plus one contact sheet.
//
//   pio test -d firmware -e native -f test_web_pages     (writes the pages)
//   node tools/docs/shots.mjs [folder]                   (default firmware/.pio/webcheck)
//
// Once per clone: npm ci --prefix tools/webcheck, then, if the browser is
// missing, npx --prefix tools/webcheck playwright install webkit.
// Regenerate the screen images whenever the posters change (after
// node tools/simulator/posters.mjs) and the page images whenever a page
// changes. The page PNGs are run through pngquant, else ImageMagick, when installed. Development
// only: nothing here reaches the firmware.
import { execFileSync } from 'node:child_process';
import { mkdirSync, readFileSync, statSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { crc32, deflateSync, gunzipSync, inflateSync } from 'node:zlib';

const HERE = dirname(fileURLToPath(import.meta.url));
const OUT = resolve(HERE, '../../docs/images');
const dir = resolve(process.argv[2] || join(HERE, '../../firmware/.pio/webcheck'));
const BUDGET = 2 * 1024 * 1024;

// --- Screens as LEDs ----------------------------------------------------------
const CELL_W = 64, CELL_H = 32;

function readPosters() {
  const src = readFileSync(join(HERE, '../../firmware/src/posters.cpp'), 'utf8');
  const names = [...src.match(/POSTER_NAMES\[\] = \{([^}]*)\}/)[1].matchAll(/"([^"]*)"/g)].map((m) => m[1]);
  const bytes = Buffer.from([...src.match(/POSTERS_PNG\[\] = \{([^}]*)\}/)[1].matchAll(/0x([0-9a-f]{2})/g)]
    .map((m) => parseInt(m[1], 16)));
  // The sheet is an 8-bit indexed PNG, filter 0 on every row (posters.mjs)
  let pos = 8, plte = null;
  const idat = [];
  while (pos < bytes.length) {
    const len = bytes.readUInt32BE(pos), type = bytes.toString('latin1', pos + 4, pos + 8);
    const data = bytes.subarray(pos + 8, pos + 8 + len);
    if (type === 'PLTE') plte = data;
    if (type === 'IDAT') idat.push(data);
    pos += 12 + len;
  }
  const w = bytes.readUInt32BE(16), h = bytes.readUInt32BE(20);
  const raw = inflateSync(Buffer.concat(idat));
  if (w !== CELL_W || h !== CELL_H * names.length) throw new Error('posters.cpp: unexpected sheet size');
  return names.map((name, n) => {
    const rgb = Buffer.alloc(CELL_W * CELL_H * 3);
    for (let y = 0; y < CELL_H; y++) for (let x = 0; x < CELL_W; x++) {
      const i = raw[(n * CELL_H + y) * (w + 1) + 1 + x];
      plte.copy(rgb, (y * CELL_W + x) * 3, i * 3, i * 3 + 3);
    }
    return { name, rgb };
  });
}

function pngRgb(width, height, rgb) {
  const raw = Buffer.alloc(height * (width * 3 + 1));
  for (let y = 0; y < height; y++) rgb.copy(raw, y * (width * 3 + 1) + 1, y * width * 3, (y + 1) * width * 3);
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
  ihdr.set([8, 2, 0, 0, 0], 8);
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
}

// Round dots on near-black, `px` pixels per LED; draws into canvas at (ox, oy)
const BACKGROUND = [10, 10, 12];
function drawLeds(cell, px, canvas, canvasW, ox, oy) {
  const r = px * 0.42, c = (px - 1) / 2;
  for (let y = 0; y < CELL_H; y++) for (let x = 0; x < CELL_W; x++) {
    const led = cell.subarray((y * CELL_W + x) * 3, (y * CELL_W + x) * 3 + 3);
    for (let dy = 0; dy < px; dy++) for (let dx = 0; dx < px; dx++) {
      const d = Math.hypot(dx - c, dy - c);
      const a = Math.max(0, Math.min(1, r + 0.5 - d));   // soft edge, one pixel
      const o = ((oy + y * px + dy) * canvasW + ox + x * px + dx) * 3;
      for (let k = 0; k < 3; k++) canvas[o + k] = Math.round(BACKGROUND[k] + (led[k] - BACKGROUND[k]) * a);
    }
  }
}
function fill(canvas, colour) { for (let i = 0; i < canvas.length; i += 3) canvas.set(colour, i); }

function screens() {
  const files = [];
  const posters = readPosters();
  const slug = (n) => n.toLowerCase().replace(/^@/, '').replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
  for (const p of posters) {
    const px = 6, w = CELL_W * px, h = CELL_H * px;
    const canvas = Buffer.alloc(w * h * 3);
    fill(canvas, BACKGROUND);
    drawLeds(p.rgb, px, canvas, w, 0, 0);
    files.push([`screen-${slug(p.name)}.png`, pngRgb(w, h, canvas)]);
  }
  // Contact sheet of the screens and the panel's frames (not each style of a
  // screen, which has its own picture): 5 columns, 4 px per LED, a gap and a
  // margin of 12 px
  const sheet = posters.filter((p) => !p.name.includes(':'));
  const px = 4, cw = CELL_W * px, ch = CELL_H * px, gap = 12, cols = 5;
  const rows = Math.ceil(sheet.length / cols);
  const w = gap + cols * (cw + gap), h = gap + rows * (ch + gap);
  const canvas = Buffer.alloc(w * h * 3);
  fill(canvas, [24, 24, 28]);
  sheet.forEach((p, i) => {
    const ox = gap + (i % cols) * (cw + gap), oy = gap + Math.floor(i / cols) * (ch + gap);
    for (let y = 0; y < ch; y++) for (let x = 0; x < cw; x++) canvas.set(BACKGROUND, ((oy + y) * w + ox + x) * 3);
    drawLeds(p.rgb, px, canvas, w, ox, oy);
  });
  files.push(['screens.png', pngRgb(w, h, canvas)]);
  return files;
}

// --- Web pages ----------------------------------------------------------------
// The pages' own server, as in tools/webcheck/webcheck.mjs (which runs its
// checks on import, so the minimum is copied here)
const FIXTURES = {
  '/images/f': ['image.gif', 'image/gif'],
  '/api/info': ['info.json', 'application/json'],
};
// A 24-bit BMP of one 64 x 32 cell, as /preview sends it
function bmp(rgb) {
  const row = CELL_W * 3, head = Buffer.alloc(54);
  head.write('BM'); head.writeUInt32LE(54 + row * CELL_H, 2); head.writeUInt32LE(54, 10);
  head.writeUInt32LE(40, 14); head.writeInt32LE(CELL_W, 18); head.writeInt32LE(CELL_H, 22);
  head.writeUInt16LE(1, 26); head.writeUInt16LE(24, 28);
  const px = Buffer.alloc(row * CELL_H);
  for (let y = 0; y < CELL_H; y++) for (let x = 0; x < CELL_W; x++) {
    const i = (y * CELL_W + x) * 3, o = (CELL_H - 1 - y) * row + x * 3;
    px[o] = rgb[i + 2]; px[o + 1] = rgb[i + 1]; px[o + 2] = rgb[i];
  }
  return Buffer.concat([head, px]);
}

function serve(files) {
  const read = (name) => { try { return readFileSync(join(dir, name)); } catch { return null; } };
  const server = createServer((req, res) => {
    const path = new URL(req.url, 'http://127.0.0.1').pathname;
    const send = (code, type, body) => {
      res.writeHead(code, type ? { 'Content-Type': type } : {});
      res.end(body);
    };
    let m;
    if ((m = path.match(/^\/s\/[^/]+\/(c\.css|p\.png)$/))) {
      const gz = m[1] === 'c.css' ? read('c.css.gz') : null;
      const body = m[1] === 'c.css' ? gz && gunzipSync(gz) : read('p.png');
      return body ? send(200, m[1] === 'c.css' ? 'text/css' : 'image/png', body) : send(404, 'text/plain', '404');
    }
    if (path === '/preview') {   // the real frame of the screen on stage, not the test pattern
      const on = JSON.parse(read('info.json')).screen;
      const cell = readPosters().find((p) => p.name === on);
      return cell ? send(200, 'image/bmp', bmp(cell.rgb)) : send(404, 'text/plain', '404');
    }
    if (FIXTURES[path]) {
      const body = read(FIXTURES[path][0]);
      return body ? send(200, FIXTURES[path][1], body) : send(404, 'text/plain', '404');
    }
    const file = path.slice(1);
    if (files.includes(file)) return send(200, 'text/html; charset=utf-8', read(file));
    return send(404, 'text/plain', '404');
  });
  return new Promise((ok) => server.listen(0, '127.0.0.1', () => ok(server)));
}

// name, page, shot: phone (first screen), full (phone, whole page), desktop
const PAGES = [
  ['stage', 'stage-rotation-en.html', 'phone'],
  ['stage-desktop', 'stage-rotation-en.html', 'desktop'],
  ['order', 'order-default-en.html', 'phone'],
  ['order-full', 'order-default-en.html', 'full'],
  ['images', 'images-full-en.html', 'phone'],
  ['panel', 'panel-default-en.html', 'phone'],
  ['feedback', 'feedback-default-en.html', 'phone'],
  ['screen-clock', 'screen-clock-default-en.html', 'phone'],
  ['wifi-setup', 'wifi-first-en.html', 'phone'],
  ['signin', 'signin-default-en.html', 'phone'],
  ['error', 'error-404-en.html', 'phone'],
];

async function pages() {
  let webkit;
  try {
    const require = createRequire(join(HERE, '../webcheck/package.json'));
    const mod = await import(pathToFileURL(require.resolve('playwright')).href);
    webkit = mod.webkit || mod.default.webkit;
  } catch (e) {
    console.error(`shots: ${e.message}\nRun once: npm ci --prefix tools/webcheck`);
    process.exit(2);
  }
  const server = await serve(PAGES.map((p) => p[1]));
  const base = `http://127.0.0.1:${server.address().port}/`;
  const browser = await webkit.launch();
  const out = [];
  for (const [name, file, shot] of PAGES) {
    const desktop = shot === 'desktop';
    const ctx = await browser.newContext({
      viewport: desktop ? { width: 1280, height: 800 } : { width: 390, height: 844 },
      deviceScaleFactor: desktop ? 1 : 2,
      locale: 'en',
      reducedMotion: 'reduce',
    });
    const page = await ctx.newPage();
    await page.goto(base + file, { waitUntil: 'load' });
    await page.waitForTimeout(600);   // the preview image and the poster thumbnails
    out.push([`web-${name}.png`, await page.screenshot({ fullPage: shot === 'full' })]);
    await ctx.close();
  }
  await browser.close();
  server.close();
  return out;
}

// --- Write, optimise, count -----------------------------------------------------
mkdirSync(OUT, { recursive: true });
const made = [...screens(), ...(await pages())];
const has = (cmd) => { try { execFileSync(cmd, ['--version'], { stdio: 'ignore' }); return true; } catch { return false; } };
const quant = has('pngquant') ? 'pngquant' : has('magick') ? 'magick' : null;
for (const [name, bytes] of made) {
  const path = join(OUT, name);
  writeFileSync(path, bytes);
  if (name.startsWith('web-') && quant === 'pngquant') {
    execFileSync('pngquant', ['--force', '--skip-if-larger', '--quality=70-95', '--output', path, path]);
  } else if (name.startsWith('web-') && quant === 'magick') {
    execFileSync('magick', [path, '-colors', '128', `PNG8:${path}`]);   // pngquant absent: ImageMagick's palette
  }
}
const sizes = made.map(([name]) => statSync(join(OUT, name)).size);
const total = sizes.reduce((a, b) => a + b, 0);
console.log(`shots: ${sizes.length} images written to docs/images, ${(total / 1024).toFixed(0)} KB${quant ? ` (${quant})` : ' (not optimised: no pngquant or magick)'}`);
if (total > BUDGET) {
  console.error(`shots: over the ${BUDGET / 1024 / 1024} MB budget`);
  process.exit(1);
}
