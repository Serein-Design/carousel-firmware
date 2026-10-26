#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-only
# shellcheck shell=sh
# Tests install.sh and setup.sh in a fake home, once per JSON tool: node, python3,
# and osascript on macOS. Each case runs from the sources, then as the panel
# serves it (embed.mjs's body behind a prelude, piped into sh). Nothing here
# talks to a panel or touches the real home.
#
#   sh tools/agent-bridge/test/test_install.sh
HERE=$(cd "$(dirname "$0")" && pwd)
INSTALL=$HERE/../install.sh
HOOK_SRC=${CAROUSEL_TEST_SRC:-$HERE/..}/hook.sh   # what the installer copies
TOKEN=0123456789abcdef0123456789abcdef            # a fake token, never a real one
NAME=carousel-695c
IP=192.168.1.20
PY=$(command -v python3)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
unset XDG_CONFIG_HOME CODEX_HOME CAROUSEL_TOKEN CAROUSEL_IP CAROUSEL_NAME CAROUSEL_HOST
EMBED=$HERE/../embed.mjs
failures=0
n=0
MODE=source
IPATH='' ICODEX=''   # a case's own PATH and CODEX_HOME

check() { if eval "$1"; then echo "  ok      $2"; else echo "  FAILED  $2"; failures=$((failures + 1)); fi; }

# The runs see only the base system and the JSON tools, never an agent installed
# on this machine: an agent is found by the folders and commands a case creates.
BIN=$TMP/bin
mkdir "$BIN"
for t in node python3 osascript; do
  p=$(command -v "$t") && ln -s "$p" "$BIN/$t"
done
BASEPATH=$BIN:/usr/bin:/bin
APPS=$TMP/Applications   # where the runs look for Cursor.app: never this machine's /Applications

# view.py: the test's own JSON reader, so the checks never trust the tool under test
cat > "$TMP/view.py" <<'EOF'
import json, sys
def load(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)
def ours(x):   # what the installer writes, not any carousel/hook.sh
    return ".config/carousel/hook.sh' " in json.dumps(x)
cmd = sys.argv[1]
if cmd == "entries":
    # one line per Carousel command: event, matcher, command, timeout
    for ev, groups in load(sys.argv[2]).get("hooks", {}).items():
        for g in groups:
            for h in g.get("hooks", [g]):
                if ours(h):
                    t = " timeout=%s" % h["timeout"] if "timeout" in h else ""
                    print("%s %s %s%s" % (ev, g.get("matcher", "-"), h["command"], t))
elif cmd == "kept":
    # every key and hook of the original is still there, with the same value
    a, b = load(sys.argv[2]), load(sys.argv[3])
    ha, hb = a.pop("hooks", {}), b.pop("hooks", {})
    for k in a:
        if a[k] != b.get(k):
            sys.exit(1)
    for ev in ha:
        mine = []
        for g in hb.get(ev, []):
            if "hooks" in g:
                g = dict(g, hooks=[h for h in g["hooks"] if not ours(h)])
                if not g["hooks"]:
                    continue
            elif ours(g):
                continue
            mine.append(g)
        if mine != ha[ev]:
            sys.exit(1)
elif cmd == "equal":
    sys.exit(load(sys.argv[2]) != load(sys.argv[3]))
elif cmd == "block":
    # the block printed for one agent parses and holds that many commands
    text = open(sys.argv[2], encoding="utf-8").read()
    start = text.index(sys.argv[3] + ": to do by hand")
    start = text.index("{", start)
    block, _ = json.JSONDecoder().raw_decode(text[start:])
    sys.exit(json.dumps(block).count(".config/carousel/hook.sh' ") != int(sys.argv[4]))
EOF
js() { "$PY" "$TMP/view.py" "$@"; }

