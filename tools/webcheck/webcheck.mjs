// SPDX-License-Identifier: AGPL-3.0-only
// Browser checks of the web pages, on the HTML files the host test
// test_web_pages writes, served over HTTP on 127.0.0.1 as the panel would
// (the stylesheet at /s/<version>/c.css, fixtures for the rest): each file
// at 360 and 1280 px in headless Chromium, four pages at a time, then again
// in WebKit (Safari's engine, the one every iPhone browser uses) for
// overflow and targets, which its native controls size their own way,
// failing on
//   - overflow: the page wider than the window (a long name must wrap);
//   - targets:  an enabled button, input, select, summary, checkbox label
//               or drag handle (.dg) under 44 x 44 px;
//   - contrast: text under WCAG AA (4.5:1, 3:1 for large text);
//   - axe:      any axe-core violation of impact serious or critical;
//   - folded:   a settings field inside a <details> that a value, set or
//               typed, can make invalid (the browser's message cannot show
//               in a folded block, so the save would not post, without a
//               word);
//   - structure: one main, one h1, every button in a form (or form=), every
//               POST form with an action, every link with an address, the
//               skip link first and aimed at <main id=m>;
//   - footer:   on a page shorter than the window, the footer's bottom
//               above the window's bottom edge (it must sit at the edge,
//               not rise and leave a gap under it);
//   - focus:    a focus stop, reached with Tab, without a 2 px ring;
//   - motion:   an animation or transition running under reduced motion;
//   - no-js:    overflow, targets and structure again with scripts off;
//   - fold:     Stage pages: the preview, as the fixture really is (32 x 64
//               on the portrait pages), not loaded, or it, the screen's name
//               or the live line ending below the first window;
//   - live:     Stage pages: the preview not reloaded within 1.5 s, or
//               it or /api/info read while hidden;
//   - follow:   Stage pages (WebKit: rotation pages), /preview's headers
//               stubbed: a changed X-Screen not naming the screen within
//               1.5 s, one the page's list lacks included (named, its place
//               without a number), or X-Next not naming the next one;
//   - press:    Stage (rotation pages), Chromium and WebKit, /api/info,
//               /preview's X-Brightness and refusals stubbed: Dimmer and
//               Brighter pressed fast moving the steps, the level line and
//               the ends at once, each posting its target, without a
//               navigation; a refusal putting back /api/info's level with
//               the answer's words; the poll's level (a knob press) showing
//               within 2 s; Send clearing the field and saying so, in place;
//   - unsaved:  Running order, Panel, Feedback and each screen's page: the save bar hidden as loaded,
//               shown with "Unsaved changes" and Discard once a box (a
//               letter typed or a style chosen on a screen page without one;
//               on Running order, ArrowDown on a handle) changes something, the browser
//               asking before leaving, Discard putting the box (and the
//               order) back and hiding the bar; with scripts off, Save in
//               view and Discard not; after a refused or failed save
//               (order-failed, order-refused, panel-failed,
//               feedback-failed, screen-<slug>-failed), the bar in
//               view as loaded, the browser asking before leaving, and
//               Discard opening the stored settings without asking;
//   - drag:     Running order (but after a refused or failed save),
//               Chromium and WebKit: a handle on every row and no arrow
//               with scripts, the two arrows and no handle without; row 3
//               dragged above row 1 by its handle putting its order field
//               first and the rows' numbers in order, showing the save bar
//               with its words, saying the screen's new place in #ann and
//               naming its handle for it;
//   - keys:     the same pages and browsers: row 1's handle focused,
//               ArrowDown moving the row to place 2 and ArrowUp back, the
//               focus staying on the handle, #ann and the handle's name
//               saying the new place each time;
//   - dot:      Running order (but after a refused or failed save), scripts
//               on, Chromium and WebKit, /preview's X-Screen stubbed: the
//               dot (.at) following the screen within 2.6 s, on one row, a
//               screen not in the list taking it off, with "on stage" in the
//               row's hidden text only while it is there, the dot 8 px and
//               drawn over the poster; and no row, poster, name or handle
//               moved or resized by the dot being on a row, or on none;
//   - city:     WEATHER's page, scripts on, /places stubbed, Chromium and
//               WebKit: suggestions asked from 2 characters, 250 ms after
//               the last key, stale replies dropped; at most 5 options of at
//               least 44 px inside the window; arrows, Escape, Enter and a
//               click; a choice posting the city and its cid, typing or Discard
//               posting city again; none and a refusal closing the list;
//               scripts off, a plain field posting city;
//   - place:    WEATHER's page right after a save, scripts on, /place
//               stubbed on a fake clock, Chromium and WebKit: the Found
//               row ("searching...") filled with the place once /place
//               gives one (a failed or empty answer asked again 2 s later),
//               with the "not found" sentence when it says so, the text
//               never read as markup, and no more than 30 questions;
//   - align:    Running order, scripts on and off: the handles (the arrows
//               without scripts) and the settings column at the same x on
//               every row, one with settings or not;
//   - words:    Running order at 360 px, scripts on and off, Chromium and
//               WebKit: a screen's name broken inside a word;
//   - mark:     the header's mark not the 28 px square (the se/de square);
//   - password: Wi-Fi pages, Chromium and WebKit: each row chosen in turn,
//               the one password field shown, enabled and right under the
//               row when the row asks for a password, else hidden and
//               disabled; a name typed choosing the name row;
//   - units:    the first Wi-Fi page, Chromium and WebKit: rg() giving the
//               unit and currency of US, GB, CH, FR, CA, JP and LR, and the
//               unit, currency and clock the page posts under six locales
//               (three in WebKit, which emulates only some);
//   - code:     sign-in pages, Chromium and WebKit: six one-digit boxes
//               built from the field; a digit typed moving to the next box,
//               Backspace on an empty box going back, a pasted code filling
//               every box, the sixth digit posting the six as code;
//   - assets:   once, firmware/src/web_assets.cpp not the compressed
//               firmware/web/page.css (tools/webcheck/assets.mjs).
// One line per check and width, and the wall time; known debt is listed
// below by name.
//
//   pio test -d firmware -e native -f test_web_pages     (writes the pages)
//   node tools/webcheck/webcheck.mjs [folder]            (default firmware/.pio/webcheck)
//
// Once per clone: npm ci --prefix tools/webcheck, then, if a browser is
// missing, npx --prefix tools/webcheck playwright install chromium webkit.
// Development only: nothing here reaches the firmware.
import { readdirSync, readFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { gunzipSync } from 'node:zlib';

import { CSS_ASSET, CSS_BUDGET, CSS_SOURCE, committedBytes, joinCss } from './assets.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const WIDTHS = [360, 1280];
const HEIGHT = 800;
const MIN_TARGET = 44;
const FAILING_IMPACTS = ['serious', 'critical'];

// Known debt, accepted by name until it is fixed. Each entry gives the reason
// it is accepted (`why`); a violation on any other element,
// or of any other rule, still fails. checks: 'targets', 'contrast',
// 'axe:<rule id>' or 'structure:<part>'. selector: the elements it covers
// ('*': the whole page). None today.
const EXCEPTIONS = [];

async function loadTools() {
  try {
    const { chromium, webkit } = await import('playwright');
    const axePath = createRequire(import.meta.url).resolve('axe-core/axe.min.js');
    return { chromium, webkit, axeSource: readFileSync(axePath, 'utf8') };
  } catch (e) {
    console.error(`webcheck: ${e.message}\nRun once: npm ci --prefix tools/webcheck`);
    process.exit(2);
  }
}

// Runs in the page. Every measured element gets the names of the exceptions
// that cover it, so the verdict stays here and the page code only measures.
function measure({ exceptions, minTarget }) {
  const covers = (el, check) => exceptions
    .filter((x) => x.checks.includes(check) && el && el.matches(x.selector))
    .map((x) => x.name);
  const visible = (el) => {
    const r = el.getBoundingClientRect();
    const cs = getComputedStyle(el);
    return r.width > 0 && r.height > 0 && cs.visibility !== 'hidden' && cs.display !== 'none';
  };
  const describe = (el) => {
    const name = el.getAttribute('name') || el.id;
    const text = (el.innerText || el.value || '').trim().replace(/\s+/g, ' ').slice(0, 40);
    return `${el.tagName.toLowerCase()}${name ? `[${name}]` : ''}${text ? ` "${text}"` : ''}`;
  };

  // Targets: a checkbox or radio inside a label is reached through the label
  const isBox = (el) => el.matches('input[type=checkbox], input[type=radio]');
  const targets = [];
  for (const el of document.querySelectorAll('button, input, select, summary, label, .dg')) {
    if (el.matches('input[type=hidden], :disabled') || !visible(el)) continue;
    if (el.tagName === 'LABEL') {
      const box = el.control;
      if (!box || !isBox(box) || box.disabled) continue;
    } else if (isBox(el) && el.closest('label')) {
      continue;
    }
    const r = el.getBoundingClientRect();
    if (r.width < minTarget || r.height < minTarget) {
      targets.push({ what: `${describe(el)} ${r.width.toFixed(1)}x${r.height.toFixed(1)}`,
                     known: covers(el, 'targets') });
    } else {
      targets.push(null);
    }
  }

  // Contrast: each visible text run and each field's own text, against the
  // backgrounds stacked under it (no image backgrounds on these pages)
  const rgba = (c) => {
    const m = c.match(/rgba?\(([^)]+)\)/);
    if (!m) return [0, 0, 0, 0];
    const p = m[1].split(/[ ,/]+/).filter(Boolean).map(Number);
    return [p[0], p[1], p[2], p.length > 3 ? p[3] : 1];
  };
  const over = (top, under) => {
    const a = top[3];
    return [0, 1, 2].map((i) => top[i] * a + under[i] * (1 - a)).concat(1);
  };
  const background = (el) => {
    const layers = [];
    for (let e = el; e; e = e.parentElement) {
      const c = rgba(getComputedStyle(e).backgroundColor);
      if (c[3] > 0) layers.push(c);
      if (c[3] >= 1) break;
    }
    let bg = [255, 255, 255, 1];
    for (let i = layers.length - 1; i >= 0; i--) bg = over(layers[i], bg);
    return bg;
  };
  const luminance = (c) => {
    const ch = c.slice(0, 3).map((v) => {
      const s = v / 255;
      return s <= 0.04045 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4;
    });
    return 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2];
  };
  const contrast = [];
  const checkText = (el) => {
    if (!visible(el) || el.closest(':disabled')) return;
    const cs = getComputedStyle(el);
    const bg = background(el);
    const fg = over(rgba(cs.color), bg);
    const l1 = luminance(fg), l2 = luminance(bg);
    const ratio = (Math.max(l1, l2) + 0.05) / (Math.min(l1, l2) + 0.05);
    const size = parseFloat(cs.fontSize);
    const large = size >= 24 || (size >= 18.66 && Number(cs.fontWeight) >= 700);
    const need = large ? 3 : 4.5;
    contrast.push({ ratio, ok: ratio >= need,
                    what: `${describe(el)} ${ratio.toFixed(2)}:1 < ${need}:1`,
                    known: covers(el, 'contrast') });
  };
  const seen = new Set();
  const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
  for (let n = walker.nextNode(); n; n = walker.nextNode()) {
    const el = n.parentElement;
    if (!n.textContent.trim() || seen.has(el) || el.closest('select, option, script, style')) continue;
    seen.add(el);
    checkText(el);
  }
  for (const el of document.querySelectorAll('select, input:not([type=hidden])')) {
    if (el.matches('select') || (!isBox(el) && el.value)) checkText(el);
  }

  return {
    overflow: { scrollWidth: document.documentElement.scrollWidth, width: window.innerWidth },
    footer: { bottom: document.querySelector('footer')?.getBoundingClientRect().bottom ?? 0,
              doc: document.documentElement.scrollHeight, height: window.innerHeight },
    targets,
    contrast,
  };
}

