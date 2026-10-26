#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-only
# shellcheck shell=sh
# Sets up, or removes, the Carousel hooks of the coding agents found on this
# computer. install.sh writes it next to hook.sh; it also runs on its own:
#   sh ~/.config/carousel/setup.sh install | uninstall
#
# POSIX sh cannot edit JSON safely, so the merge borrows the first JSON tool
# present: python3, then node, then JavaScript through osascript (every Mac).
# python3 first, as it keeps every number as written. On a Mac without the
# developer tools, /usr/bin/python3 is a stub that opens an install dialog:
# skipped there. With no tool, an agent's file is left alone and the block to
# paste is printed instead.
#
# Every run removes all of ours (see JS below), then adds the current set: a
# rerun leaves each file byte-identical, and an old install is migrated. The
# agent files hold only the command, never the token: agent settings often
# end up in a dotfiles repository.

C=${XDG_CONFIG_HOME:-$HOME/.config}/carousel

# The events each agent reports, one per line: event, state, matcher. The
# SessionStart matchers leave out "compact": compaction happens mid-task, and
# would put the mascot to sleep while the agent keeps working. Claude Code runs
# every matching group at once, so its two PreToolUse groups exclude each other,
# or the arrival order of two requests would decide the state. Gemini CLI takes
# exact strings as matchers on these events, hence none there.
table() {
  case $1 in
    claude) printf '%s\n' 'SessionStart idle startup|resume|clear|fork' 'UserPromptSubmit thinking' \
      'PreToolUse attention AskUserQuestion|ExitPlanMode' 'PreToolUse tool ^(?!(AskUserQuestion|ExitPlanMode)$)' \
      'PermissionRequest attention' 'PostToolUse thinking' 'PostToolUseFailure failed' 'StopFailure error' \
      'Stop done' 'Notification idle idle_prompt' 'Notification attention elicitation_dialog' 'SessionEnd end' ;;
    codex) printf '%s\n' 'SessionStart idle startup|resume|clear|fork' 'Interrupt idle' 'UserPromptSubmit thinking' \
      'PostToolUse thinking' 'PreToolUse tool' 'PermissionRequest attention' 'Stop done' 'SessionEnd end' ;;
    cursor) printf '%s\n' 'beforeSubmitPrompt thinking' 'postToolUse thinking' 'preToolUse tool' \
      'postToolUseFailure failed' 'stop stop' 'sessionEnd end' ;;
    gemini) printf '%s\n' 'SessionStart idle' 'BeforeAgent thinking' 'AfterTool thinking' 'BeforeTool tool' \
      'Notification attention' 'AfterAgent done' 'SessionEnd end' ;;
  esac
}

# The hooks of agent $1 as JSON, to merge or to paste. Cursor's entries are flat
# and carry a timeout, its default being undocumented; the others keep their
# agent's default.
block() {
  table "$1" | awk -v c="sh '$C/hook.sh' $1" -v a="$1" '
    BEGIN { printf "{%s\"hooks\": {\n", (a == "cursor" ? "\"version\": 1, " : "") }
    {
      if ($1 != e) { if (NR > 1) printf "],\n"; e = $1; printf "  \"%s\": [", e; s = "" } else s = ", "
      h = "\"command\": \"" c " " $2 "\""
      if (a == "cursor") printf "%s{%s, \"timeout\": 5}", s, h
      else printf "%s{%s\"hooks\": [{\"type\": \"command\", %s}]}", s, ($3 == "" ? "" : "\"matcher\": \"" $3 "\", "), h
    }
    END { printf "]\n}}\n" }'
}