# The Carousel commands each agent gets, sorted
want() {
  case $1 in
    claude) printf '%s\n' \
      'SessionStart startup|resume|clear|fork idle' 'UserPromptSubmit - thinking' \
      'PreToolUse AskUserQuestion|ExitPlanMode attention' 'PreToolUse ^(?!(AskUserQuestion|ExitPlanMode)$) tool' \
      'PermissionRequest - attention' 'PostToolUse - thinking' 'PostToolUseFailure - failed' \
      'StopFailure - error' 'Stop - done' 'Notification idle_prompt idle' \
      'Notification elicitation_dialog attention' 'SessionEnd - end' ;;
    codex) printf '%s\n' \
      'SessionStart startup|resume|clear|fork idle' 'Interrupt - idle' 'UserPromptSubmit - thinking' \
      'PostToolUse - thinking' 'PreToolUse - tool' 'PermissionRequest - attention' 'Stop - done' \
      'SessionEnd - end' ;;
    cursor) printf '%s\n' \
      'beforeSubmitPrompt - thinking' 'postToolUse - thinking' 'preToolUse - tool' \
      'postToolUseFailure - failed' 'stop - stop' 'sessionEnd - end' ;;
    gemini) printf '%s\n' \
      'SessionStart - idle' 'BeforeAgent - thinking' 'AfterTool - thinking' 'BeforeTool - tool' \
      'Notification - attention' 'AfterAgent - done' 'SessionEnd - end' ;;
  esac | while read -r ev m st; do
    t=; [ "$1" = cursor ] && t=' timeout=5'
    printf "%s %s sh '%s/hook.sh' %s %s%s\n" "$ev" "$m" "$C" "$1" "$st" "$t"
  done | sort
}
entries_ok() { js entries "$2" | sort > "$TMP/got" && want "$1" | cmp -s - "$TMP/got"; }

fresh() {   # a new fake home
  n=$((n + 1)); H=$TMP/home$n; mkdir "$H"; C=$H/.config/carousel
  AB=$TMP/agentbin$n; mkdir "$AB"; RUNPATH=$AB:$BASEPATH   # the agents' commands, once agents() makes them
  CL=$H/.claude/settings.json; CX=$H/.codex/hooks.json; CU=$H/.cursor/hooks.json; GE=$H/.gemini/settings.json
}
agents() {   # each agent installed: its folder, and its command on the PATH
  for d in .claude .codex .cursor .gemini; do mkdir -p "$H/$d"; done
  for c in claude codex cursor gemini; do printf '#!/bin/sh\n' > "$AB/$c"; chmod +x "$AB/$c"; done
}
fixtures() {   # each agent's file with the user's own keys and hooks
  agents
  cat > "$CL" <<'EOF'
{"model": "opus", "permissions": {"allow": ["Bash(ls:*)"], "deny": []},
 "hooks": {"PreToolUse": [{"matcher": "Bash", "hooks": [{"type": "command", "command": "~/bin/guard.sh", "timeout": 3},
                                                     {"type": "command", "command": "/opt/mycarousel/hook.sh check"}]}],
           "Stop": [{"hooks": [{"type": "command", "command": "say done"}]}], "PreCompact": [],
           "PostToolUse": [{"hooks": [{"type": "command", "command": "sh ~/src/carousel/hook.sh claude done"}]}]},
 "statusLine": {"type": "command", "command": "~/bin/status.sh"}, "ratio": 1.5, "name": "é ü"}
EOF
  cat > "$CX" <<'EOF'
{"description": "mine", "hooks": {"PreToolUse": [{"matcher": "Bash", "hooks": [{"type": "command", "command": "python3 ~/.codex/hooks/p.py"}]}]}}
EOF
  cat > "$CU" <<'EOF'
{"version": 1, "hooks": {"afterFileEdit": [{"command": "./hooks/format.sh"}], "stop": [{"command": "./audit.sh", "loop_limit": 10}],
 "preToolUse": [{"command": "/opt/mycarousel/hook.sh cursor tool"}]}}
EOF
  cat > "$GE" <<'EOF'
{"theme": "GitHub", "general": {"vimMode": true},
 "hooks": {"BeforeTool": [{"matcher": "write_file", "hooks": [{"name": "sec", "type": "command", "command": "$GEMINI_PROJECT_DIR/sec.sh", "timeout": 5000}]}]}}
EOF
  for f in "$CL" "$CX" "$CU" "$GE"; do cp "$f" "$f.orig"; done
}
# The installer, as this mode runs it: served, it is read from stdin as from
# curl, away from the sources, with only the prelude's token, address and name
inst() {
  if [ $MODE = served ]; then
    (cd "$TMP" && HOME=$H PATH=${IPATH:-$RUNPATH} CAROUSEL_TEST_APPS=$APPS CODEX_HOME=$ICODEX CAROUSEL_TEST_JSON=$J \
      $SH -s -- "$@" < "$SERVED") > "$TMP/out" 2> "$TMP/err"; rc=$?
  else
    HOME=$H PATH=${IPATH:-$RUNPATH} CAROUSEL_TEST_APPS=$APPS CODEX_HOME=$ICODEX CAROUSEL_TEST_JSON=$J CAROUSEL_TOKEN=$TOKEN \
      CAROUSEL_IP=$IP CAROUSEL_NAME=$NAME $SH "$INSTALL" "$@" > "$TMP/out" 2> "$TMP/err"; rc=$?
  fi
}
setup() { HOME=$H PATH=$RUNPATH CAROUSEL_TEST_APPS=$APPS CAROUSEL_TEST_JSON=$J $SH "$C/setup.sh" "$@" > "$TMP/out" 2> "$TMP/err"; rc=$?; }
said() { grep -qxF "$1" "$TMP/out"; }
mode() { ls -ld "$1" | cut -c1-10; }
snapshot() { find "$H" -type f -exec cksum {} + | sort > "$1"; }