// Runs in the page. Each settings field in a block is given values a
// person can type; any that the browser would refuse is reported. Set from
// a script, a value finds a bound (required, min, max, pattern), but never a
// half-typed number: see typedInBlocks().
function foldedFields() {
  const tries = { number: ['-1', '300', '12.345', '1e6', ''], text: ['', 'x'.repeat(200)] };
  const out = [];
  const fields = document.querySelectorAll('details [form=sv]');
  for (const el of fields) {
    const kept = el.value;
    for (const v of tries[el.type === 'number' ? 'number' : 'text']) {
      el.value = v;
      if (!el.checkValidity()) out.push({ what: `${el.name}="${v.slice(0, 12)}" stops the save`, known: [] });
    }
    el.value = kept;
  }
  return { count: fields.length, problems: out };
}

// Half-typed numbers, typed with the keyboard: a type=number field refuses
// them (badInput), and a script cannot set them to find out. Opens every
// block, so it runs after the other checks.
const TYPED = ['1e', '--', '9.', 'abc', '1,5'];
async function typedInBlocks(page) {
  await page.evaluate(() => { for (const d of document.querySelectorAll('details')) d.open = true; });
  const fields = page.locator('details input[form=sv]:not([type=hidden]):not([type=radio]):not([type=checkbox])');   // originals are not typed; choices are not typed
  const out = [];
  for (let i = 0; i < await fields.count(); i++) {
    const field = fields.nth(i);
    for (const v of TYPED) {
      await field.fill('');
      await field.pressSequentially(v);
      const [name, valid] = await field.evaluate((e) => [e.name, e.checkValidity()]);
      if (!valid) out.push({ what: `${name} typed "${v}" stops the save`, known: [] });
    }
  }
  return out;
}

const PASSWORD_PASS = 'the password field under each row that asks for one, hidden otherwise';
async function wifiFlow(context, url) {
  const page = await context.newPage();
  try {
    await page.goto(url);
    return await page.evaluate(() => {
      const out = [];
      const what = (w) => out.push({ what: w, known: [] });
      const pb = document.getElementById('pb'), pw = document.getElementById('pw');
      const on = document.getElementById('on');
      const rows = [...document.getElementsByName('ssid')];
      for (const d of document.querySelectorAll('details')) d.open = true;
      for (const r of rows) {
        r.click();
        const wants = r.hasAttribute('data-pw');
        const shown = !pb.hidden && !pw.disabled && pb.offsetParent !== null;
        const above = r.value ? r.parentNode : on;
        if (wants !== shown) what(`"${r.value}" chosen: password field ${shown ? 'shown' : 'hidden'}`);
        if (wants && pb.previousElementSibling !== above) what(`"${r.value}" chosen: password field not under it`);
      }
      rows[0].click();
      on.value = 'Typed';
      on.dispatchEvent(new Event('input', { bubbles: true }));
      if (!rows[rows.length - 1].checked || pb.hidden) what('a typed name does not choose the name row');
      return out;
    });
  } finally {
    await page.close();
  }
}

// The phone's units at Wi-Fi setup (web_access.cpp): rg() by region code,
// then the fields as filled under a context's locale, which navigator.language
// and Intl follow. [temperature unit, currency] and [tu, cu, ck] as the enums
// number them (region.h): °F 1; USD 1, GBP 2, CHF 3; 12 h 1.
const UNITS_BY_REGION = { US: [1, 1], GB: [0, 2], CH: [0, 3], FR: [0, 0], CA: [0, 0], JP: [0, 0], LR: [1, 1] };
const UNITS_BY_LOCALE = { 'en-US': '1,1,1', 'en-GB': '0,2,0', 'de-CH': '0,3,0', 'fr-FR': '0,0,0',
                          'en-CA': '0,0,1', 'ja-JP': '0,0,0' };
// Playwright's WebKit takes only some locales: de-CH, for one, comes out
// as de-DE (probed 2026-10-23), so it is given the ones it keeps
const WEBKIT_LOCALES = ['en-US', 'en-GB', 'fr-FR'];
const unitsPass = (locales) => `rg() for ${Object.keys(UNITS_BY_REGION).join(' ')}, fields under ${locales.join(' ')}`;
async function unitsCheck(engine, url, locales) {
  const out = [];
  const what = (w) => out.push({ what: w, known: [] });
  for (const locale of locales) {
    const want = UNITS_BY_LOCALE[locale];
    const context = await engine.newContext({ locale });
    try {
      const page = await context.newPage();
      await page.goto(url);
      if (locale === 'en-US') {
        // rg is the page script's mapping, a global function
        const got = await page.evaluate((codes) => codes.map((c) => rg(c).join()), Object.keys(UNITS_BY_REGION));
        Object.entries(UNITS_BY_REGION).forEach(([code, w], i) => {
          if (got[i] !== w.join()) what(`rg('${code}') ${got[i]}, not ${w.join()}`);
        });
      }
      const got = (await page.evaluate(() => ['tu', 'cu', 'ck'].map((id) => document.getElementById(id).value))).join();
      if (got !== want) what(`${locale}: posts tu,cu,ck ${got}, not ${want}`);
    } finally {
      await context.close();
    }
  }
  return out;
}

const CODE_PASS = 'six boxes, advance, back, paste, posted at the sixth digit';
async function codeBoxes(context, url) {
  const page = await context.newPage();
  const out = [];
  const what = (w) => out.push({ what: w, known: [] });
  let posted = null;
  await page.route('**/login', (route) => {
    posted = new URLSearchParams(route.request().postData() || '').get('code');
    route.fulfill({ status: 200, contentType: 'text/html', body: '<title>in</title>' });
  });
  try {
    await page.goto(url);
    const boxes = page.locator('.otp input');
    if (await boxes.count() !== 6) { what(`${await boxes.count()} boxes, not 6`); return out; }
    const focused = () => page.evaluate(() => [...document.querySelectorAll('.otp input')].indexOf(document.activeElement));
    await boxes.first().click();
    await page.keyboard.type('48');
    if (await focused() !== 2) what(`after 2 digits, focus on box ${await focused()}`);
    await page.keyboard.press('Backspace');
    if (await focused() !== 1 || await boxes.nth(1).inputValue() !== '') what('Backspace on an empty box did not clear the one before');
    // Each post navigates to /login: waited for, so the next visit is not cut
    const sent = () => page.waitForURL('**/login', { timeout: 3000 }).catch(() => {});
    let nav = sent();
    await boxes.nth(3).evaluate((b) => {
      const d = new DataTransfer();
      d.setData('text', '123 456');
      b.dispatchEvent(new ClipboardEvent('paste', { clipboardData: d, bubbles: true, cancelable: true }));
    });
    await nav;
    if (posted !== '123456') what(`a pasted code posted ${JSON.stringify(posted)}`);
    posted = null;
    await page.goto(url);
    await boxes.first().click();
    await page.keyboard.type('90817');
    if (posted !== null) what('posted before the sixth digit');
    nav = sent();
    await page.keyboard.type('2');
    await nav;
    if (posted !== '908172') what(`six digits typed posted ${JSON.stringify(posted)}`);
  } finally {
    await page.close();
  }
  return out;
}

async function runAxe({ exceptions }) {
  const result = await window.axe.run(document, { resultTypes: ['violations'] });
  const out = [];
  for (const v of result.violations) {
    for (const node of v.nodes) {
      const el = document.querySelector(node.target[node.target.length - 1]);
      out.push({
        rule: v.id,
        impact: node.impact || v.impact,
        what: `${v.id} (${node.impact || v.impact}) ${node.target.join(' ')}`,
        known: exceptions
          .filter((x) => x.checks.includes(`axe:${v.id}`) && (x.selector === '*' || (el && el.matches(x.selector))))
          .map((x) => x.name),
      });
    }
  }
  return out;
}

// Runs in the page, scripts on or off. One main, one h1, every button tied
// to a form (inside it or by form=), every POST form with an action, every
// link with an address, the skip link first and aimed at the main content.
function structure({ exceptions }) {
  const out = [];
  const add = (check, what, el) => out.push({
    what,
    known: exceptions.filter((x) => x.checks.includes(check) &&
                                    (x.selector === '*' || (el && el.matches(x.selector)))).map((x) => x.name),
  });
  const text = (el) => (el.innerText || el.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 40);
  const mains = document.querySelectorAll('main');
  if (mains.length !== 1) add('structure:main', `${mains.length} main landmarks`, mains[1] || document.body);
  const h1 = document.querySelectorAll('h1');
  if (h1.length !== 1) add('structure:h1', `${h1.length} h1`, h1[1] || document.body);
  const buttons = document.querySelectorAll('button');
  for (const b of buttons) if (!b.form) add('structure:button', `button "${text(b)}" in no form`, b);
  const forms = document.querySelectorAll('form');
  for (const f of forms) {
    if (f.getAttribute('method')?.toLowerCase() === 'post' && !f.getAttribute('action')) {
      add('structure:form', `POST form ${f.id || text(f).slice(0, 20)} without action`, f);
    }
  }
  const links = document.querySelectorAll('a');
  for (const a of links) if (!a.hasAttribute('href')) add('structure:link', `link "${text(a)}" without href`, a);
  const skip = document.querySelector('a.skip');
  const target = document.getElementById('m');
  if (!skip || skip.getAttribute('href') !== '#m' || !target || target.tagName !== 'MAIN' ||
      skip !== document.querySelector('body a, body button, body input, body select')) {
    add('structure:skip', 'no skip link first in the page, aimed at <main id=m>', document.body);
  }
  return { problems: out, counted: `${mains.length} main, ${h1.length} h1, ${buttons.length} buttons, ${forms.length} forms, ${links.length} links` };
}