label() {
  case $1 in claude) echo 'Claude Code' ;; codex) echo Codex ;; cursor) echo Cursor ;; gemini) echo 'Gemini CLI' ;; esac
}
file() {
  case $1 in
    claude) echo "$HOME/.claude/settings.json" ;;
    codex) echo "${CODEX_HOME:-$HOME/.codex}/hooks.json" ;;
    cursor) echo "$HOME/.cursor/hooks.json" ;;
    gemini) echo "$HOME/.gemini/settings.json" ;;
  esac
}
# Agent $1 is installed when its command is on the PATH (Cursor: cursor or
# cursor-agent, or its app on a Mac). Its config folder alone proves nothing:
# other tools make ~/.gemini, for one.
found() {
  if [ "$1" = cursor ]; then
    [ -d "${CAROUSEL_TEST_APPS-/Applications}/Cursor.app" ] && return
    set -- cursor-agent cursor
  fi
  for c; do command -v "$c" > /dev/null 2>&1 && return; done
  return 1
}
tool() {
  if [ -n "${CAROUSEL_TEST_JSON:-}" ]; then echo "$CAROUSEL_TEST_JSON"; return; fi
  for t in python3 node osascript; do
    command -v $t > /dev/null 2>&1 || continue
    if [ $t = python3 ] && [ "$(command -v python3)" = /usr/bin/python3 ] && [ "$(uname -s)" = Darwin ] &&
      ! xcode-select -p > /dev/null 2>&1; then continue; fi
    echo $t; return
  done
  echo none
}

# The merge, in JavaScript for osascript and node, and the same in Python:
# drop ours, add block $2 (none when removing), keep everything else. Ours is
# exactly what block() writes, sh '<this folder>/hook.sh' <agent> <state>, or
# what the old hooks-claude-code.json wrote, bash ~/.claude/hooks/carousel.sh
# <state>: any other command stays, even one that runs a carousel/hook.sh.
# An event's list is deleted only when it held ours and nothing is left.
# When the settings come out the same, the file's own text is kept, byte for
# byte. A changed file is written out again by the tool, in the file's own
# indent: lists on one line are spread out, of two equal keys the last one
# stays, and osascript and node round integers past 2^53 (python3 keeps them).
JS='function carousel(t, b, c) {
  var o = function (x) { return x !== null && typeof x == "object" && !Array.isArray(x); };
  var mine = "sh \x27" + c + "/hook.sh\x27 ", gone = 0;
  var ours = function (x) {
    var s = o(x) && typeof x.command == "string" ? x.command : "";
    return (s.indexOf(mine) === 0 && /^(claude|codex|cursor|gemini) [a-z]+$/.test(s.slice(mine.length))) ||
      /^bash ~\/\.claude\/hooks\/carousel\.sh [a-z]+$/.test(s);
  };
  var drop = function (l) {
    var k = l.filter(function (x) { return !ours(x); });
    gone += l.length - k.length;
    return k;
  };
  var d = /\S/.test(t) ? JSON.parse(t) : {};
  if (!o(d) || ("hooks" in d && !o(d.hooks))) throw new Error("not a settings object");
  var was = JSON.stringify(d), h = d.hooks || {};
  Object.keys(h).forEach(function (k) {
    if (!Array.isArray(h[k])) return;
    var before = gone;
    h[k] = drop(h[k]).filter(function (g) {
      if (!o(g) || !Array.isArray(g.hooks) || !g.hooks.length) return true;
      g.hooks = drop(g.hooks);
      return g.hooks.length;
    });
    if (gone > before && !h[k].length) delete h[k];
  });
  if (b) {
    b = JSON.parse(b);
    if (b.version && !("version" in d)) d.version = b.version;
    Object.keys(b.hooks).forEach(function (k) { h[k] = (h[k] || []).concat(b.hooks[k]); });
  }
  if (Object.keys(h).length) d.hooks = h; else if (gone) delete d.hooks;
  if (JSON.stringify(d) === was) return t;
  var i = /\n([ \t]+)"/.exec(t);
  return JSON.stringify(d, null, i ? i[1] : "  ") + "\n";
}'
NODE='
var fs = require("fs"), p = process.argv[1];
fs.writeFileSync(p, carousel(fs.readFileSync(p, "utf8"), process.argv[2], process.argv[3]));'
JXA='
function run(a) {
  var t = $.NSString.stringWithContentsOfFileEncodingError(a[0], 4, null);
  if (t.isNil() || !$(carousel(t.js, a[1], a[2])).writeToFileAtomicallyEncodingError(a[0], true, 4, null)) throw new Error("cannot rewrite");
}'
PY='import json, re, sys
p, b, c = sys.argv[1:4]
t = open(p, encoding="utf-8").read()
d = json.loads(t) if t.strip() else {}
o = lambda x: isinstance(x, dict)
mine = "sh \x27" + c + "/hook.sh\x27 "
def ours(x):
    s = x.get("command") if o(x) else None
    if not isinstance(s, str):
        return False
    if s.startswith(mine) and re.fullmatch(r"(claude|codex|cursor|gemini) [a-z]+", s[len(mine):]):
        return True
    return re.fullmatch(r"bash ~/\.claude/hooks/carousel\.sh [a-z]+", s) is not None
