// SPDX-License-Identifier: AGPL-3.0-only
// The installer as the panel serves it, from its readable sources to the
// bytes in flash: install.sh, with each "# >>> embed <file>" ... "# <<< embed"
// region replaced by a quoted heredoc of that file, so the script arrives
// whole in one download. Every file loses its comment and blank lines (the
// shebang and the shellcheck lines stay); the result, compressed (raw
// deflate, level 9), goes into firmware/src/agent_scripts.cpp. The panel
// sends it as gzip behind its prelude (the token, the address, the name;
// agent_install.h), for "curl --compressed".
//
//   node tools/agent-bridge/embed.mjs           (after every edit of a script)
//   node tools/agent-bridge/embed.mjs --check   (exit 1 when the C++ file is stale)
//   node tools/agent-bridge/embed.mjs --print   (the served body on stdout)
//
// Development only, like tools/webcheck/assets.mjs: nothing here reaches the
// computers that run the command.
import { readFileSync, writeFileSync } from 'node:fs';
import { crc32, deflateRawSync, inflateRawSync } from 'node:zlib';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const OUT = resolve(HERE, '../../firmware/src/agent_scripts.cpp');
// The firmware's own ceiling on the compressed bytes (static_assert in the
// generated file): the panel's flash budget. Past it, the way out is to
// shorten the scripts.
const BUDGET = 16384;
const LITERAL_END = ')CAROUSEL"';

function fail(message) {
  console.error(`embed: ${message}`);
  process.exit(1);
}

// Reads one line from inside the given quote ('', "'" or '"'): the quote it
// leaves open for the next line, and where a comment starts (-1: none).
// Enough of sh's quoting for these scripts: backslash escapes, single and
// double quotes, and a "#" that starts a word begins a comment.
function scan(line, quote) {
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (quote === "'") { if (c === "'") quote = ''; continue; }
    if (c === '\\') { i++; continue; }
    if (quote === '"') { if (c === '"') quote = ''; continue; }
    if (c === "'" || c === '"') quote = c;
    else if (c === '#' && (i === 0 || /\s/.test(line[i - 1]))) return { quote, comment: i };
  }
  return { quote, comment: -1 };
}