// Runs in the page after each Tab: what has the focus, and whether it shows
// a ring of 2 px or more (Chromium's own "auto" ring is drawn 2 px wide)
function focusRing() {
  const el = document.activeElement;
  if (!el || el === document.body) return null;
  const cs = getComputedStyle(el);
  const outline = cs.outlineStyle !== 'none' &&
                  (cs.outlineStyle === 'auto' || parseFloat(cs.outlineWidth) >= 2);
  const shadow = cs.boxShadow !== 'none';
  const name = el.getAttribute('name') || el.id;
  const label = (el.innerText || el.value || '').trim().replace(/\s+/g, ' ').slice(0, 30);
  return {
    key: el.outerHTML.slice(0, 200) + [...el.parentNode.children].indexOf(el),
    what: `${el.tagName.toLowerCase()}${name ? `[${name}]` : ''}${label ? ` "${label}"` : ''} ` +
          `outline ${cs.outlineStyle} ${cs.outlineWidth}`,
    ok: outline || shadow,
  };
}

async function focusCheck(page) {
  const out = [];
  const seen = new Set();
  let reached = 0;
  for (let i = 0; i < 400; i++) {
    await page.keyboard.press('Tab');
    const f = await page.evaluate(focusRing);
    if (!f) { if (reached) break; continue; }
    if (seen.has(f.key)) break;                 // back at the start
    seen.add(f.key);
    reached++;
    if (!f.ok) out.push({ what: f.what, known: [] });
  }
  return { problems: out, reached };
}

// The pages' own server: absolute addresses resolve as on the panel. The
// stylesheet under any version (the bytes the host test wrote, unzipped);
// the poster sheet, the preview, an image file and /api/info from fixtures
// the host tests write, 404 when absent. A portrait Stage page gets the
// 32 x 64 preview, as the panel sends it in rotation 1 or 3.
const FIXTURES = {
  '/preview': ['preview.bmp', 'image/bmp'],
  '/images/f': ['image.gif', 'image/gif'],
  '/api/info': ['info.json', 'application/json'],
};
function serve(dir, files) {
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
    if (FIXTURES[path]) {
      const from = new URL(req.headers.referer || '/', 'http://127.0.0.1').pathname;
      const portrait = path === '/preview' && /^\/stage-(text-)?portrait-/.test(from);
      const body = read(portrait ? 'preview-portrait.bmp' : FIXTURES[path][0]);
      return body ? send(200, FIXTURES[path][1], body) : send(404, 'text/plain', '404');
    }
    // Stage's actions, accepted as the panel accepts them (pressCheck stubs
    // the refusals: a route cannot answer a redirect in WebKit)
    if (req.method === 'POST' && (path === '/brightness' || path === '/text')) {
      res.writeHead(303, { Location: '/' });
      return res.end();
    }
    const file = path.slice(1);
    if (files.includes(file)) return send(200, 'text/html; charset=utf-8', read(file));
    return send(404, 'text/plain', '404');
  });
  return new Promise((ok) => server.listen(0, '127.0.0.1', () => ok(server)));
}

// Once per run: the stylesheet the firmware carries is its source's
function assetsCheck() {
  const problems = [];
  const bytes = committedBytes(readFileSync(CSS_ASSET, 'utf8'));
  const want = joinCss(readFileSync(CSS_SOURCE, 'utf8'));
  let size = 0;
  if (!bytes) {
    problems.push({ what: 'no STYLESHEET_GZ in firmware/src/web_assets.cpp', known: [] });
  } else {
    size = bytes.length;
    let got = null;
    try { got = gunzipSync(bytes).toString('utf8'); } catch (e) { problems.push({ what: `not gzip: ${e.message}`, known: [] }); }
    if (got !== null && got !== want) {
      problems.push({ what: 'web_assets.cpp is stale: run node tools/webcheck/assets.mjs', known: [] });
    }
    if (size > CSS_BUDGET) problems.push({ what: `${size} bytes > ${CSS_BUDGET}`, known: [] });
  }
  return { problems, passText: `web_assets.cpp unzips to firmware/web/page.css, ${size} / ${CSS_BUDGET} bytes` };
}

// Stage pages: what is on the panel, its name and the live line fit the
// first window (a 32 x 64 frame drawn full width once pushed the name to
// y = 801 at 360 px and 1071 at 1280)
function stageFold(height) {
  const img = document.querySelector('.stage img');
  const problems = [];
  if (!img || !img.complete || !img.naturalWidth) problems.push({ what: 'the preview did not load', known: [] });
  for (const sel of ['.stage img', '.stage h1', '.stage .live']) {
    const e = document.querySelector(sel);
    const end = e ? Math.round(e.getBoundingClientRect().bottom) : Infinity;
    if (end > height) problems.push({ what: `${sel} ends at ${end} > ${height}`, known: [] });
  }
  // A portrait page (a 32 x 64 frame declared) must get a portrait frame
  if (img && img.naturalWidth && +img.getAttribute('height') > +img.getAttribute('width')
      && !(img.naturalHeight > img.naturalWidth)) {
    problems.push({ what: `portrait page got a ${img.naturalWidth}x${img.naturalHeight} preview`, known: [] });
  }
  const size = img ? `${img.naturalWidth}x${img.naturalHeight}` : 'no';
  return { problems, passText: `${size} preview, name and live line above ${height}` };
}

// Stage pages: the preview reloads within 1.5 s while the tab is visible,
// and nothing is fetched while it is hidden
async function liveCheck(context, url) {
  const page = await context.newPage();
  const asked = [];
  page.on('request', (r) => {
    const p = new URL(r.url()).pathname;
    if (p === '/preview' || p === '/api/info') asked.push([p, Date.now()]);
  });
  await page.goto(url);
  await page.waitForTimeout(2600);
  const problems = [];
  // The page's own load of the image is the first request; the script's
  // first reload must follow it within 1.5 s
  const previews = asked.filter(([q]) => q === '/preview');
  if (previews.length < 2 || previews[1][1] - previews[0][1] > 1500) {
    problems.push({ what: 'the preview was not reloaded within 1.5 s', known: [] });
  }
  await page.evaluate(() => {
    Object.defineProperty(document, 'visibilityState', { configurable: true, get: () => 'hidden' });
    Object.defineProperty(document, 'hidden', { configurable: true, get: () => true });
    document.dispatchEvent(new Event('visibilitychange'));
  });
  const hiddenAt = Date.now();
  await page.waitForTimeout(6000);
  const late = asked.filter(([, t]) => t > hiddenAt + 100).length;
  if (late) problems.push({ what: `${late} request(s) while hidden`, known: [] });
  await page.close();
  return problems;
}

// /preview answered as the panel does, its state in extra headers (each
// header a function, read at the request)
function previewWith(page, headers) {
  return page.route('**/preview*', async (r) => {
    const res = await r.fetch();
    const extra = {};
    for (const [k, f] of Object.entries(headers)) if (f() != null) extra[k] = String(f());
    return r.fulfill({ response: res, headers: { ...res.headers(), ...extra } });
  });
}

// Stage with scripts: the name follows /preview's X-Screen within 1.5 s,
// another page's change included (the list there may differ: a name this
// one lacks shows as sent, its place without a number); X-Next names the
// next screen
async function followCheck(context, url) {
  const page = await context.newPage();
  const problems = [];
  const fail = (what) => problems.push({ what, known: [] });
  let screen = null, next = null;
  await previewWith(page, { 'X-Screen': () => screen, 'X-Next': () => next, 'X-Next-In': () => next && 7 });
  await page.goto(url);
  const { o, l, h, e } = await page.evaluate(() => {
    const d = document.querySelector('.stage').dataset;
    return { o: d.o.split(','), l: d.l.split(','), h: document.querySelector('.stage h1').textContent, e: d.e };
  });
  const named = async (want, what) => {
    const at = Date.now();
    const ok = await page.waitForFunction((w) => document.querySelector('.stage h1').textContent === w, want,
                                          { timeout: 1500 }).then(() => true, () => false);
    if (!ok) fail(`${what}: "${await page.locator('.stage h1').textContent()}", not "${want}" within 1.5 s`);
    return Date.now() - at;
  };
  const k = l.findIndex((n) => n !== h);
  if (k >= 0) {
    screen = o[k];
    next = o[0];
    await named(l[k], `X-Screen ${o[k]}`);
    if (e) {
      const b = await page.locator('.stage .next b').textContent().catch(() => null);
      if (b !== l[0]) fail(`X-Next ${o[0]}: the next line names "${b}", not "${l[0]}"`);
    }
  }
  screen = 'ELSEWHERE';
  await named('ELSEWHERE', 'a screen not in the page\'s list');
  const live = await page.locator('.stage .live').textContent();
  if (/\d/.test(live)) fail(`a screen not in the list: the place line "${live}" keeps a number`);
  await page.close();
  return problems;
}
const FOLLOW_PASS = 'the name follows X-Screen within 1.5 s, a screen not in the list too; X-Next named';