gone = 0
def drop(l):
    global gone
    k = [x for x in l if not ours(x)]
    gone += len(l) - len(k)
    return k
def keep(g):
    if not o(g) or not isinstance(g.get("hooks"), list) or not g["hooks"]:
        return True
    g["hooks"] = drop(g["hooks"])
    return bool(g["hooks"])
if not o(d) or not o(d.get("hooks", {})):
    sys.exit(1)
was = json.dumps(d)
h = d.get("hooks", {})
for k in list(h):
    if isinstance(h[k], list):
        before = gone
        h[k] = [g for g in drop(h[k]) if keep(g)]
        if gone > before and not h[k]:
            del h[k]
if b:
    b = json.loads(b)
    if "version" in b and "version" not in d:
        d["version"] = b["version"]
    for k, v in b["hooks"].items():
        h[k] = h.get(k, []) + v
if h:
    d["hooks"] = h
elif gone:
    d.pop("hooks", None)
if json.dumps(d) != was:
    i = re.search(r"\n([ \t]+)\"", t)
    open(p, "w", encoding="utf-8").write(json.dumps(d, indent=i.group(1) if i else "  ", ensure_ascii=False) + "\n")'

# The file behind $1, through its symbolic links
real() {
  r=$1 i=0
  while [ -L "$r" ]; do
    [ $i -lt 20 ] || return 1
    l=$(readlink "$r") || return 1
    case $l in /*) r=$l ;; *) r=${r%/*}/$l ;; esac
    i=$((i + 1))
  done
  echo "$r"
}

# Merges block $2 into file $1 through a copy, and sets changed when the file
# changed. The new text goes to a file beside the real one (a link's target:
# the link stays), with its mode, then is moved over it: the move is atomic,
# so a kill or a full disk leaves the old file whole. Each change first
# copies the file as it was to <file>.carousel-backup, the same way.
merge() {
  changed=
  r=$(real "$1") || return 1
  [ ! -e "$r" ] || [ -w "$r" ] || return 1   # made read-only: the user's call
  t=$r.carousel-tmp n=$r.carousel-new k=$1.carousel-backup
  if [ -e "$r" ]; then cat "$r" > "$t"; else : > "$t"; fi || { rm -f "$t"; return 1; }
  case $J in
    osascript) osascript -l JavaScript -e "$JS$JXA" "$t" "$2" "$C" ;;
    node) node -e "$JS$NODE" "$t" "$2" "$C" ;;
    python3) python3 -c "$PY" "$t" "$2" "$C" ;;
    *) false ;;
  esac > /dev/null 2>&1 || { rm -f "$t"; return 1; }
  if [ -e "$r" ]; then
    cmp -s "$t" "$r" && { rm -f "$t"; return 0; }
    { cp "$r" "$k.new" && mv -f "$k.new" "$k" && cp -p "$r" "$n"; } || { rm -f "$t" "$k.new" "$n"; return 1; }
  fi
  { cat "$t" > "$n" && mv -f "$n" "$r"; } || { rm -f "$t" "$n"; return 1; }
  rm -f "$t"
  changed=1
}

# The first settings file that still runs the old bash hook: the user's,
# this folder's, and each project's Claude Code knows (~/.claude.json)
oldCaller() {
  { printf '%s\n' "$HOME" "$PWD"
    grep -o '"/[^"]*"[[:space:]]*:' "$HOME/.claude.json" 2> /dev/null | sed 's/^"\(.*\)"[[:space:]]*:$/\1/'
  } | while IFS= read -r d; do
    for s in "$d/.claude/settings.json" "$d/.claude/settings.local.json"; do
      grep -qsF '.claude/hooks/carousel.sh' "$s" && { echo "$s"; break 2; }
    done
  done | grep .
}

# Removes ours from file $f: sets did when it changed, fails when it cannot. A
# file the installer created, with nothing else in it, goes with its backup.
strip() {
  did=
  grep -qs -e 'carousel/hook\.sh' -e '\.claude/hooks/carousel\.sh' "$f" || return 0
  merge "$f" "" || return 1
  [ -n "$changed" ] || return 0
  did=1
  if grep -qsxF "$f" "$C/created" && empty "$f"; then rm -f "$f" "$f.carousel-backup"; fi
}

# Nothing left but what the installer wrote: {} or Cursor's version
empty() {
  case $(tr -d ' \t\r\n' < "$1") in '{}' | '{"version":1}') return 0 ;; esac
  return 1
}

main() {
  umask 077
  case ${1:-} in install | uninstall) ;; *) echo "usage: sh setup.sh install | uninstall" >&2; return 2 ;; esac
  case $C in *[\"\'\\]*) echo "carousel: $C has a quote or a backslash in its path: nothing done" >&2; return 1 ;; esac
  J=$(tool) ok=0 calls=
  for a in claude codex cursor gemini; do
    f=$(file $a) l=$(label $a) did=
    if [ "$1" = uninstall ]; then
      if ! strip; then
        echo "$l: to do by hand, remove the hooks that run $C/hook.sh from $f"; ok=1 calls="$calls $f"
      elif [ -z "$did" ]; then
        echo "$l: nothing to remove"
      else
        echo "$l: removed"
      fi
      [ ! -e "$f.carousel-backup" ] || echo "  left $f.carousel-backup, the file before the last change"
    elif ! found $a; then
      # Not installed, but an earlier run may have left its hooks in the file
      if ! strip; then
        echo "$l: not found, to do by hand: remove the hooks that run $C/hook.sh from $f"; ok=1
      elif [ -n "$did" ]; then
        echo "$l: not found, its Carousel hooks removed"
        [ ! -e "$f.carousel-backup" ] || echo "  left $f.carousel-backup, the file before the last change"
      else
        echo "$l: not found"
      fi
    elif new=$( [ -e "$f" ] || echo 1 ) && b=$(block $a) && [ -n "$b" ] && mkdir -p "${f%/*}" && merge "$f" "$b"; then
      echo "$l: installed"; did=1
      [ -z "$new" ] || grep -qsxF "$f" "$C/created" || echo "$f" >> "$C/created"
      [ $a != codex ] || echo "Codex: review the new hooks once in Codex (/hooks)"
    else
      echo "$l: to do by hand"
      echo "  add these hooks to $f:"
      block $a; ok=1
    fi
    # The old bash hook: deleted only if it is the one we shipped and no
    # settings file still runs it
    o=$HOME/.claude/hooks/carousel.sh
    if [ $a = claude ] && [ -n "$did" ] && grep -qsxF '# Claude Code hook -> Carousel LED panel.' "$o"; then
      if s=$(oldCaller); then echo "  kept $o: $s still runs it"; else rm -f "$o"; fi
    fi
  done
  if [ "$1" = uninstall ]; then
    # The hooks that could not be removed still run hook.sh: it stays
    if [ -n "$calls" ]; then echo "Carousel: $C kept, as$calls still run its hook.sh"; else rm -rf "$C"; fi
  fi
  return $ok
}

main "$@"
