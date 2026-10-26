// SPDX-License-Identifier: AGPL-3.0-only
// Runs every example animation of simulator.html for a few hundred frames, in
// each orientation it supports, with the simulator's own engine. Catches the
// errors a refactor or a translation would introduce, without a browser.
// Then loads the list as a browser would (fresh, or saved by an older version)
// and checks where the examples land.
//
//   node tools/simulator/selftest.mjs [path/to/simulator.html]
//
// The engine is the first part of the main <script>, up to the END ENGINE
// marker: it draws into a framebuffer and never touches the DOM.
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const FRAMES = 400;
const file = process.argv[2] || fileURLToPath(new URL('simulator.html', import.meta.url));
const html = readFileSync(file, 'utf8');

const main = html.slice(html.lastIndexOf('<script>') + '<script>'.length);
const endMarker = main.search(/\/\/ ==== (END ENGINE|FIN MOTEUR) ====/);
if (endMarker < 0) throw new Error('engine end marker not found');
const engine = main.slice(0, endMarker);

// Examples: <script type="text/plain" data-example="NAME" data-orientation="...">
const examples = [...html.matchAll(
  /<script type="text\/plain" data-ex(?:emple|ample)="([^"]+)"([^>]*)>\n([\s\S]*?)<\/script>/g)]
  .map(([, name, attrs, code]) => {
    const orientation = /data-orientation="([^"]+)"/.exec(attrs)?.[1];
    const orientations = { both: [0, 1], deux: [0, 1], portrait: [1] }[orientation || 'landscape'] || [0];
    return { name, code, orientation, orientations };
  });

// A test that finds nothing must not pass.
if (!examples.length) throw new Error('no example found in ' + file);

// Offline and storage-free: examples that fetch live data must cope with failure.
globalThis.fetch = () => Promise.reject(new Error('offline (selftest)'));
const store = new Map();
globalThis.localStorage = {
  getItem: k => (store.has(k) ? store.get(k) : null),
  setItem: (k, v) => store.set(k, String(v)),
  removeItem: k => store.delete(k),
};

// The engine, plus a small probe exposing what the test needs, whatever the
// identifiers are called (French before the translation, English after).
const probe = `
  const st = typeof state !== 'undefined' ? state : etat;
  return {
    compile: typeof compile === 'function' ? compile : compiler,
    setRotation: r => { st.rotation = r; },
    display,
  };`;
const api = new Function(engine + probe)();

const failures = [];
for (const ex of examples) {
  for (const rotation of ex.orientations) {
    api.setRotation(rotation);
    try {
      const anim = api.compile(ex.code);
      for (let f = 0; f < FRAMES; f++) anim(f, f === 0);
    } catch (e) {
      failures.push(`${ex.name} (${rotation ? 'portrait' : 'landscape'}): ${e && e.message}`);
    }
  }
}

// The IMAGE prototype shows the firmware's own panel words: each one of its
// WORDS must exist as a string literal in i18n.cpp, so the two never drift.
const imageEx = examples.find(ex => ex.name === 'IMAGE');
if (!imageEx) {
  failures.push('IMAGE: example not found');
} else {
  const block = /const WORDS = \{([\s\S]*?)\n\};/.exec(imageEx.code);
  const words = block ? [...block[1].matchAll(/"([^"]*)"/g)].map(m => m[1]) : [];
  const i18n = readFileSync(fileURLToPath(new URL('../../firmware/src/i18n.cpp', import.meta.url)), 'utf8');
  if (!words.length) failures.push('IMAGE: no WORDS found');
  for (const w of words) {
    if (!i18n.includes(`"${w}"`)) failures.push(`IMAGE: "${w}" is not a string in firmware/src/i18n.cpp`);
  }
}

// The list loader (GALLERY_ORDER up to loadAnimations), run against a stand-in
// DOM that only knows the example scripts and a localStorage of our own.
const uiStart = main.indexOf('const STORAGE_KEY');
const uiEnd = main.indexOf('let saveTimer');
if (uiStart < 0 || uiEnd < uiStart) throw new Error('list loader not found');
const loader = (saved = {}) => {
  const ls = new Map(Object.entries(saved));
  const storage = { getItem: k => (ls.has(k) ? ls.get(k) : null), setItem: (k, v) => ls.set(k, String(v)) };
  const doc = { querySelectorAll: () => examples.map(ex => ({
    textContent: '\n' + ex.code, dataset: { example: ex.name, orientation: ex.orientation } })) };
  const $ = () => ({ getContext: () => null });
  return new Function('document', 'localStorage', '$', main.slice(uiStart, uiEnd) +
    'return { GALLERY_ORDER, EXAMPLES_VERSION, STORAGE_KEY, VERSION_KEY, loadAnimations, animationsFromFile, store: () => ls };')(doc, storage, $);
};
const names = list => list.map(a => a.name);
const ui = loader();