// Stage with scripts: the actions post in place. /brightness and /text go
// to the server's 303 (or a stubbed refusal); /api/info and /preview's
// X-Brightness are stubbed: the level, which follows an accepted post as on
// the panel (so a poll between presses agrees) until set() turns the knob.
async function pressCheck(context, url) {
  const page = await context.newPage();
  const problems = [];
  const fail = (what) => problems.push({ what, known: [] });
  const posted = [];
  let level = 4, refuse = false;   // test_web_pages builds Stage at level 4
  await page.route('**/api/info', (r) => r.fulfill({ status: 200, contentType: 'application/json',
                                                     body: JSON.stringify({ screen: '', brightness: level }) }));
  await previewWith(page, { 'X-Brightness': () => level });
  await page.route('**/brightness', (r) => {
    posted.push(r.request().postData());
    if (!refuse) level = +r.request().postData().slice(2);
    return refuse ? r.fulfill({ status: 400, contentType: 'text/html', body: '<div class=er>Refused here</div>' })
      : r.continue();
  });
  await page.route('**/text', (r) => { posted.push(r.request().postData()); return r.continue(); });
  await page.goto(url);
  try {
    await pressSteps(page, fail, posted, (l, r) => { level = l; refuse = r; });
  } catch (e) {
    fail(`stopped: ${e.message.split('\n')[0]}`);
  }
  await page.close();
  return problems;
}

// The presses, in order; set(level, refuse) changes what the stubs answer
async function pressSteps(page, fail, posted, set) {
  page.setDefaultTimeout(2000);
  let navigations = 0;
  page.on('framenavigated', (f) => { if (f === page.mainFrame()) navigations++; });
  // Lit steps, the first number of the steps' words and of the night line,
  // and the ends
  const shown = () => page.evaluate(() => {
    const n = (t) => +(t.match(/\d+/) || [NaN])[0];
    const f = document.querySelector('.lv');
    const [dim, bright] = f.querySelectorAll('button');
    return { lit: f.querySelectorAll('.on').length, words: n(f.querySelector('.steps').getAttribute('aria-label')),
             line: n(f.nextElementSibling.firstChild.textContent), dim: dim.disabled, bright: bright.disabled,
             said: f.previousElementSibling.textContent };
  });
  const same = (s, lit) => s.lit === lit && s.words === lit && s.line === lit;
  const from = (await shown()).lit;
  const dimmer = page.locator('.lv button').first(), brighter = page.locator('.lv button').last();
  for (let k = 0; k < 3; k++) await dimmer.click();
  await brighter.click();
  let s = await shown();
  if (!same(s, from - 2)) fail(`after 3 x Dimmer and Brighter, ${JSON.stringify(s)}, not ${from - 2} everywhere`);
  for (let k = from - 2; k < 6; k++) await brighter.click();   // to the top end
  s = await shown();
  if (!same(s, 6) || !s.bright || s.dim) fail(`at the top: ${JSON.stringify(s)}, not 6 lit and Brighter off`);
  await page.waitForTimeout(300);
  const want = [from - 1, from - 2, from - 3, from - 2];
  for (let b = from - 1; b <= 6; b++) want.push(b);
  for (let k = 0; k < want.length; k++) want[k] = `b=${want[k] - 1}`;
  if (posted.join() !== want.join()) fail(`posted ${posted.join(' ')}, not ${want.join(' ')}`);
  set(1, true);
  await dimmer.click();
  await page.waitForFunction(() => document.querySelectorAll('.lv .on').length === 2, null, { timeout: 2000 }).catch(() => {});
  s = await shown();
  if (!same(s, 2) || s.said !== 'Refused here') fail(`after a refusal: ${JSON.stringify(s)}, not /api/info's 2 lit and its words`);
  set(4, false);   // the knob
  await page.waitForFunction(() => document.querySelectorAll('.lv .on').length === 5, null, { timeout: 2000 }).catch(() => {});
  s = await shown();
  if (!same(s, 5)) fail(`the poll's level 4 not shown within 2 s: ${JSON.stringify(s)}`);
  posted.length = 0;
  await page.fill('#tx', 'HI');
  await page.locator('#tx').evaluate((i) => i.form.requestSubmit());
  await page.waitForFunction(() => tx.value === '', null, { timeout: 2000 }).catch(() => {});
  const sent = await page.evaluate(() => [tx.value, tx.form.previousElementSibling.textContent,
                                          document.querySelector('.stage').dataset.s]);
  if (sent[0] !== '' || sent[1] !== sent[2] || posted[0] !== 'msg=HI') {
    fail(`Send: field "${sent[0]}", said "${sent[1]}", posted ${posted[0]}`);
  }
  if (navigations) fail(`${navigations} navigation(s)`);
}
const PRESS_PASS = 'in place: fast presses, the ends, a refusal undone, the knob within 2 s, Send';

// The bars in view, each with what it shows: the words, Save, Discard
function bars() {
  const seen = (el) => !!el && el.getClientRects().length > 0 && getComputedStyle(el).visibility !== 'hidden';
  return [...document.querySelectorAll('.sv')].filter(seen)
    .map((b) => ({ words: seen(b.querySelector('.u')), save: seen(b.querySelector('.p')),
                   discard: seen(b.querySelector('.dc')) }));
}

// A change a page can take back, and what it changed: its first box
// ticked; on a screen's page without one, a letter added to its first
// text field; else another style chosen
async function changer(page) {
  const box = page.locator('input[type=checkbox][form=sv]:visible').first();
  if (await box.count()) return { state: () => box.isChecked(), change: () => box.click(), undo: () => box.click() };
  const text = page.locator('input[form=sv]:not([type]):visible').first();
  if (await text.count()) {
    const was = await text.inputValue();   // fill types it whole (End does not move WebKit's caret on macOS)
    return { state: () => text.inputValue(), change: () => text.fill(`${was}x`), undo: () => text.fill(was) };
  }
  const loaded = page.locator('input[type=radio][form=sv]:checked').first();
  const other = page.locator('label:has(input[type=radio][form=sv]:not(:checked))').first();
  const was = await loaded.getAttribute('value');
  const back = page.locator(`label:has(input[type=radio][form=sv][value="${was}"])`).first();
  return { state: () => page.locator('input[type=radio][form=sv]:checked').first().getAttribute('value'),
           change: () => other.click(), undo: () => back.click() };
}

// Running order, Panel and the screens' pages with scripts: a change shows
// the bar, Discard takes it back. Returns the problems.
async function unsavedCheck(context, url, width, order) {
  const page = await context.newPage();
  await page.setViewportSize({ width, height: HEIGHT });
  await page.goto(url);
  const problems = [];
  const fail = (what) => problems.push({ what, known: [] });
  const box = await changer(page);
  const ordered = () => page.evaluate(() => [...document.querySelectorAll('input[name=o]')].map((i) => i.value).join());
  const loaded = await ordered();
  if ((await page.evaluate(bars)).length) fail('a save bar shows before any change');
  const was = await box.state();
  await box.change();
  const shown = await page.evaluate(bars);
  if (shown.length !== 1 || !shown[0].words || !shown[0].save || !shown[0].discard) {
    fail(`after a change: ${JSON.stringify(shown)}, not one bar with its words, Save and Discard`);
  }
  const said = await page.evaluate(() => ann.textContent);
  if (said !== await page.locator('.u').first().textContent()) fail(`#ann says "${said}", not the change`);
  await box.undo();
  if ((await page.evaluate(bars)).length) fail('the bar stays once the box is back as loaded');
  await box.change();
  if (order) {
    await page.locator('.dg').first().press('ArrowDown');
    if ((await ordered()) === loaded) fail('ArrowDown on a handle did not move the row');
  }
  let asked = false;
  page.on('dialog', (d) => { asked ||= d.type() === 'beforeunload'; d.dismiss(); });
  await page.close({ runBeforeUnload: true });
  await page.waitForTimeout(200);
  if (!asked) fail('leaving with a change did not ask');
  const again = await context.newPage();
  await again.setViewportSize({ width, height: HEIGHT });
  await again.goto(url);
  const box2 = await changer(again);
  await box2.change();
  if (order) await again.locator('.dg').first().press('ArrowDown');
  await again.locator('.dc:visible').click();
  if ((await again.evaluate(bars)).length) fail('Discard left a bar in view');
  if ((await box2.state()) !== was) fail('Discard did not put the box back');
  if (order && (await again.evaluate(() => [...document.querySelectorAll('input[name=o]')].map((i) => i.value).join())) !== loaded) {
    fail('Discard did not put the rows back in order');
  }
  await again.close();
  return problems;
}

// Running order with scripts: row 3 dragged by its handle above row 1, as a
// mouse does it (pointer events, as a finger's). The list is first scrolled
// so the three rows sit below the top edge, where the page would scroll.
const DRAG_PASS = 'a handle per row; row 3 dragged above row 1: order field, numbers, bar and #ann follow';
async function dragCheck(context, url, width) {
  const page = await context.newPage();
  await page.setViewportSize({ width, height: HEIGHT });
  await page.goto(url);
  const problems = [];
  const fail = (what) => problems.push({ what, known: [] });
  try {
    const rows = await page.locator('ol.list>li').count();
    const shown = await page.locator('.dg:visible').count();
    if (shown !== rows) { fail(`${shown} handles in view for ${rows} rows`); return problems; }
    const arrows = await page.locator('.mv:visible').count();
    if (arrows) fail(`${arrows} arrows in view beside the handles`);
    const ordered = () => page.evaluate(() => [...document.querySelectorAll('input[name=o]')].map((i) => i.value));
    const loaded = await ordered();
    await page.evaluate(() => scrollBy(0, document.querySelector('ol.list').getBoundingClientRect().top - 200));
    const handle = await page.locator('ol.list>li').nth(2).locator('.dg').boundingBox();
    const first = await page.locator('ol.list>li').first().boundingBox();
    await page.mouse.move(handle.x + handle.width / 2, handle.y + handle.height / 2);
    await page.mouse.down();
    await page.mouse.move(handle.x + handle.width / 2, first.y + 4, { steps: 12 });
    await page.mouse.up();
    const want = [loaded[2], loaded[0], loaded[1], ...loaded.slice(3)];
    const got = await ordered();
    if (got.join() !== want.join()) fail(`order after the drag: ${got.slice(0, 4).join(', ')}..., not ${want.slice(0, 4).join(', ')}...`);
    const numbers = await page.evaluate(() => [...document.querySelectorAll('ol.list .n')].map((n) => n.textContent).join());
    if (numbers !== loaded.map((_, i) => i + 1).join()) fail(`numbers after the drag: ${numbers}`);
    const bar = await page.evaluate(bars);
    if (bar.length !== 1 || !bar[0].words || !bar[0].save || !bar[0].discard) fail(`after the drag: ${JSON.stringify(bar)}, not one bar with its words, Save and Discard`);
    const said = await page.evaluate(() => ann.textContent);
    const name = await page.locator('ol.list>li').first().getAttribute('data-n');
    if (!said.startsWith(`${name} 1/${rows}`)) fail(`#ann says "${said}", not "${name} 1/${rows}"`);
    const label = await page.locator('ol.list>li').first().locator('.dg').getAttribute('aria-label');
    if (!handleNamed(label, name, 1, rows)) fail(`the handle is named "${label}", not for ${name} at 1 of ${rows}`);
    const moved = await page.evaluate(() => [...document.querySelectorAll('ol.list>li')].filter((l) => l.style.translate).length);
    if (moved) fail(`${moved} rows still shifted after the drop`);
  } finally {
    await page.close();
  }
  return problems;
}