// The lines of a script without its comments and blank lines: commented, the
// sources would take 3 KB more in flash, compressed. Indentation costs 132
// bytes there and keeps the installed files readable. A line inside a multi-line string is kept as it is (python3 reads
// its indentation), and may be neither blank nor start with "#": dropping it
// would change the string.
function strip(name, text) {
  const lines = text.split('\n');
  if (lines[lines.length - 1] === '') lines.pop();
  const out = [];
  let quote = '';
  lines.forEach((line, i) => {
    const dropped = /^\s*(#|$)/.test(line) &&
      !(i === 0 && line.startsWith('#!')) && !/^\s*# shellcheck /.test(line);
    const inside = quote;
    const read = scan(line, quote);
    quote = read.quote;
    if (inside) {
      if (dropped) fail(`${name}:${i + 1}: a comment or blank line inside a multi-line string`);
      out.push(line);
    } else if (!dropped) {
      const code = /^\s*#/.test(line) || read.comment < 0 ? line : line.slice(0, read.comment).trimEnd();
      if (/<<-?\s*['"]?\w/.test(code)) fail(`${name}:${i + 1}: a heredoc`);
      out.push(code);
    }
  });
  if (quote) fail(`${name}: a quote left open at the end`);
  return out;
}

// The served body: install.sh without its shebang (the prelude has one)
export function servedBody() {
  const read = (file) => readFileSync(join(HERE, file), 'utf8');
  const source = read('install.sh').split('\n');
  const kept = [];
  const regions = [];
  for (let i = 0; i < source.length; i++) {
    const m = source[i].match(/^\s*# >>> embed (\S+)$/);
    if (!m) { kept.push(source[i]); continue; }
    while (i < source.length && !/^\s*# <<< embed$/.test(source[i])) i++;
    if (i === source.length) fail(`install.sh: the region of ${m[1]} is not closed`);
    // A line strip() keeps as is, and that names no file of the scripts
    kept.push(`@embed ${regions.length}`);
    regions.push(m[1]);
  }
  const lines = strip('install.sh', kept.join('\n'));
  if (lines[0].startsWith('#!')) lines.shift();
  const body = lines.map((line) => {
    const m = line.match(/^@embed (\d+)$/);
    if (!m) return line;
    const file = regions[Number(m[1])];
    const text = read(file);
    const delimiter = `CAROUSEL_EOF_${file.replace(/\.sh$/, '').toUpperCase()}`;
    if (text.includes(delimiter)) fail(`${file} contains ${delimiter}`);
    return [`cat <<'${delimiter}'`, ...strip(file, text), delimiter].join('\n');
  }).join('\n') + '\n';
  if (body.includes(LITERAL_END)) fail(`a script contains ${LITERAL_END}`);
  if (/^@embed /m.test(body)) fail('a region left unreplaced');
  return body;
}

function cpp(body, gz) {
  const text = Buffer.from(body, 'utf8');
  const rows = [];
  for (let i = 0; i < gz.length; i += 16) {
    rows.push('  ' + [...gz.subarray(i, i + 16)].map((b) => `0x${b.toString(16).padStart(2, '0')}`).join(', ') + ',');
  }
  return `// SPDX-License-Identifier: AGPL-3.0-only
// Generated by tools/agent-bridge/embed.mjs; do not edit.
// tools/agent-bridge/install.sh as the panel serves it after the prelude,
// hook.sh and setup.sh inside it (${text.length} bytes, ${gz.length} compressed).
#include "agent_scripts.h"

const uint8_t INSTALL_SH_GZ[] PROGMEM = {
${rows.join('\n')}
};
const size_t INSTALL_SH_GZ_LEN = sizeof(INSTALL_SH_GZ);
const size_t INSTALL_SH_LEN = ${text.length};
const uint32_t INSTALL_SH_CRC = 0x${crc32(text).toString(16).padStart(8, '0')};
static_assert(sizeof(INSTALL_SH_GZ) <= ${BUDGET}, "the served installer is over its budget");
`;
}

// The bytes of INSTALL_SH_GZ as written in agent_scripts.cpp, when they
// inflate to the body. zlib's output may differ from one Node to the next:
// the bytes need only decode to the same script.
function committedBytes(text, body) {
  const m = text.match(/INSTALL_SH_GZ\[\][^=]*=\s*\{([^}]*)\}/);
  if (!m) return null;
  const gz = Buffer.from(m[1].split(',').map((x) => x.trim()).filter(Boolean).map((x) => Number(x)));
  try {
    return inflateRawSync(gz).equals(Buffer.from(body, 'utf8')) ? gz : null;
  } catch {
    return null;
  }
}

if (resolve(process.argv[1] || '') === fileURLToPath(import.meta.url)) {
  const body = servedBody();
  let gz = deflateRawSync(Buffer.from(body, 'utf8'), { level: 9, memLevel: 9 });
  const mode = process.argv[2] || '';
  if (mode === '--print') {
    process.stdout.write(body);
  } else if (mode === '--check') {
    let current = '';
    try { current = readFileSync(OUT, 'utf8'); } catch { /* missing: stale */ }
    const committed = committedBytes(current, body);
    if (committed) gz = committed;
    if (!committed || current !== cpp(body, gz)) fail('firmware/src/agent_scripts.cpp is stale: run node tools/agent-bridge/embed.mjs');
  } else if (mode) {
    fail(`unknown option ${mode}`);
  } else {
    writeFileSync(OUT, cpp(body, gz));
    console.log(`embed: install.sh served as ${Buffer.byteLength(body)} bytes, ${gz.length} compressed / ${BUDGET}`);
  }
  if (gz.length > BUDGET) fail(`the served script is ${gz.length} bytes compressed, over ${BUDGET}`);
}