// Every example has its place in the gallery order: indexOf() -1 would sort it
// first, and the simulator would open on it.
for (const ex of examples) {
  if (!ui.GALLERY_ORDER.includes(ex.name)) failures.push(`${ex.name}: missing from GALLERY_ORDER`);
}
const fresh = names(ui.loadAnimations());
if (fresh[0] !== ui.GALLERY_ORDER[0]) failures.push(`fresh list starts with ${fresh[0]}, not ${ui.GALLERY_ORDER[0]}`);
if (fresh.indexOf('IMAGE') !== fresh.indexOf('FOLLOWERS') + 1) failures.push('IMAGE: not right after FOLLOWERS in a fresh list');

// A list saved by version 17, the last one without IMAGE, is offered it at its place.
const v17 = ui.loadAnimations().filter(a => a.name !== 'IMAGE');
const upgraded = names(loader({ [ui.STORAGE_KEY]: JSON.stringify(v17), [ui.VERSION_KEY]: '17' }).loadAnimations());
if (upgraded.indexOf('IMAGE') !== upgraded.indexOf('FOLLOWERS') + 1) failures.push('IMAGE: not added after FOLLOWERS to a list saved by version 17');

// Examples saved before they went into both galleries become 'both'; an animation
// of the user's own (new, duplicate, import: never a fingerprint) keeps its
// orientation, even when it shares an example's name.
const seeded = ui.loadAnimations().find(a => a.name === 'PLASMA');
const mine = { name: 'IMAGE', code: 'return () => {};', orientation: 'landscape' };
const kept = loader({ [ui.STORAGE_KEY]: JSON.stringify([{ ...seeded, orientation: 'landscape' }, mine]),
                      [ui.VERSION_KEY]: String(ui.EXAMPLES_VERSION) }).loadAnimations();
if (kept[0].orientation !== 'both') failures.push(`PLASMA: a saved landscape copy stays ${kept[0].orientation}`);
if (kept[1].orientation !== 'landscape') failures.push(`IMAGE: a user animation of that name became ${kept[1].orientation}`);

// The product ships its own screens, Nimbus and generic examples only: no third-party
// character, meme, brand or logo. Fan art lives in a separate community pack that the
// user loads from a file (Open...), in the format the simulator itself exports.
const thirdParty = /\b(spotify|openai|xai|grok|codex|pepe|nyan|doge|among us|creeper|snorlax|pac-?man|tetris|dvd|this is fine)\b/i
  .exec(html);
if (thirdParty) failures.push(`third-party name in simulator.html: "${thirdParty[0]}"`);
if (!/<p class="help">[^<]*community[^<]*<\/p>/i.test(html)) failures.push('help: no line on loading community examples from a file');

// What Open... accepts: a pack (array) or a single animation, in the current or the
// French format; an entry without a usable orientation takes the current gallery's.
const pack = JSON.parse(JSON.stringify([
  { name: 'A', code: 'function anim() {}', orientation: 'portrait' },
  { nom: 'B', code: 'function anim() {}', orientation: 'deux' },
  { name: 'C', code: 'function anim() {}' },
  { name: 'no code' },
]));
const loaded = ui.animationsFromFile(pack, 'landscape');
if (names(loaded).join() !== 'A,B,C') failures.push(`pack: loaded ${names(loaded)}`);
else if (loaded.map(a => a.orientation).join() !== 'portrait,both,landscape') failures.push(`pack: orientations ${loaded.map(a => a.orientation)}`);
if (ui.animationsFromFile({ code: 'x' }, 'portrait')[0]?.name !== 'IMPORT') failures.push('pack: a single unnamed animation is not named IMPORT');
let empty = null;
try { ui.animationsFromFile([{ name: 'x' }], 'landscape'); } catch (e) { empty = e.message; }
if (empty !== 'no animation in this file') failures.push('pack: a file without animation is not refused');

console.log(`${examples.length} examples, ${FRAMES} frames each, ${failures.length} failure(s)`);
for (const f of failures) console.log('  FAILED  ' + f);
process.exit(failures.length ? 1 : 0);