async function alignOn(context, url, width) {
  const page = await context.newPage();
  await page.setViewportSize({ width, height: HEIGHT });
  try {
    await page.goto(url);
    return await page.evaluate(alignedColumns, true);
  } finally {
    await page.close();
  }
}

// Every row's boxes: the row, its poster, its name and its controls, to
// compare before and after the dot moves
function rowBoxes() {
  const box = (el) => {
    const r = el.getBoundingClientRect();
    return [r.x, r.y + scrollY, r.width, r.height].map((n) => Math.round(n * 10) / 10).join(',');
  };
  return [...document.querySelectorAll('ol.list>li')].map((li) =>
    ['', '.th', '.nm', '.dg', '.gt, .gp'].map((q) => (q ? li.querySelector(q) : li)).map(box).join(' | '));
}

// Running order with scripts, /preview stubbed to name a screen: the dot
// follows within one poll (2 s) and a second, sits over the poster at 8 px,
// the hidden text goes with it, and nothing else on the page moves
const DOT_PASS = 'the dot follows X-Screen within 2.6 s on one row, with its hidden text, 8 px over the poster; no row, poster, name or handle moves or resizes';
async function dotCheck(context, url, width) {
  const page = await context.newPage();
  await page.setViewportSize({ width, height: HEIGHT });
  const problems = [];
  const fail = (what) => problems.push({ what, known: [] });
  let screen = null;
  await previewWith(page, { 'X-Screen': () => screen });
  try {
    await page.goto(url);
    const names = await page.evaluate(() => [...document.querySelectorAll('input[name=o]')].map((i) => i.value));
    const marked = () => page.evaluate(() => [...document.querySelectorAll('ol.list>li')]
      .map((li, i) => (li.classList.contains('at') ? i : -1)).filter((i) => i >= 0));
    const follows = async (name, want) => {
      screen = name;
      const ok = await page.waitForFunction((w) => {
        const at = [...document.querySelectorAll('ol.list>li')].map((li, i) => (li.classList.contains('at') ? i : -1)).filter((i) => i >= 0);
        return at.join() === w;
      }, want.join(), { timeout: 2600 }).then(() => true, () => false);
      if (!ok) fail(`X-Screen ${name}: the dot is on rows [${await marked()}], not [${want}] within 2.6 s`);
      return ok;
    };
    const before = await page.evaluate(rowBoxes);
    const spots = [0, Math.floor(names.length / 2), names.length - 1];
    for (const k of spots) {
      if (!await follows(names[k], [k])) continue;
      const row = await page.evaluate((i) => {
        const li = document.querySelectorAll('ol.list>li')[i];
        const hidden = [...document.querySelectorAll('ol.list .vh')].map((v) => v.closest('li') === li);
        const dot = getComputedStyle(li.querySelector('.th'), '::after');
        const v = li.querySelector('.vh');
        const r = v && v.getBoundingClientRect();
        return { hidden, v: v && v.textContent.trim(), small: r && r.width <= 1 && r.height <= 1,
                 dot: [dot.position, dot.width, dot.height], on: li.querySelector('.th').getBoundingClientRect().width > 0 };
      }, k);
      if (row.hidden.length !== 1 || !row.hidden[0] || !row.v) fail(`row ${k + 1}: ${row.hidden.length} hidden "on stage" texts, ${row.v ? 'on' : 'none on'} its row`);
      if (!row.small) fail(`row ${k + 1}: the hidden text takes room`);
      if (row.dot.join() !== 'absolute,8px,8px') fail(`row ${k + 1}: the dot is ${row.dot.join(' ')}, not absolute 8px 8px`);
      const now = await page.evaluate(rowBoxes);
      const moved = now.findIndex((b, i) => b !== before[i]);
      if (moved >= 0) fail(`dot on row ${k + 1}: row ${moved + 1} moved or resized (${before[moved]} -> ${now[moved]})`);
    }
    // A screen the list lacks takes the dot off every row, and nothing moves
    if (await follows('ELSEWHERE', [])) {
      const left = await page.locator('ol.list .vh').count();
      if (left) fail(`no screen on show: ${left} hidden "on stage" texts left`);
      const now = await page.evaluate(rowBoxes);
      const moved = now.findIndex((b, i) => b !== before[i]);
      if (moved >= 0) fail(`no dot: row ${moved + 1} moved or resized (${before[moved]} -> ${now[moved]})`);
    }
  } finally {
    await page.close();
  }
  return problems;
}

// Running order: the handles (without scripts, the arrows) and the settings
// column share their x on every row, a row without settings included
const ALIGN_PASS = 'the handles (arrows) and the settings column at the same x on every row';
function alignedColumns(scripts) {
  const out = [];
  for (const [what, sel] of [[scripts ? 'handle' : 'up arrow', scripts ? '.dg' : '.mv'], ['settings column', '.gt, .gp']]) {
    const xs = [...document.querySelectorAll('ol.list>li')].map((li) => {
      const el = li.querySelector(sel);
      return el && el.getBoundingClientRect().width ? Math.round(el.getBoundingClientRect().x * 10) / 10 : null;
    });
    const set = new Set(xs);
    if (set.size !== 1 || set.has(null)) out.push({ what: `${what} x differs between rows: ${[...set].join(', ')}`, known: [] });
  }
  return out;
}

// A handle's name: its screen, then its place and the count, the only
// numbers after the name ("Move CLOCK, position 3 of 24")
function handleNamed(label, name, place, count) {
  const at = (label || '').indexOf(name);
  return at >= 0 && (label.slice(at + name.length).match(/\d+/g) || []).join() === `${place},${count}`;
}

// Row 1's handle focused, ArrowDown then ArrowUp: the row moves down a
// place and back, the focus stays on the handle, and #ann and the handle's
// name follow
const KEYS_PASS = "row 1's handle: ArrowDown to place 2, ArrowUp back, focus kept, #ann and its name follow";
async function keysCheck(context, url, width) {
  const page = await context.newPage();
  await page.setViewportSize({ width, height: HEIGHT });
  await page.goto(url);
  const problems = [];
  const fail = (what) => problems.push({ what, known: [] });
  try {
    const rows = await page.locator('ol.list>li').count();
    const ordered = () => page.evaluate(() => [...document.querySelectorAll('input[name=o]')].map((i) => i.value).join());
    const loaded = (await ordered()).split(',');
    const name = await page.locator('ol.list>li').first().getAttribute('data-n');
    await page.locator('ol.list>li').first().locator('.dg').focus();
    for (const [key, place, want] of [['ArrowDown', 2, [loaded[1], loaded[0], ...loaded.slice(2)]],
                                      ['ArrowUp', 1, loaded]]) {
      await page.keyboard.press(key);
      if ((await ordered()) !== want.join()) fail(`${key}: the order is not ${want.slice(0, 3).join(', ')}...`);
      const focused = await page.evaluate(() => {
        const f = document.activeElement;
        return f && f.matches('.dg') ? { row: f.closest('li').dataset.n, label: f.getAttribute('aria-label') } : null;
      });
      if (!focused || focused.row !== name) fail(`${key}: the focus left ${name}'s handle`);
      else if (!handleNamed(focused.label, name, place, rows)) fail(`${key}: the handle is named "${focused.label}"`);
      const said = await page.evaluate(() => ann.textContent);
      if (!said.startsWith(`${name} ${place}/${rows}`)) fail(`${key}: #ann says "${said}", not "${name} ${place}/${rows}"`);
    }
  } finally {
    await page.close();
  }
  return problems;
}

// WEATHER's page, scripts on, /places stubbed, Chromium and WebKit: the city
// field's suggestions. Nothing asked before 2 characters, one question for
// a word typed at once (250 ms after the last key), a reply that arrives
// after a newer question not shown; at most 5 buttons "Name · Region,
// Country", each at least 44 px, none wider than the window; the field a
// combobox that says whether the list is open and which option is active;
// ArrowDown and ArrowUp move, Escape closes without losing the text, Enter
// chooses; a choice (by key or by a click) fills the name and the hidden cid
// and the form then posts cid and not city; typing again, or Discard, puts
// city back and drops cid; the count, or "none", in the live line; a refusal
// or an empty reply closes the list. Scripts off, the form posts city alone.
const placePage = (file) => /^screen-weather-searching-/.test(file);
const CITY_PASS = 'asked after 2 characters and 250 ms, stale replies dropped, 5 options of 44+ px, keys, click, cid or city posted';
const PARIS = [
  { id: 1234, n: 'Paris', a: 'Île-de-France', c: 'France' },
  { id: 2345, n: 'Paris', a: 'Texas', c: 'United States' },
  { id: 3456, n: 'Parigi', a: '', c: 'Italy' },
  { id: 4567, n: 'Parintins with a long name that has to wrap inside the column rather than push it', a: 'Amazonas', c: 'Brazil' },
  { id: 5678, n: 'Paraty', a: 'Rio de Janeiro', c: 'Brazil' },
  { id: 6789, n: 'Sixth', a: 'Nowhere', c: 'Nowhere' },
  { id: 7890, n: 'Seventh', a: 'Nowhere', c: 'Nowhere' },
];
const cityPage = (file) => /^screen-weather-/.test(file) && !dirtyPage(file) && !placePage(file);