cases() {
  echo "- $MODE, $J ($SH): the four agents"
  fresh; agents
  inst
  check "[ $rc = 0 ]" "exit 0"
  for a in 'Claude Code' Codex Cursor 'Gemini CLI'; do
    check "said '$a: installed'" "$a: installed"
  done
  check "said 'Codex: review the new hooks once in Codex (/hooks)'" "the Codex review line"
  check "entries_ok claude $CL" "Claude Code gets its hooks"
  check "entries_ok codex $CX" "Codex gets its hooks"
  check "entries_ok cursor $CU" "Cursor gets its hooks, with a timeout"
  check "entries_ok gemini $GE" "Gemini CLI gets its hooks"
  check "\"$PY\" -c 'import json,sys; sys.exit(json.load(open(sys.argv[1])).get(\"version\") != 1)' $CU" "Cursor's file has version 1"
  check "[ \"\$(cat $C/panels/$NAME/header)\" = 'X-Hook-Token: $TOKEN' ]" "the token goes in the header file"
  check "[ \"\$(cat $C/panels/$NAME/ip)\" = $IP ] && [ \"\$(cat $C/default)\" = $NAME ]" "the address and the followed panel"
  check "cmp -s $C/hook.sh $HOOK_WANT && cmp -s $C/setup.sh $SETUP_WANT" "the hook and setup.sh are copied"
  check "[ \"\$(mode $C)\" = drwx------ ] && [ \"\$(mode $C/panels/$NAME)\" = drwx------ ]" "folders are 0700"
  check "[ \"\$(mode $C/panels/$NAME/header)\" = -rw------- ]" "the header file is 0600"
  check "! grep -rqs $TOKEN $H/.claude $H/.codex $H/.cursor $H/.gemini" "no token in any agent file"
  check "[ ! -e $CL.carousel-backup ]" "no backup of a file that did not exist"
  inst; inst --uninstall
  check "[ $rc = 0 ] && [ -z \"\$(find $H/.claude $H/.codex $H/.cursor $H/.gemini -type f)\" ]" "uninstall deletes the files it created, and their backups"

  echo "- $MODE, $J ($SH): the user's keys and hooks are kept"
  fresh; fixtures
  inst
  check "[ $rc = 0 ]" "exit 0"
  for f in "$CL" "$CX" "$CU" "$GE"; do
    check "js kept $f.orig $f" "${f#"$H"/} keeps its other keys and hooks"
  done
  check "entries_ok claude $CL && entries_ok cursor $CU" "ours are added next to them"
  for f in "$CL" "$CX" "$CU" "$GE"; do
    check "cmp -s $f.orig $f.carousel-backup" "${f#"$H"/} backed up first"
  done

  echo "- $MODE, $J ($SH): a second run changes nothing"
  cp "$TMP/out" "$TMP/out1"; snapshot "$TMP/snap1"
  inst
  snapshot "$TMP/snap2"
  check "cmp -s $TMP/snap1 $TMP/snap2" "every file byte-identical"
  check "cmp -s $TMP/out1 $TMP/out" "the same lines printed"

  echo "- $MODE, $J ($SH): a run that changes nothing keeps the file's own text"
  "$PY" -c 'import json,sys; d=json.load(open(sys.argv[1])); d["theme"]="dark"; json.dump(d, open(sys.argv[1], "w"))' "$CL"
  cp "$CL" "$TMP/mine"
  setup install
  check "cmp -s $TMP/mine $CL" "the user's compact file, already holding ours, byte-identical"
  check "cmp -s $CL.orig $CL.carousel-backup" "and the backup untouched"

  echo "- $MODE, $J ($SH): each change backs the file up first, and replaces it whole"
  "$PY" -c 'import json,sys; d=json.load(open(sys.argv[1])); d["hooks"].pop("SessionEnd"); json.dump(d, open(sys.argv[1], "w"), indent=4)' "$CL"
  cp "$CL" "$TMP/mine"; chmod 644 "$CL"; ino=$(ls -i "$CL" | awk '{print $1}')
  setup install
  check "entries_ok claude $CL && grep -q dark $CL" "our missing hook is back, the user's change kept"
  check "cmp -s $TMP/mine $CL.carousel-backup" "the backup is the file just before this change"
  check "[ \"\$(ls -i $CL | awk '{print \$1}')\" != $ino ]" "the file is replaced by a move, never rewritten in place"
  check "[ \"\$(mode $CL)\" = -rw-r--r-- ] && [ \"\$(mode $CL.carousel-backup)\" = -rw------- ]" "the file keeps its mode, the backup is 0600"
  check "grep -q '^    \"model\"' $CL" "in the file's own indent"
  check "[ -z \"\$(find $H -name '*.carousel-tmp' -o -name '*.carousel-new')\" ]" "no temporary file left"

  echo "- $MODE, $J ($SH): uninstall"
  inst --uninstall
  check "[ $rc = 0 ]" "exit 0"
  check "said 'Claude Code: removed' && said 'Gemini CLI: removed'" "one line per agent"
  check "[ ! -e $C ]" "the Carousel folder is gone"
  check "js equal $CX.orig $CX && js equal $CU.orig $CU && js equal $GE.orig $GE" "the rest is as before"
  check "! grep -rqs \".config/carousel/hook.sh' \" $CL $CX $CU $GE" "none of ours is left"
  check "grep -q /opt/mycarousel/hook.sh $CL && grep -q /opt/mycarousel/hook.sh $CU && grep -q src/carousel/hook.sh $CL" "a hook of the user's that runs another carousel/hook.sh stays"
  check "grep -q '\"PreCompact\": \[\]' $CL" "the user's own empty event list stays"
  check "said \"  left $CL.carousel-backup, the file before the last change\"" "it says which backups it leaves"
  fresh; fixtures
  inst; setup uninstall
  check "[ $rc = 0 ] && [ ! -e $C ]" "setup.sh uninstall: exit 0, the folder gone"
  check "js equal $CL.orig $CL && js equal $CX.orig $CX && js equal $CU.orig $CU && js equal $GE.orig $GE" "setup.sh uninstall: the rest as before"
  if [ $MODE = source ]; then
    fresh; fixtures
    HOME=$H PATH=$RUNPATH CAROUSEL_TEST_JSON=$J $SH "$INSTALL" --uninstall > "$TMP/out" 2>&1; rc=$?
    check "[ $rc = 0 ] && js equal $CL.orig $CL" "--uninstall needs no token and no panel"
  fi

  echo "- $MODE, $J ($SH): an old Claude Code install"
  fresh; agents; mkdir "$H/.claude/hooks"
  printf '#!/bin/bash\n# SPDX-License-Identifier: AGPL-3.0-only\n# Claude Code hook -> Carousel LED panel.\nexit 0\n' > "$H/.claude/hooks/carousel.sh"
  cat > "$CL" <<'EOF'
{"hooks": {"SessionStart": [{"matcher": "startup|resume|clear|fork", "hooks": [{"type": "command", "command": "bash ~/.claude/hooks/carousel.sh veille", "timeout": 5}]}],
           "Stop": [{"hooks": [{"type": "command", "command": "bash ~/.claude/hooks/carousel.sh fini", "timeout": 5}, {"type": "command", "command": "say done"}]}]}}
EOF
  inst
  check "! grep -q 'hooks/carousel.sh' $CL" "the old entries are gone"
  check "grep -q 'say done' $CL && entries_ok claude $CL" "the user's hook stays, ours are in"
  check "[ ! -e $H/.claude/hooks/carousel.sh ]" "the old script we shipped is deleted"
  fresh; agents; mkdir -p "$H/.claude/hooks" "$H/proj/.claude"
  printf '#!/bin/bash\n# Claude Code hook -> Carousel LED panel.\nexit 0\n' > "$H/.claude/hooks/carousel.sh"
  printf '{"projects": {\n  "%s/proj": {"allowedTools": []}}}\n' "$H" > "$H/.claude.json"
  printf '{"hooks": {"Stop": [{"hooks": [{"type": "command", "command": "bash ~/.claude/hooks/carousel.sh fini"}]}]}}\n' > "$H/proj/.claude/settings.local.json"
  printf '{"hooks": {"Stop": [{"hooks": [{"type": "command", "command": "bash ~/.claude/hooks/carousel.sh fini"}]}]}}\n' > "$CL"
  inst
  check "[ -e $H/.claude/hooks/carousel.sh ] && entries_ok claude $CL" "kept while a project's settings still run it"
  check "said \"  kept $H/.claude/hooks/carousel.sh: $H/proj/.claude/settings.local.json still runs it\"" "and it says which one"
  fresh; agents; mkdir "$H/.claude/hooks"
  printf '#!/bin/sh\necho mine\n' > "$H/.claude/hooks/carousel.sh"
  inst
  check "[ -e $H/.claude/hooks/carousel.sh ]" "a carousel.sh of the user's own is kept"

  echo "- $MODE, $J ($SH): not found, found by its command, CODEX_HOME"
  fresh; mkdir "$H/.claude" "$TMP/cmd$n"
  printf '#!/bin/sh\n' > "$TMP/cmd$n/codex"; chmod +x "$TMP/cmd$n/codex"
  IPATH=$TMP/cmd$n:$RUNPATH; inst; IPATH=
  check "said 'Claude Code: not found' && said 'Cursor: not found' && said 'Gemini CLI: not found'" "not found lines"
  check "said 'Codex: installed' && entries_ok codex $CX" "codex on the PATH: found, its folder made"
  check "[ ! -e $H/.cursor ] && [ ! -e $H/.gemini ] && [ ! -e $CL ]" "nothing made for an agent not found, a folder alone included"
  fresh; printf '#!/bin/sh\n' > "$AB/cursor-agent"; chmod +x "$AB/cursor-agent"
  inst
  check "said 'Cursor: installed' && entries_ok cursor $CU" "cursor-agent on the PATH is Cursor"
  fresh; mkdir -p "$APPS/Cursor.app"
  inst; rm -rf "$APPS/Cursor.app"
  check "said 'Cursor: installed' && entries_ok cursor $CU" "Cursor.app in /Applications is Cursor"

  echo "- $MODE, $J ($SH): only a folder, hooks left from before"
  fresh; fixtures
  inst
  rm -f "$AB"/*   # the agents are gone, their folders and our hooks stay
  inst
  check "[ $rc = 0 ]" "exit 0"
  for a in 'Claude Code' Codex Cursor 'Gemini CLI'; do
    check "said '$a: not found, its Carousel hooks removed'" "$a: our hooks removed"
  done
  check "js equal $CL.orig $CL && js equal $CX.orig $CX && js equal $CU.orig $CU && js equal $GE.orig $GE" "the user's own hooks and keys are kept"
  check "grep -q /opt/mycarousel/hook.sh $CL && grep -q src/carousel/hook.sh $CL" "another carousel/hook.sh stays"
  check "said \"  left $CL.carousel-backup, the file before the last change\"" "it says which backup it leaves"
  inst
  check "[ $rc = 0 ] && said 'Gemini CLI: not found' && ! grep -q removed $TMP/out" "a second run has nothing left to remove"
  fresh; agents
  inst
  rm -f "$AB"/*
  inst
  check "[ $rc = 0 ] && said 'Codex: not found, its Carousel hooks removed'" "files the installer made: hooks removed"
  check "[ -z \"\$(find $H/.claude $H/.codex $H/.cursor $H/.gemini -type f)\" ]" "and the files with them, and their backups"
  fresh; mkdir "$H/codexhome"; printf '#!/bin/sh\n' > "$AB/codex"; chmod +x "$AB/codex"
  ICODEX=$H/codexhome; inst; ICODEX=
  check "entries_ok codex $H/codexhome/hooks.json && [ ! -e $H/.codex ]" "CODEX_HOME is followed"

  echo "- $MODE, $J ($SH): an agent file that is not valid JSON"
  fresh; fixtures
  printf '{"theme": "GitHub",\n' > "$GE"; cp "$GE" "$GE.orig"
  inst
  check "[ $rc = 1 ]" "exit 1"
  check "cmp -s $GE.orig $GE && [ ! -e $GE.carousel-backup ]" "the file is left untouched"
  check "js block $TMP/out 'Gemini CLI' 7" "its block is printed, ready to paste"
  check "said 'Claude Code: installed' && said 'Cursor: installed'" "the others go on"

  echo "- $MODE, $J ($SH): an agent file behind a symlink"
  fresh; fixtures
  mv "$CL" "$H/dotfiles.json"; ln -s "$H/dotfiles.json" "$CL"
  inst
  check "[ -L $CL ] && entries_ok claude $H/dotfiles.json" "written through the link, the link kept"

  [ $MODE = source ] || return 0   # the panel serves only a valid prelude
  echo "- $MODE, $J ($SH): refused without a valid token"
  for bad in '' 0123456789abcdef0123456789abcde 0123456789abcdef0123456789abcdef0 0123456789abcdeF0123456789abcdef; do
    fresh; fixtures
    HOME=$H PATH=$RUNPATH CAROUSEL_TEST_JSON=$J CAROUSEL_TOKEN=$bad CAROUSEL_IP=$IP CAROUSEL_NAME=$NAME \
      $SH "$INSTALL" > "$TMP/out" 2> "$TMP/err"; rc=$?
    check "[ $rc = 1 ] && [ \$(wc -l < $TMP/err) = 1 ] && [ ! -s $TMP/out ]" "token '$bad': one line on stderr, exit 1"
    check "[ ! -e $H/.config ] && js equal $CL.orig $CL && [ ! -e $CL.carousel-backup ]" "token '$bad': nothing written"
  done
  fresh; agents
  HOME=$H PATH=$RUNPATH CAROUSEL_TEST_JSON=$J CAROUSEL_TOKEN=$TOKEN CAROUSEL_IP=$IP CAROUSEL_NAME="../x" \
    $SH "$INSTALL" > "$TMP/out" 2>&1; rc=$?
  check "[ $rc = 1 ] && [ ! -e $H/.config ]" "a panel name outside [a-z0-9-] refused"
}

# The served script, and the hook and setup.sh it carries
SERVED=$TMP/served.sh
printf "#!/bin/sh\nCAROUSEL_TOKEN='%s'\nCAROUSEL_IP='%s'\nCAROUSEL_NAME='%s'\n" "$TOKEN" "$IP" "$NAME" > "$SERVED"
node "$EMBED" --print >> "$SERVED"
served() { sed -n "/<<'$1'\$/,/^$1\$/p" "$SERVED" | sed '1d;$d'; }
served CAROUSEL_EOF_HOOK > "$TMP/served-hook.sh"
served CAROUSEL_EOF_SETUP > "$TMP/served-setup.sh"
# A source's commands: what is left without its comment and blank lines
commands() { awk 'NR == 1 && /^#!/ || /^[ \t]*# shellcheck / || !/^[ \t]*(#|$)/' "$1"; }
# The same lines, give or take indentation and a comment at the end
flat() { sed -e 's/^[[:space:]]*//' -e 's/[[:space:]][[:space:]]*#[[:space:]].*$//' "$@"; }
commands_kept() { commands "$1" | flat > "$TMP/a"; flat "$2" > "$TMP/b"; cmp -s "$TMP/a" "$TMP/b"; }

echo "- the script the panel serves"
check "node '$EMBED' --check" "agent_scripts.cpp matches the sources (embed.mjs --check)"
check "[ \$(gzip -9 -c $SERVED | wc -c) -le 16384 ]" "compressed, as the panel holds it, it fits in 16 KB"
check "[ \"\$(tail -n 1 $SERVED)\" = 'main \"\$@\"' ]" "its last line is the only call"
check "! grep -q 'CAROUSEL_TEST_SRC\|dirname' $SERVED" "it reads no file beside it"
check "[ -s $TMP/served-hook.sh ] && sh -n $TMP/served-hook.sh && commands_kept $HERE/../hook.sh $TMP/served-hook.sh" \
  "it carries hook.sh, less its comments"
check "[ -s $TMP/served-setup.sh ] && sh -n $TMP/served-setup.sh && commands_kept $HERE/../setup.sh $TMP/served-setup.sh" \
  "it carries setup.sh, less its comments"
# curl | sh runs what has arrived: cut short, the script defines functions and
# never calls them
size=$(wc -c < "$SERVED")
for cut in 40 100 3000 8000 $((size - 40)) $((size - 3)); do
  fresh; fixtures
  head -c "$cut" "$SERVED" > "$TMP/cut.sh"
  (cd "$TMP" && HOME=$H PATH=$RUNPATH CAROUSEL_TEST_JSON=node sh < "$TMP/cut.sh") > /dev/null 2>&1
  check "[ ! -e $H/.config ] && js equal $CL.orig $CL && js equal $GE.orig $GE" "cut after $cut bytes: nothing written"
done

for MODE in source served; do
  if [ $MODE = served ]; then HOOK_WANT=$TMP/served-hook.sh SETUP_WANT=$TMP/served-setup.sh
  else HOOK_WANT=$HOOK_SRC SETUP_WANT=$HERE/../setup.sh; fi
  for J in node python3 osascript; do
    if [ ! -e "$BIN/$J" ]; then echo "- $J: not on this machine, skipped"; continue; fi
    SH="sh"; cases
  done
done
MODE=source

echo "- none: no JSON tool"
SH="sh"; J=none
fresh; fixtures
inst
check "[ $rc = 1 ]" "exit 1"
check "cmp -s $CL.orig $CL && cmp -s $CX.orig $CX && cmp -s $CU.orig $CU && cmp -s $GE.orig $GE" "no agent file changed"
check "js block $TMP/out 'Claude Code' 12 && js block $TMP/out Codex 8 && js block $TMP/out Cursor 6 && js block $TMP/out 'Gemini CLI' 7" "every block printed"
check "[ -s $C/hook.sh ] && [ -s $C/panels/$NAME/header ]" "the hook is still set up, for the pasted blocks"

echo "- the JSON tool found by itself"
SH="sh"; J=
fresh; fixtures
inst
check "[ $rc = 0 ] && entries_ok claude $CL && js kept $CL.orig $CL" "this machine's first tool does the merge"
# python3 first: it keeps every number as written
mkdir "$TMP/fake$n"
printf '#!/bin/sh\ntouch "%s"\nexit 1\n' "$TMP/fake$n/ran" > "$TMP/fake$n/python3"
chmod +x "$TMP/fake$n/python3"
F=$TMP/fake$n
fresh; fixtures
HOME=$H PATH=$F:$RUNPATH CAROUSEL_TOKEN=$TOKEN CAROUSEL_IP=$IP CAROUSEL_NAME=$NAME \
  sh "$INSTALL" > "$TMP/out" 2>&1; rc=$?
check "[ -e $F/ran ] && [ $rc = 1 ] && cmp -s $CL.orig $CL" "python3 is tried before node and osascript"
big() { "$PY" -c 'import json,sys; sys.exit(json.load(open(sys.argv[1])).get("big") != 12345678901234567890)' "$1"; }
if [ "$(uname -s)" = Darwin ] && [ -x /usr/bin/python3 ] && [ -x /usr/bin/osascript ] && xcode-select -p > /dev/null 2>&1; then
  # On a Mac without the developer tools, /usr/bin/python3 is a stub that
  # opens an install dialog: skipped, osascript does the merge (and rounds)
  mkdir "$TMP/noclt"
  printf '#!/bin/sh\nexit 2\n' > "$TMP/noclt/xcode-select"; chmod +x "$TMP/noclt/xcode-select"
  for clt in yes no; do
    fresh; agents; printf '{"big": 12345678901234567890}\n' > "$CL"
    p=$AB:/usr/bin:/bin; [ $clt = no ] && p=$TMP/noclt:$p
    HOME=$H PATH=$p CAROUSEL_TOKEN=$TOKEN CAROUSEL_IP=$IP CAROUSEL_NAME=$NAME sh "$INSTALL" > "$TMP/out" 2>&1
    if [ $clt = yes ]; then check "entries_ok claude $CL && big $CL" "macOS with the developer tools: /usr/bin/python3, the number kept"
    else check "entries_ok claude $CL && ! big $CL" "macOS without them: the python3 stub skipped, osascript merges"; fi
  done
fi

echo "- none: uninstall without a JSON tool"
J=node; fresh; fixtures; inst
J=none; setup uninstall
check "[ $rc = 1 ] && said 'Claude Code: to do by hand, remove the hooks that run $C/hook.sh from $CL'" "to do by hand"
check "[ -s $C/hook.sh ] && grep -q '^Carousel: $C kept, as ' $TMP/out" "the folder stays, as the agents still run its hook.sh, and it says so"

echo "- a write that fails leaves the file whole"
J=node
mkdir "$TMP/failmv"
printf '#!/bin/sh\ncase "$*" in *.carousel-new*) exit 1 ;; esac\nexec /bin/mv "$@"\n' > "$TMP/failmv/mv"
chmod +x "$TMP/failmv/mv"
fresh; fixtures
IPATH=$TMP/failmv:$RUNPATH; inst; IPATH=
check "[ $rc = 1 ] && cmp -s $CL.orig $CL && cmp -s $GE.orig $GE" "the move fails: every agent file as it was"
check "cmp -s $CL.orig $CL.carousel-backup" "the backup holds it too"
check "[ -z \"\$(find $H -name '*.carousel-tmp' -o -name '*.carousel-new')\" ]" "no temporary file left"
fresh; fixtures; chmod 444 "$CL"
inst
check "[ $rc = 1 ] && cmp -s $CL.orig $CL" "a file made read-only is left alone"
chmod 644 "$CL"

if [ -e "$BIN/python3" ]; then
  echo "- python3 keeps every number"
  J=python3; fresh; agents; printf '{"big": 12345678901234567890, "f": 1.5}\n' > "$CL"
  inst
  check "entries_ok claude $CL && big $CL" "an integer past 2^53 kept"
fi

if command -v dash > /dev/null; then
  J=node; SH=dash
  echo "- dash"
  fresh; fixtures
  inst
  check "[ $rc = 0 ] && entries_ok gemini $GE && js kept $GE.orig $GE" "install under dash"
  inst --uninstall
  check "[ $rc = 0 ] && [ ! -e $C ] && js equal $CL.orig $CL" "uninstall under dash"
fi

echo
echo "$failures failure(s)"
[ "$failures" -eq 0 ]