async function cityCheck(context, url, width) {
  const page = await context.newPage();
  await page.setViewportSize({ width, height: HEIGHT });
  const problems = [];
  const fail = (what) => problems.push({ what, known: [] });
  const asked = [];
  // What /places answers, by query: [status, body, delay in ms]
  const answers = {
    par: [200, PARIS], pari: [200, PARIS.slice(0, 2)], ber: [200, [{ id: 99, n: 'Berlin', a: 'Berlin', c: 'Germany' },
      { id: 98, n: 'Bern', a: 'Bern', c: 'Switzerland' }]],
    zzz: [200, []], bus: [503, { error: 'busy' }],
    sl: [200, [{ id: 1, n: 'Slow', a: '', c: 'Late' }], 800], sli: [200, [{ id: 2, n: 'Quick', a: 'Fast', c: 'First' }]],
  };
  await page.route(/\/places\?/, async (r) => {
    const q = new URL(r.request().url()).searchParams.get('q');
    asked.push(q);
    // ret: busy the first time, answered the next
    const [status, body, delay] = q === 'ret' && !asked.slice(0, -1).includes('ret')
      ? [503, { error: 'busy' }] : q === 'ret' ? [200, PARIS] : answers[q] || [200, []];
    if (delay) await new Promise((res) => setTimeout(res, delay));
    await r.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) }).catch(() => {});
  });
  await page.goto(url);
  page.setDefaultTimeout(2000);
  const field = page.locator('#city');
  const stored = await field.inputValue();
  const state = () => page.evaluate(() => ({
    shown: !pl.hidden, expanded: city.getAttribute('aria-expanded'), active: city.getAttribute('aria-activedescendant'),
    value: city.value, named: city.getAttribute('name'), cid: [cid.disabled, cid.value],
    options: [...pl.children].map((b) => ({ text: b.textContent, role: b.getAttribute('role'), id: b.id,
                                            selected: b.getAttribute('aria-selected'), h: b.getBoundingClientRect().height,
                                            right: b.getBoundingClientRect().right })),
    said: pn.textContent, ann: ann.textContent, bars: document.querySelectorAll('.sv.on').length,
    over: document.documentElement.scrollWidth,
    posted: [...new FormData(sv).entries()].filter(([k]) => k === 'city' || k === 'cid').map(([k, v]) => `${k}=${v}`).join('&'),
    focus: document.activeElement.id,
  }));
  const open = (open = true) => page.waitForFunction((o) => !pl.hidden === o, open);
  const wait = (ms) => page.waitForTimeout(ms);
  const said = (n) => page.evaluate((c) => pn.dataset.m.replace('*', c), n);
  try {
    let s = await state();
    if (s.named !== 'city' || !s.cid[0] || s.cid[1] !== '' || s.shown || s.expanded !== 'false'
        || await field.getAttribute('role') !== 'combobox') {
      fail(`as loaded: ${JSON.stringify({ named: s.named, cid: s.cid, shown: s.shown, expanded: s.expanded })}, not a closed combobox posting city`);
    }
    // One character, then a word typed at once: one question, after the pause
    await field.fill('');
    await field.pressSequentially('p');
    await wait(450);
    if (asked.length) fail(`asked ${asked} for 1 character`);
    await field.pressSequentially('ar');
    await wait(120);
    if (asked.length) fail('asked before the 250 ms pause');
    await open();
    s = await state();
    if (asked.join() !== 'par') fail(`asked "${asked}" for "par" typed at once`);
    const texts = s.options.map((o) => o.text);
    if (s.options.length !== 5) fail(`${s.options.length} options for 7 replies, not 5`);
    const want = ['Paris · Île-de-France, France', 'Paris · Texas, United States', 'Parigi · Italy'];
    if (want.some((w, i) => texts[i] !== w)) fail(`options read ${JSON.stringify(texts.slice(0, 3))}, not ${JSON.stringify(want)}`);
    if (s.options.some((o) => o.role !== 'option' || o.h < 44)) fail(`options: ${JSON.stringify(s.options.map((o) => [o.role, o.h]))}, not 44+ px options`);
    if (s.options.some((o) => o.right > width) || s.over > width) fail(`the list is wider than the window (${s.over} > ${width})`);
    if (s.expanded !== 'true' || s.said !== await said(5)) fail(`open: aria-expanded ${s.expanded}, live line "${s.said}"`);
    // Keys: the focus stays in the field, the active option moves
    await page.keyboard.press('ArrowDown');
    await page.keyboard.press('ArrowDown');
    s = await state();
    if (s.active !== 'o1' || s.options.filter((o) => o.selected === 'true').map((o) => o.id).join() !== 'o1' || s.focus !== 'city') {
      fail(`after 2 x ArrowDown: active ${s.active}, selected ${s.options.filter((o) => o.selected === 'true').map((o) => o.id)}, focus ${s.focus}; not o1 in the field`);
    }
    await page.keyboard.press('ArrowUp');
    if ((await state()).active !== 'o0') fail('ArrowUp did not go back to o0');
    await page.keyboard.press('Escape');
    s = await state();
    if (s.shown || s.expanded !== 'false' || s.active || s.value !== 'par') fail(`Escape: ${JSON.stringify({ shown: s.shown, expanded: s.expanded, active: s.active, value: s.value })}, not closed with "par" kept`);
    // Typing again asks again; Enter chooses the active option
    await field.pressSequentially('i');
    await open();
    if (asked.join() !== 'par,pari') fail(`asked "${asked}", not par then pari`);
    await page.keyboard.press('ArrowDown');
    await page.keyboard.press('ArrowDown');
    const navigations = [];
    page.on('framenavigated', (f) => { if (f === page.mainFrame()) navigations.push(f.url()); });
    await page.keyboard.press('Enter');
    s = await state();
    if (s.value !== 'Paris' || s.cid[0] || s.cid[1] !== '2345' || s.named !== 'city' || s.shown || s.posted !== 'city=Paris&cid=2345') {
      fail(`Enter on option 2: ${JSON.stringify({ value: s.value, cid: s.cid, named: s.named, shown: s.shown, posted: s.posted })}, not Paris, city and cid 2345 posted`);
    }
    if (s.focus !== 'city') fail(`the focus is on "${s.focus}" after a choice`);
    if (s.bars !== 2 || !s.ann.startsWith('Paris · Texas, United States')) fail(`after a choice: ${s.bars} bars, #ann "${s.ann}"`);
    await wait(200);
    if (navigations.length) fail(`Enter left the page: ${navigations}`);
    // Typing again takes the choice back
    await field.pressSequentially('x');
    s = await state();
    if (!s.cid[0] || s.cid[1] !== '' || s.named !== 'city' || s.posted !== 'city=Parisx') {
      fail(`typing after a choice: ${JSON.stringify({ cid: s.cid, named: s.named, posted: s.posted })}, not city posted alone`);
    }
    // A click chooses too (WebKit gives a button no focus: the list must still be there)
    await field.fill('ber');
    await open();
    await page.locator('#pl button').nth(1).click();
    s = await state();
    if (s.value !== 'Bern' || s.cid[1] !== '98' || s.posted !== 'city=Bern&cid=98' || s.shown) {
      fail(`a click on option 2: ${JSON.stringify({ value: s.value, cid: s.cid, posted: s.posted, shown: s.shown })}, not Bern, cid 98`);
    }
    // Discard puts the stored city back
    await page.locator('.dc:visible').click();
    s = await state();
    if (s.value !== stored || !s.cid[0] || s.cid[1] !== '' || s.named !== 'city' || s.posted !== `city=${stored}` || s.bars) {
      fail(`Discard: ${JSON.stringify({ value: s.value, cid: s.cid, named: s.named, posted: s.posted, bars: s.bars })}, not ${stored} posted as city`);
    }
    // Nothing found, a refusal, a text too long for the table
    asked.length = 0;
    await field.fill('zzz');
    await page.waitForFunction(() => pn.textContent !== '');
    s = await state();
    if (s.shown || s.said !== await page.evaluate(() => pn.dataset.z)) fail(`no result: shown ${s.shown}, said "${s.said}"`);
    await field.fill('bus');
    await page.waitForFunction(() => pn.textContent === '');
    await wait(100);
    s = await state();
    if (s.shown || s.said || s.posted !== 'city=bus') fail(`a 503: ${JSON.stringify({ shown: s.shown, said: s.said, posted: s.posted })}, not closed and silent, posting city`);
    // Busy, then answered: the page asks again a second later
    asked.length = 0;
    await field.fill('ret');
    await page.waitForFunction(() => !pl.hidden, null, { timeout: 3000 }).catch(() => {});
    s = await state();
    if (!s.shown || asked.filter((q) => q === 'ret').length !== 2) fail(`a 503 then a 200: shown ${s.shown}, asked ${JSON.stringify(asked)}, not asked again and listed`);
    await field.fill('é'.repeat(17));
    await wait(450);
    if (asked.includes('é'.repeat(17))) fail('asked for 34 bytes');
    // A late reply to an old question is dropped
    asked.length = 0;
    await field.fill('sl');
    await wait(400);
    await field.fill('sli');
    await open();
    await wait(900);
    s = await state();
    if (asked.join() !== 'sl,sli' || s.options.map((o) => o.text).join() !== 'Quick · Fast, First') {
      fail(`late reply: asked "${asked}", list ${JSON.stringify(s.options.map((o) => o.text))}, not Quick alone`);
    }
  } catch (e) {
    fail(`stopped: ${e.message.split('\n')[0]}`);
  }
  await page.close();
  return problems;
}

// The Found row while the city is looked up (#fd): /place is asked every
// 2 s of a fake clock, 30 times at most. Returns the problems.
const PLACE_PASS = 'filled when /place answers, "not found" on f, text not markup, asked again after an error, 30 questions at most';
async function placeCheck(context, url, width) {
  const problems = [];
  const fail = (what) => problems.push({ what, known: [] });
  // Each case: what /place answers, in turn (the last one for ever)
  const run = async (answers, steps) => {
    const page = await context.newPage();
    await page.setViewportSize({ width, height: HEIGHT });
    let asked = 0;
    await page.route(/\/place(\?|$)/, async (r) => {
      const [status, body] = answers[Math.min(asked++, answers.length - 1)];
      await r.fulfill({ status, contentType: 'application/json', body }).catch(() => {});
    });
    await page.clock.install();
    await page.goto(url);
    const row = () => page.evaluate(() => ({ text: fd.textContent, kids: fd.children.length, f: fd.dataset.f }));
    const first = await row();
    for (let i = 0; i < steps; i++) {
      await page.clock.runFor(2000);
      await page.waitForTimeout(20);
    }
    const last = await row();
    await page.close();
    return { first, last, asked };
  };
  try {
    const ok = (p) => [200, JSON.stringify({ p })];
    let r = await run([[500, 'oops'], ok(''), ok('Innsbruck, Tyrol, Austria')], 6);
    if (!r.first.text || !r.first.f) fail(`as loaded: ${JSON.stringify(r.first)}, not the searching text with its not-found sentence`);
    if (r.last.text !== 'Innsbruck, Tyrol, Austria' || r.asked !== 3) fail(`after an error, an empty and a full answer: "${r.last.text}" after ${r.asked} questions, not the place after 3`);
    r = await run([[200, JSON.stringify({ p: '', f: 1 })]], 3);
    if (r.last.text !== r.first.f || r.asked !== 1) fail(`not found: "${r.last.text}" after ${r.asked} questions, not "${r.first.f}" after 1`);
    r = await run([ok('<i>x</i> & "q"')], 2);
    if (r.last.text !== '<i>x</i> & "q"' || r.last.kids) fail(`a place read as markup: ${JSON.stringify(r.last)}`);
    r = await run([ok('')], 40);
    if (r.asked !== 30 || r.last.text !== r.first.text) fail(`never found: ${r.asked} questions, "${r.last.text}"; not 30 and the searching text`);
  } catch (e) {
    fail(`stopped: ${e.message.split('\n')[0]}`);
  }
  return problems;
}

// Scripts off: the field is a plain one that posts city, the list not in view
async function cityOff(off) {
  const o = await off.evaluate(() => ({
    posted: [...new FormData(sv).entries()].filter(([k]) => k === 'city' || k === 'cid').map(([k, v]) => `${k}=${v}`).join('&'),
    stored: city.defaultValue, list: pl.getBoundingClientRect().height, role: city.getAttribute('role'),
  }));
  return o.posted === `city=${o.stored}` && !o.list && !o.role ? []
    : [{ what: `scripts off: ${JSON.stringify(o)}, not city=stored posted from a plain field`, known: [] }];
}

// Scripts off: no handle in view, and each row's two arrows (a row moves
// with them, posted)
function handlesWithoutScript(handles, arrows, rows) {
  const out = [];
  if (handles) out.push({ what: `scripts off: ${handles} drag handles in view`, known: [] });
  if (arrows !== 2 * rows) out.push({ what: `scripts off: ${arrows} arrows in view for ${rows} rows`, known: [] });
  return out;
}

// Running order at 360 px: a screen's name (the row's first words) never
// broken inside a word. A word's characters, measured as a range, sit on
// one line.
const WORDS_PASS = 'no screen name broken inside a word';
function brokenNames() {
  const out = [];
  for (const nm of document.querySelectorAll('.sr .nm')) {
    const text = nm.firstChild;
    if (!text || text.nodeType !== Node.TEXT_NODE) continue;
    const range = document.createRange();
    for (const m of text.data.matchAll(/\S+/g)) {
      range.setStart(text, m.index);
      range.setEnd(text, m.index + m[0].length);
      const tops = new Set([...range.getClientRects()].map((r) => Math.round(r.top)));
      if (tops.size > 1) out.push({ what: `"${m[0]}" broken over ${tops.size} lines`, known: [] });
    }
  }
  return out;
}

// A refused or failed save: the page holds the posted values, so the bar is
// up as loaded, leaving asks, and Discard opens the stored settings (back),
// with a GET and without the question. Returns the problems.
async function dirtyCheck(context, url, width, back) {
  const page = await context.newPage();
  await page.setViewportSize({ width, height: HEIGHT });
  await page.route(`**${back}`, (r) => r.fulfill({ status: 200, contentType: 'text/html', body: 'stored' }));
  await page.goto(url);
  const problems = [];
  const fail = (what) => problems.push({ what, known: [] });
  const shown = await page.evaluate(bars);
  if (shown.length !== 1 || !shown[0].words || !shown[0].save || !shown[0].discard) {
    fail(`as loaded: ${JSON.stringify(shown)}, not one bar with its words, Save and Discard`);
  }
  if (!(await page.locator('.dc:visible').count())) {
    await page.close();
    return problems;
  }
  let asked = false;
  page.on('dialog', (d) => { asked ||= d.type() === 'beforeunload'; d.dismiss(); });
  const gone = page.waitForRequest((r) => new URL(r.url()).pathname === back && r.method() === 'GET', { timeout: 2000 })
    .catch(() => null);
  await page.locator('.dc:visible').click();
  if (!(await gone)) fail(`Discard did not open ${back}`);
  await page.waitForTimeout(200);
  if (asked) fail('Discard asked before leaving');
  await page.close();
  const again = await context.newPage();
  await again.setViewportSize({ width, height: HEIGHT });
  await again.goto(url);
  // A browser asks only once the page had a click (sticky activation): the
  // handler must be armed as loaded, and ask after any click
  const armed = await again.evaluate(() => {
    const e = new Event('beforeunload', { cancelable: true });
    dispatchEvent(e);
    return e.defaultPrevented;
  });
  if (!armed) fail('the leaving question is not armed as loaded');
  await again.locator('h1').click();
  asked = false;
  again.on('dialog', (d) => { asked ||= d.type() === 'beforeunload'; d.dismiss(); });
  await again.close({ runBeforeUnload: true });
  await again.waitForTimeout(200).catch(() => {});
  if (!asked) fail('leaving the posted values did not ask');
  if (!again.isClosed()) await again.close();
  return problems;
}

// The answer to a refused or failed save, and the stored page Discard opens
const dirtyPage = (file) => /^(order-(failed|refused)|panel-failed|feedback-failed|screen-.+-failed)-/.test(file);
const storedPage = (file) => (file.startsWith('order-') ? '/screens'
  : file.startsWith('feedback-') ? '/panel/feedback'
  : file.startsWith('screen-') ? `/screens/${file.match(/^screen-(.+)-[a-z]+-[a-z]+\.html$/)[1]}` : '/panel');
const unsavedPass = (file) => (dirtyPage(file)
  ? 'in view as loaded, asks before leaving, Discard opens the stored settings'
  : 'hidden as loaded, shown on a change, asks before leaving, Discard restores');
const unsaved = (context, file, url, width) => (dirtyPage(file)
  ? dirtyCheck(context, url, width, storedPage(file))
  : unsavedCheck(context, url, width, file.startsWith('order-')));

// Scripts off: Save in view, the words and Discard not
function savedWithoutScript(shown) {
  const ok = shown.length === 1 && shown[0].save && !shown[0].words && !shown[0].discard;
  return ok ? [] : [{ what: `scripts off: ${JSON.stringify(shown)}, not Save alone`, known: [] }];
}

// IMAGE's page has nothing to save: no form, no bar
const formPage = (file) => /^(order|panel|feedback)-/.test(file) || (/^screen-/.test(file) && !/^screen-image-/.test(file));
const dragPage = (file) => file.startsWith('order-') && !dirtyPage(file);

// At most `width` jobs at once, results in the jobs' order
async function pool(items, width, job) {
  const results = new Array(items.length);
  let next = 0;
  await Promise.all(Array.from({ length: Math.min(width, items.length) }, async () => {
    while (next < items.length) {
      const i = next++;
      results[i] = await job(items[i]);
    }
  }));
  return results;
}

const started = Date.now();
const dir = resolve(process.argv[2] || join(HERE, '../../firmware/.pio/webcheck'));
let files = [];
try {
  files = readdirSync(dir).filter((f) => f.endsWith('.html')).sort();
} catch {
  // reported below
}
if (!files.length) {
  console.error(`webcheck: no page in ${dir}; run pio test -d firmware -e native -f test_web_pages first`);
  process.exit(2);
}

const { chromium, webkit, axeSource } = await loadTools();
const server = await serve(dir, files);
const origin = `http://127.0.0.1:${server.address().port}`;
const used = new Set();
let checks = 0, failed = 0;

const known = (items) => [...new Set(items.flatMap((i) => i.known))];
function report({ check, where, problems, passText }) {
  checks++;
  const fails = problems.filter((p) => !p.known.length);
  const names = known(problems);
  names.forEach((n) => used.add(n));
  const tail = names.length ? ` (known: ${names.join(', ')})` : '';
  if (fails.length) {
    failed++;
    console.log(`FAIL  ${check.padEnd(11)} ${where}  ${fails.length} problem(s)${tail}`);
    for (const p of fails) console.log(`        ${p.what}`);
  } else {
    console.log(`ok    ${check.padEnd(11)} ${where}  ${passText}${tail}`);
  }
}

// The checks every engine runs on one page at one width
function layout(m, where) {
  const o = m.overflow;
  const small = m.targets.filter(Boolean);
  return [
    { check: 'overflow', where, passText: `scrollWidth ${o.scrollWidth} <= ${o.width}`,
      problems: o.scrollWidth > o.width ? [{ what: `scrollWidth ${o.scrollWidth} > ${o.width}`, known: [] }] : [] },
    { check: 'targets', where, problems: small,
      passText: `${m.targets.length - small.length} controls >= ${MIN_TARGET}x${MIN_TARGET}` },
    footerAtBottom(m.footer, where),
  ];
}

// A page shorter than the window keeps its footer on the bottom edge; a
// taller one scrolls, and the footer follows the content
function footerAtBottom(f, where) {
  const short = f.doc <= f.height;
  const gap = short && f.bottom < f.height - 1;
  return { check: 'footer', where,
           passText: short ? `bottom ${f.bottom.toFixed(1)} at ${f.height}` : `page ${f.doc} px, scrolls`,
           problems: gap ? [{ what: `footer bottom ${f.bottom.toFixed(1)} < ${f.height - 1}`, known: [] }] : [] };
}

const PARALLEL = 4;
const assets = assetsCheck();
report({ check: 'assets', where: 'c.css', ...assets });

const browser = await chromium.launch();
try {
  const results = await pool(files, PARALLEL, async (file) => {
    const out = [];
    const url = `${origin}/${file}`;
    const context = await browser.newContext();
    const quiet = await browser.newContext({ javaScriptEnabled: false });
    const still = await browser.newContext({ reducedMotion: 'reduce' });
    try {
      for (const width of WIDTHS) {
        const where = `${file} @${width}`;
        const page = await context.newPage();
        await page.setViewportSize({ width, height: HEIGHT });
        await page.goto(url);
        const m = await page.evaluate(measure, { exceptions: EXCEPTIONS, minTarget: MIN_TARGET });
        out.push(...layout(m, where));
        if (file.startsWith('order-') && width === WIDTHS[0]) {
          out.push({ check: 'words', where, passText: WORDS_PASS, problems: await page.evaluate(brokenNames) });
        }
        const low = m.contrast.filter((c) => !c.ok);
        const lowest = Math.min(...m.contrast.map((c) => c.ratio));
        out.push({ check: 'contrast', where, problems: low,
                   passText: `${m.contrast.length} text runs >= AA, lowest ${lowest.toFixed(2)}:1` });
        const mark = await page.evaluate(() => {
          const b = document.querySelector('.brand');
          const cs = b && getComputedStyle(b, '::before');
          return cs ? `${cs.width} x ${cs.height}` : 'none';
        });
        out.push({ check: 'mark', where, passText: mark,
                   problems: mark === '28px x 28px' ? [] : [{ what: `brand mark ${mark}, not 28px x 28px`, known: [] }] });
        const s = await page.evaluate(structure, { exceptions: EXCEPTIONS });
        out.push({ check: 'structure', where, problems: s.problems, passText: s.counted });
        if (file.startsWith('stage-')) out.push({ check: 'fold', where, ...await page.evaluate(stageFold, HEIGHT) });
        if (width === WIDTHS[WIDTHS.length - 1]) {
          const f = await focusCheck(page);
          out.push({ check: 'focus', where, problems: f.problems,
                     passText: `${f.reached} focus stops, each with a ring >= 2 px` });
        }
        const folded = await page.evaluate(foldedFields);
        await page.addScriptTag({ content: axeSource });
        const violations = await page.evaluate(runAxe, { exceptions: EXCEPTIONS });
        const failing = violations.filter((v) => FAILING_IMPACTS.includes(v.impact) || v.known.length);
        const minor = violations.length - failing.length;
        out.push({ check: 'axe', where, problems: failing,
                   passText: `0 serious or critical${minor ? `, ${minor} moderate or minor not gated` : ''}` });
        const typed = await typedInBlocks(page);
        out.push({ check: 'folded', where, problems: [...folded.problems, ...typed],
                   passText: `${folded.count} fields in blocks post whatever is typed` });
        await page.close();

        // Scripts off: every action is a link or a form, so the page must
        // hold together without them
        const off = await quiet.newPage();
        await off.setViewportSize({ width, height: HEIGHT });
        await off.goto(url);
        const mo = await off.evaluate(measure, { exceptions: EXCEPTIONS, minTarget: MIN_TARGET });
        const so = await off.evaluate(structure, { exceptions: EXCEPTIONS });
        for (const r of layout(mo, `${where} no-js`)) out.push(r);
        out.push({ check: 'structure', where: `${where} no-js`, problems: so.problems, passText: so.counted });
        if (formPage(file)) {
          out.push({ check: 'unsaved', where: `${where} no-js`, problems: savedWithoutScript(await off.evaluate(bars)),
                     passText: 'Save in view, no Discard' });
        }
        if (file.startsWith('order-')) {
          out.push({ check: 'align', where: `${where} no-js`, problems: await off.evaluate(alignedColumns, false),
                     passText: ALIGN_PASS });
          out.push({ check: 'drag', where: `${where} no-js`, passText: 'no handle in view, two arrows a row',
                     problems: handlesWithoutScript(await off.locator('.dg:visible').count(),
                                                    await off.locator('.mv:visible').count(),
                                                    await off.locator('ol.list>li').count()) });
          if (width === WIDTHS[0]) {
            out.push({ check: 'words', where: `${where} no-js`, passText: WORDS_PASS,
                       problems: await off.evaluate(brokenNames) });
          }
        }
        if (cityPage(file)) {
          out.push({ check: 'city', where: `${where} no-js`, problems: await cityOff(off),
                     passText: 'a plain field posting city, no list' });
        }
        await off.close();
        if (dragPage(file)) {
          out.push({ check: 'drag', where, problems: await dragCheck(context, url, width), passText: DRAG_PASS });
          out.push({ check: 'keys', where, problems: await keysCheck(context, url, width), passText: KEYS_PASS });
          out.push({ check: 'dot', where, problems: await dotCheck(context, url, width), passText: DOT_PASS });
          out.push({ check: 'align', where, passText: ALIGN_PASS,
                     problems: await alignOn(context, url, width) });
        }
        if (formPage(file)) {
          out.push({ check: 'unsaved', where, problems: await unsaved(context, file, url, width),
                     passText: unsavedPass(file) });
        }
        if (cityPage(file)) {
          out.push({ check: 'city', where, problems: await cityCheck(context, url, width), passText: CITY_PASS });
        }
        if (placePage(file)) {
          out.push({ check: 'place', where, problems: await placeCheck(context, url, width), passText: PLACE_PASS });
        }
      }
      // Reduced motion: nothing moves
      const calm = await still.newPage();
      await calm.goto(url);
      const moving = await calm.evaluate(() => document.getAnimations()
        .filter((a) => a.playState === 'running').map((a) => a.animationName || a.transitionProperty || 'animation'));
      out.push({ check: 'motion', where: `${file} reduced`, problems: moving.map((n) => ({ what: `${n} runs`, known: [] })),
                 passText: 'no animation or transition runs' });
      await calm.close();
      if (file.startsWith('stage-')) {
        out.push({ check: 'live', where: file, problems: await liveCheck(context, url),
                   passText: 'preview within 1.5 s, nothing while hidden' });
        out.push({ check: 'follow', where: file, problems: await followCheck(context, url), passText: FOLLOW_PASS });
      }
      if (file.startsWith('stage-rotation-')) {
        out.push({ check: 'press', where: file, problems: await pressCheck(context, url), passText: PRESS_PASS });
      }
      if (file.startsWith('signin-')) {
        out.push({ check: 'code', where: file, problems: await codeBoxes(context, url), passText: CODE_PASS });
      }
      if (file.startsWith('wifi-')) {
        out.push({ check: 'password', where: file, problems: await wifiFlow(context, url), passText: PASSWORD_PASS });
      }
      if (file === 'wifi-first-en.html') {
        const locales = Object.keys(UNITS_BY_LOCALE);
        out.push({ check: 'units', where: file, problems: await unitsCheck(browser, url, locales),
                   passText: unitsPass(locales) });
      }
    } finally {
      await Promise.all([context.close(), quiet.close(), still.close()]);
    }
    return out;
  });
  for (const r of results.flat()) report(r);
} finally {
  await browser.close();
}

// WebKit's native drop-down ignores min-height (it computes 18 px) and drew
// the selects 23 px tall; Chromium alone never saw it
const safari = await webkit.launch();
try {
  const results = await pool(files, PARALLEL, async (file) => {
    const out = [];
    const context = await safari.newContext();
    try {
      for (const width of WIDTHS) {
        const page = await context.newPage();
        await page.setViewportSize({ width, height: HEIGHT });
        await page.goto(`${origin}/${file}`);
        const m = await page.evaluate(measure, { exceptions: EXCEPTIONS, minTarget: MIN_TARGET });
        out.push(...layout(m, `${file} @${width} webkit`));
        if (file.startsWith('order-') && width === WIDTHS[0]) {
          out.push({ check: 'words', where: `${file} @${width} webkit`, passText: WORDS_PASS,
                     problems: await page.evaluate(brokenNames) });
        }
        await page.close();
        if (file.startsWith('signin-') && width === WIDTHS[0]) {
          out.push({ check: 'code', where: `${file} webkit`,
                     problems: await codeBoxes(context, `${origin}/${file}`), passText: CODE_PASS });
        }
        if (file.startsWith('wifi-') && width === WIDTHS[0]) {
          out.push({ check: 'password', where: `${file} webkit`,
                     problems: await wifiFlow(context, `${origin}/${file}`), passText: PASSWORD_PASS });
        }
        if (file === 'wifi-first-en.html' && width === WIDTHS[0]) {
          out.push({ check: 'units', where: `${file} webkit`,
                     problems: await unitsCheck(safari, `${origin}/${file}`, WEBKIT_LOCALES),
                     passText: unitsPass(WEBKIT_LOCALES) });
        }
        if (file.startsWith('stage-rotation-') && width === WIDTHS[0]) {
          out.push({ check: 'press', where: `${file} webkit`, problems: await pressCheck(context, `${origin}/${file}`),
                     passText: PRESS_PASS });
          out.push({ check: 'follow', where: `${file} webkit`,
                     problems: await followCheck(context, `${origin}/${file}`), passText: FOLLOW_PASS });
        }
        if (dragPage(file)) {
          out.push({ check: 'drag', where: `${file} @${width} webkit`,
                     problems: await dragCheck(context, `${origin}/${file}`, width), passText: DRAG_PASS });
          out.push({ check: 'keys', where: `${file} @${width} webkit`,
                     problems: await keysCheck(context, `${origin}/${file}`, width), passText: KEYS_PASS });
          out.push({ check: 'dot', where: `${file} @${width} webkit`,
                     problems: await dotCheck(context, `${origin}/${file}`, width), passText: DOT_PASS });
          out.push({ check: 'align', where: `${file} @${width} webkit`, passText: ALIGN_PASS,
                     problems: await alignOn(context, `${origin}/${file}`, width) });
        }
        if (cityPage(file)) {
          out.push({ check: 'city', where: `${file} @${width} webkit`,
                     problems: await cityCheck(context, `${origin}/${file}`, width), passText: CITY_PASS });
        }
        if (placePage(file)) {
          out.push({ check: 'place', where: `${file} @${width} webkit`,
                     problems: await placeCheck(context, `${origin}/${file}`, width), passText: PLACE_PASS });
        }
        if (formPage(file)) {
          out.push({ check: 'unsaved', where: `${file} @${width} webkit`,
                     problems: await unsaved(context, file, `${origin}/${file}`, width),
                     passText: unsavedPass(file) });
        }
      }
    } finally {
      await context.close();
    }
    return out;
  });
  for (const r of results.flat()) report(r);
} finally {
  await safari.close();
  server.close();
}

for (const x of EXCEPTIONS) {
  console.log(used.has(x.name)
    ? `known ${x.name}: ${x.why}`
    : `note  ${x.name} was not seen: remove it once its debt is fixed (${x.why})`);
}
const seconds = ((Date.now() - started) / 1000).toFixed(1);
console.log(`webcheck: ${checks - failed} of ${checks} checks passed, ${files.length} pages x ${WIDTHS.length} widths, Chromium and WebKit, scripts on and off, in ${seconds} s`);
process.exit(failed ? 1 : 0);
