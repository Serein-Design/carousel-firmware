#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-only
# shellcheck shell=sh
# Tests tools/agent-bridge/hook.sh against a local fake panel, which is stopped
# and restarted to simulate Wi-Fi outages. Runs the hook with sh, then a short
# subset with dash when it is installed.
#
#   sh tools/agent-bridge/test/test_hook.sh
#   CAROUSEL_TEST_PORT=8199 sh tools/agent-bridge/test/test_hook.sh   # another port
HERE=$(cd "$(dirname "$0")" && pwd)
HOOK=$HERE/../hook.sh
PORT=${CAROUSEL_TEST_PORT:-8099}
WORK=$(mktemp -d)
RECEIVED=$WORK/received
: > "$RECEIVED"
# Fixed fake tokens: a test log never shows a real one
TOKEN_A=0123456789abcdef0123456789abcdef
TOKEN_B=fedcba9876543210fedcba9876543210
PY=$(command -v python3 || command -v python)
SH='sh'
WANT=                        # CAROUSEL_HOST: none, the default panel is followed
NAME=localhost               # the .local name the hook tries; absent.invalid: mDNS blocked
SHARED=absent.invalid        # carousel.local, the name a panel alone on its network takes
NET=127.0.0                  # this machine's /24: a scan never leaves it
GW=aa:bb:cc:00:00:01         # the gateway's hardware address; - when unreadable
SHA=                         # none: the hook finds no SHA-256 tool
SERVER=
failures=0

check() {
  if eval "$1"; then echo "  ok      $2"; else echo "  FAILED  $2"; failures=$((failures + 1)); fi
}
# Polls a condition for up to $2 seconds
await() {
  k=0
  while [ "$k" -lt $(($2 * 4)) ]; do
    eval "$1" && return 0
    sleep 0.25
    k=$((k + 1))
  done
  return 1
}

# A computer set up for one panel: its token and its address, as the
# installer writes them. X is the configuration home, D the runtime folder.
install_panel() {  # <name> <token> <ip>
  mkdir -p "$X/carousel/panels/$1"
  chmod 700 "$X/carousel"
  printf 'X-Hook-Token: %s\n' "$2" > "$X/carousel/panels/$1/header"
  chmod 600 "$X/carousel/panels/$1/header"
  printf '%s\n' "$3" > "$X/carousel/panels/$1/ip"
}
fresh() {
  X=$(mktemp -d "$WORK/home.XXXXXX")
  D=$(mktemp -d "$WORK/run.XXXXXX")
  install_panel carousel-695c "$TOKEN_A" 127.0.0.1
  printf 'carousel-695c\n' > "$X/carousel/default"
  R=$D/carousel-695c                   # the runtime folder of the followed panel
}

# Runs the hook as an agent does, the payload on stdin. Sets out (its stdout),
# status (its exit code) and last_ms (how long it held the agent): until its
# stdout closes, as an agent reads it, so a background process that keeps it
# open counts too. Read for 3 s at most: a held pipe fails the time checks
# instead of blocking the test.
TIMER='import os, select, subprocess, sys, time
t = time.monotonic()
p = subprocess.Popen(sys.argv[2:], stdout=subprocess.PIPE)
out = b""
while time.monotonic() - t < 3:
    if select.select([p.stdout], [], [], 0.05)[0]:
        b = os.read(p.stdout.fileno(), 4096)
        if not b:
            break
        out += b
open(sys.argv[1], "w").write("%d\n" % ((time.monotonic() - t) * 1000))
sys.stdout.buffer.write(out)
sys.exit(p.wait())'
hook() {  # <agent> <state> <payload>
  printf '%s' "$3" > "$WORK/in"
  hook_file "$1" "$2" "$WORK/in"
}
# shellcheck disable=SC2034  # out and status are read by check's eval
hook_file() {  # <agent> <state> <payload file>
  out=$(CAROUSEL_HOST=$WANT XDG_CONFIG_HOME=$X CAROUSEL_TEST_DIR=$D CAROUSEL_TEST_PORT=$PORT \
    CAROUSEL_TEST_NAME=$NAME CAROUSEL_TEST_SHARED=$SHARED CAROUSEL_TEST_NET=$NET CAROUSEL_TEST_GW=$GW CAROUSEL_TEST_SHA=$SHA \
    "$PY" -c "$TIMER" "$WORK/ms" "$SH" "$HOOK" "$1" "$2" < "$3")
  status=$?
  last_ms=$(cat "$WORK/ms")
}
claude() {  # <session> <folder>: a Windows path, as JSON escapes it
  printf '{"session_id":"%s","transcript_path":"C:\\\\x.jsonl","cwd":"C:\\\\Users\\\\x\\\\%s","hook_event_name":"x"}' "$1" "$2"
}
codex() {
  printf '{"session_id":"%s","cwd":"/home/u/%s","hook_event_name":"PreToolUse","transcript_path":"/t","model":"gpt-5","turn_id":"7"}' "$1" "$2"
}
gemini() {  # pretty-printed, over several lines
  printf '{\n  "session_id": "%s",\n  "cwd": "/Users/u/%s",\n  "hook_event_name": "BeforeTool",\n  "timestamp": "2026-10-24T10:00:00Z"\n}\n' "$1" "$2"
}
cursor() {  # <conversation> <folder> <status>: session_id differs on purpose
  printf '{"conversation_id":"%s","generation_id":"g1","hook_event_name":"stop","status":"%s","workspace_roots":["/Users/u/%s","/Users/u/other"],"cursor_version":"1.7","session_id":"not-this-one"}' "$1" "$3" "$2"
}

panel_on() {  # [token] [name] [noproof | as=<address> | busy | refuse]
  "$PY" "$HERE/fake_panel.py" "$PORT" "$RECEIVED" "$@" &
  SERVER=$!
  await "curl -s -o /dev/null -m 1 http://127.0.0.1:$PORT/id" 10
}
panel_off() { [ -n "$SERVER" ] && kill "$SERVER" 2>/dev/null; wait "$SERVER" 2>/dev/null; SERVER=; }
# What the panel received for a session: field 2 the state, 4 the timestamp,
# 5 the folder, then token-ok and path-has-32-hex
count() { grep -c " $1 " "$RECEIVED"; }
latest() { grep " $1 " "$RECEIVED" | tail -n 1 | cut -d' ' -f"$2"; }
flags() { grep " $1 " "$RECEIVED" | tail -n 1 | awk '{print $(NF - 1), $NF}'; }
# The followed panel's files, as the installer and earlier searches left them
P() { printf '%s\n' "$X/carousel/panels/carousel-695c/$1"; }
moved() { printf '127.0.0.2\n' > "$(P ip)"; }   # its cached address answers no more
# Lets a sender that still holds states give up, so it cannot send later
quiet() { rm -f "$R"/*.state; await "[ ! -d '$R/lock' ]" 12; }

panel_on "$TOKEN_A"

echo "- Normal send"
fresh
hook claude thinking "$(claude sess-a proj-a)"
check "[ $last_ms -lt 400 ]" "the agent gets control back in $last_ms ms"
check "[ $status -eq 0 ] && [ -z \"\$out\" ]" "Claude: exit 0, nothing on stdout"
check "await '[ \"\$(latest sess-a 2)\" = thinking ]' 5" "the panel receives thinking on /agent"
check "[ \"\$(flags sess-a)\" = '1 0' ]" "the token comes in the header, never in the address"
check "[ \"\$(latest sess-a 5)\" = proj-a ]" "the folder is the last segment of a Windows path"
check "! grep -rq $TOKEN_A '$D'" "no token in the runtime folder"
check "[ -f '$R/confirmed' ]" "the panel's /id confirmation is recorded"
# Every case below needs the hook to reach the fake panel: when it cannot,
# say why and stop, instead of waiting out each case
if [ "$(count sess-a)" -eq 0 ]; then
  echo "  the hook never reached the fake panel; its log:"
  sed 's/^/    /' "$R/log.txt" 2> /dev/null
  panel_off
  exit 1
fi
hook claude tool "$(claude sess-a proj-a)"
hook claude thinking "$(claude sess-a proj-a)"
hook claude tool "$(claude sess-a proj-a)"
check "await '[ \"\$(latest sess-a 2)\" = tool ]' 5" "three hooks in a row: the last one received is tool"
check "await '[ ! -d \"$R/lock\" ]' 5" "the sender finished and released the lock"
check "grep ' sess-a ' '$RECEIVED' | awk 'NR > 1 && \$4 <= p { exit 1 } { p = \$4 }'" "timestamps arrive strictly increasing"

echo "- Five events in one second"
fresh
ts=
for e in thinking tool thinking tool attention; do
  hook claude "$e" "$(claude sess-t proj-t)"
  ts="$ts $(cut -d' ' -f2 "$R/sess-t.state")"
done
# shellcheck disable=SC2086  # one word per timestamp
check "printf '%s\n' $ts | awk 'NR > 1 && \$1 <= p { exit 1 } { p = \$1 } END { exit NR != 5 }'" "five timestamps, each above the one before"
check "await '[ \"\$(latest sess-t 2)\" = attention ]' 5" "the last one is received"

echo "- Outage: states replace each other, only the current one is sent on return"
fresh
panel_off
before=$(count sess-b)
for e in thinking tool thinking attention; do hook claude "$e" "$(claude sess-b proj-b)"; done
check "[ $last_ms -lt 400 ]" "during the outage too, control comes back in $last_ms ms"
check "await 'grep -qs pending \"$R/log.txt\"' 6" "the log records the pending send"
panel_on "$TOKEN_A"
check "await '[ \"\$(latest sess-b 2)\" = attention ]' 12" "on return, the panel receives the current state"
sleep 2
check "[ \$((\$(count sess-b) - before)) -eq 1 ]" "a single request on return, not four"

echo "- Token renewed on the panel: its proof no longer matches"
fresh
install_panel carousel-695c "$TOKEN_B" 127.0.0.1
hook claude thinking "$(claude sess-r proj-r)"
check "await 'grep -qs \"127.0.0.1 answers with the panel.s name but not its token: nothing sent (token renewed? run the install command again)\" \"$R/log.txt\"' 6" "the log says to run the install command again"
check "[ \$(count sess-r) -eq 0 ]" "the old token is not sent"
check "[ \$(grep -c 'not its token' '$R/log.txt') -eq 1 ]" "said once for that address"
quiet

echo "- Token refused (renewed on the panel between its proof and the call)"
panel_off
panel_on "$TOKEN_A" carousel-695c refuse
fresh
hook claude thinking "$(claude sess-r2 proj-r2)"
check "await '[ \$(count sess-r2) -eq 1 ]' 5" "one request reaches the panel"
check "await 'grep -qs \"refused by the panel (token): run the install command again\" \"$R/log.txt\"' 5" "the log says to run the install command again"
sleep 3
check "[ \$(count sess-r2) -eq 1 ] && [ ! -d '$R/lock' ]" "no retry"
panel_off
panel_on "$TOKEN_A"

echo "- Another panel at the cached address"
panel_off
panel_on "$TOKEN_A" carousel-0a01
fresh
hook claude thinking "$(claude sess-n proj-n)"
check "await 'grep -qs pending \"$R/log.txt\"' 6" "its /id names another panel: pending"
check "[ \$(count sess-n) -eq 0 ]" "nothing reaches it, so the token is never sent there"
quiet
panel_off
panel_on "$TOKEN_A"

echo "- A host with the panel's name and MAC, but no proof of its token"
panel_off
panel_on "$TOKEN_A" carousel-695c noproof
fresh
hook claude thinking "$(claude sess-np proj-np)"
check "await 'grep -qs \"127.0.0.1 answers with the panel.s name but no proof, twice (panel busy): nothing sent\" \"$R/log.txt\"' 6" "asked twice, the log says the panel is busy"
check "! grep -qs 'token renewed' '$R/log.txt'" "not that the token was renewed"
check "[ \$(count sess-np) -eq 0 ]" "nothing reaches it, so the token is never sent there"
quiet

echo "- The panel busy: its first answer has no proof"
panel_off
panel_on "$TOKEN_A" carousel-695c busy
fresh
hook claude thinking "$(claude sess-bz proj-bz)"
check "await '[ \"\$(latest sess-bz 2)\" = thinking ]' 5" "asked again 300 ms later, it proves the token: sent"
check "! grep -qs 'busy\\|not its token' '$R/log.txt'" "and the log has no warning"
quiet

echo "- A relay at the cached address passes the nonce on to the panel elsewhere"
panel_off
panel_on "$TOKEN_A" carousel-695c as=192.168.1.50
fresh
hook claude thinking "$(claude sess-rl proj-rl)"
check "await 'grep -qs \"127.0.0.1 answers with the panel.s name but not its token: nothing sent\" \"$R/log.txt\"' 6" "its proof is for another address: refused"
check "[ \$(count sess-rl) -eq 0 ]" "nothing reaches it, so the token is never sent there"
quiet
panel_off
panel_on "$TOKEN_A" carousel-695c noproof

panel_off
panel_on "$TOKEN_A"

echo "- Without a SHA-256 tool, nothing is sent"
SHA=none
fresh
hook claude thinking "$(claude sess-ns proj-ns)"
hook claude end "$(claude sess-ns proj-ns)"
hook claude tool "$(claude sess-nt proj-nt)"
check "[ $status -eq 0 ] && [ $last_ms -lt 400 ]" "exit 0 in $last_ms ms"
sleep 2
check "[ \$(count sess-ns) -eq 0 ] && [ \$(count sess-nt) -eq 0 ]" "nothing reaches the panel, a name check alone is not enough"
check "[ \$(grep -c 'no SHA-256 tool (sha256sum, shasum or openssl): nothing sent; install one of them' '$R/log.txt') -eq 1 ]" "the log says once how to fix it"
SHA=
quiet

echo "- The real panel proves its token"
fresh
hook claude thinking "$(claude sess-pr proj-pr)"
check "await '[ \"\$(latest sess-pr 2)\" = thinking ]' 5" "sent after its proof"
check "! grep -qs 'not verified\\|not its token' '$R/log.txt'" "and the log has no warning"
quiet

echo "- CAROUSEL_HOST picks one of the installed panels"
fresh
install_panel carousel-0a01 "$TOKEN_B" 127.0.0.1
printf 'carousel-0a01\n' > "$X/carousel/default"
WANT=CAROUSEL-695C.local          # as the access point spells it
hook claude thinking "$(claude sess-h1 proj-h1)"
check "await '[ \"\$(latest sess-h1 2)\" = thinking ]' 5" "named in capitals with .local: sent"
check "[ \"\$(flags sess-h1)\" = '1 0' ]" "with that panel's token"
WANT=Carousel-695c
hook claude tool "$(claude sess-h2 proj-h2)"
check "await '[ \"\$(latest sess-h2 2)\" = tool ]' 5" "named without .local: sent"
WANT=carousel-ffff
hook claude thinking "$(claude sess-h3 proj-h3)"
sleep 2
check "[ \$(count sess-h3) -eq 0 ]" "naming no installed panel: nothing sent"
check "grep -qs 'names no installed panel' '$D/log.txt'" "and the log says why"
WANT=
R=$D/carousel-0a01
hook claude thinking "$(claude sess-h4 proj-h4)"
check "await 'grep -qs pending \"$R/log.txt\"' 6" "the default panel is not the one answering: pending"
check "[ \$(count sess-h4) -eq 0 ]" "one panel's token never goes to another"
quiet

echo "- Not installed"
X=$(mktemp -d "$WORK/home.XXXXXX"); D=$(mktemp -d "$WORK/run.XXXXXX")
hook claude thinking "$(claude sess-u proj-u)"
check "[ $status -eq 0 ] && [ -z \"\$out\" ]" "exit 0, nothing on stdout"
hook gemini thinking "$(gemini sess-u proj-u)"
check "[ \"\$out\" = '{}' ]" "Gemini still gets {}"
sleep 1
check "[ \$(count sess-u) -eq 0 ] && [ -z \"\$(ls -A '$D')\" ]" "nothing sent, nothing written"

echo "- End of session"
fresh
hook claude thinking "$(claude sess-f proj-f)"
await '[ "$(latest sess-f 2)" = thinking ]' 5
hook claude end "$(claude sess-f proj-f)"
check "[ \"\$(latest sess-f 2)\" = end ]" "end received at once, without the background sender"
check "[ $last_ms -lt 1000 ]" "in $last_ms ms"
check "[ ! -f '$R/sess-f.state' ]" "the session file is deleted"
hook claude thinking "$(claude sess-o proj-o)"
await '[ "$(latest sess-o 2)" = thinking ]' 5
await "[ ! -d '$R/lock' ]" 5
panel_off
hook claude end "$(claude sess-o proj-o)"
check "[ $last_ms -lt 1000 ]" "panel off: control back in $last_ms ms"
panel_on "$TOKEN_A"
check "await '[ \"\$(latest sess-o 2)\" = end ]' 12" "and end is sent on return"
check "await '[ ! -f \"$R/sess-o.state\" ]' 3" "then the session file is deleted"
fresh
hook claude end "$(claude sess-e proj-e)"
check "[ \"\$(latest sess-e 2)\" = end ] && [ $last_ms -lt 1000 ]" "never confirmed: proven first, then received at once, in $last_ms ms"
fresh
panel_off
hook claude end "$(claude sess-e2 proj-e2)"
check "[ $last_ms -lt 1000 ]" "never confirmed, panel off: control back in $last_ms ms"
check "[ ! -f '$R/sess-e2.state' ] && grep -qs 'end dropped (proj-e2): the panel did not prove its token in time' '$R/log.txt'" "the end is dropped, and the log says so"
panel_on "$TOKEN_A"
sleep 3
check "[ \$(count sess-e2) -eq 0 ]" "nothing goes to an address not proven"

echo "- Lock left behind by a killed sender"
fresh
mkdir -m 700 "$R"; mkdir "$R/lock"; printf '0' > "$R/lock/seen"
hook claude thinking "$(claude sess-d proj-d)"
check "await '[ \"\$(latest sess-d 2)\" = thinking ]' 5" "the stale lock is taken over and the state sent"

echo "- Sender already running: the hook only writes the state"
fresh
mkdir -m 700 "$R"; mkdir "$R/lock"; date +%s > "$R/lock/seen"
hook claude attention "$(claude sess-s proj-s)"
sleep 2
check "[ \$(count sess-s) -eq 0 ]" "no duplicate send"
check "[ -f '$R/sess-s.state' ]" "the state waits in its file"
rm -rf "$R/lock"

echo "- Runtime folder replaced by a symlink (shared /tmp): the hook does nothing"
fresh
TARGET=$(mktemp -d "$WORK/target.XXXXXX")
rmdir "$D"; ln -s "$TARGET" "$D"
hook claude thinking "$(claude sess-l proj-l)"
D=$(mktemp -d "$WORK/run.XXXXXX"); ln -s "$TARGET" "$D/carousel-695c"
hook claude thinking "$(claude sess-l proj-l)"
sleep 2
check "[ -z \"\$(ls -A '$TARGET')\" ]" "nothing written through either symlink"
check "[ \$(count sess-l) -eq 0 ]" "nothing sent"

echo "- Untrusted input"
fresh
head -c 3000 /dev/urandom > "$WORK/garbage"
hook_file claude thinking "$WORK/garbage"
check "[ $status -eq 0 ] && [ -z \"\$out\" ]" "garbage: exit 0"
hook_file gemini thinking "$WORK/garbage"
check "[ $status -eq 0 ] && [ \"\$out\" = '{}' ]" "garbage for Gemini: exit 0 and {}"
hook claude thinking '{"session_id":"sess-x'
check "[ $status -eq 0 ]" "truncated JSON: exit 0"
big() {  # <fields before> <fields after>: 300 KB of tool output between them
  printf '{%s,"tool_response":"' "$1"; head -c 300000 /dev/zero | tr '\0' 'a'; printf '"%s}' "$2"
}
big '"session_id":"sess-big","cwd":"/u/proj-big"' '' > "$WORK/big"
hook_file claude thinking "$WORK/big"
check "[ $status -eq 0 ] && [ $last_ms -lt 400 ]" "300 KB payload: exit 0 in $last_ms ms"
check "await '[ \"\$(latest sess-big 2)\" = thinking ]' 5" "its session sent from the first 2 000 bytes"
big '"cwd":"/u/x"' ',"session_id":"sess-late"' > "$WORK/big"
hook_file claude thinking "$WORK/big"
sleep 1
check "[ \$(count sess-late) -eq 0 ]" "a session id past the first 2 000 bytes is ignored"
hook claude thinking '{"session_id":"../../evil","cwd":"/u/proj-v"}'
check "await '[ \$(count ../../evil) -eq 1 ]' 5 && [ -z \"\$(find '$D' -name '*evil*' ! -path '$R/*')\" ]" "a session id with slashes stays in the runtime folder"
hook claude bogus "$(claude sess-q proj-q)"
hook codex stop "$(codex sess-q proj-q)"
sleep 1
check "[ \$(count sess-q) -eq 0 ]" "an unknown state, or stop outside Cursor: nothing sent"

echo "- Cursor"
fresh
hook cursor thinking '{"conversation_id":"sess-cu","hook_event_name":"beforeSubmitPrompt","workspace_roots":["/Users/u/proj-cu"]}'
check "await '[ \"\$(latest sess-cu 2)\" = thinking ]' 5" "conversation_id is the session"
check "[ \"\$(latest sess-cu 5)\" = proj-cu ]" "the folder comes from workspace_roots"
check "[ -z \"\$out\" ]" "nothing on stdout"
for pair in completed:done aborted:idle error:error; do
  hook cursor stop "$(cursor sess-cu proj-cu "${pair%%:*}")"
  check "await '[ \"\$(latest sess-cu 2)\" = ${pair#*:} ]' 5" "stop with status ${pair%%:*} sends ${pair#*:}"
done
before=$(count sess-cu)
hook cursor stop "$(cursor sess-cu proj-cu weird)"
sleep 1
check "[ \$(count sess-cu) -eq $before ]" "stop with another status: nothing sent"
check "[ \$(count not-this-one) -eq 0 ]" "session_id is not used when conversation_id is there"

echo "- Codex and Gemini"
fresh
hook codex tool "$(codex sess-cx proj-cx)"
check "[ $status -eq 0 ] && [ -z \"\$out\" ]" "Codex: exit 0, nothing on stdout"
check "await '[ \"\$(latest sess-cx 2)\" = tool ]' 5" "Codex: tool received"
check "[ \"\$(latest sess-cx 5)\" = proj-cx ]" "Codex: the folder from cwd"
hook gemini thinking "$(gemini sess-gm proj-gm)"
check "[ $status -eq 0 ] && [ \"\$out\" = '{}' ]" "Gemini: exit 0 and {} on stdout"
check "await '[ \"\$(latest sess-gm 2)\" = thinking ]' 5" "Gemini: a pretty-printed payload is read"
check "[ \"\$(latest sess-gm 5)\" = proj-gm ]" "Gemini: the folder from cwd"

echo "- Claude Code's hooks run by Cursor"
fresh
hook claude thinking '{"conversation_id":"sess-cc","generation_id":"g1","hook_event_name":"UserPromptSubmit","workspace_roots":["/Users/u/proj-cc"],"cursor_version":"1.7","session_id":"sess-cc2"}'
check "[ $status -eq 0 ] && [ -z \"\$out\" ] && [ $last_ms -lt 400 ]" "exit 0 at once, nothing on stdout: Cursor is not held"
hook claude tool '{"session_id":"sess-cc3","cwd":"/Users/u/proj-cc","cursor_version":"1.7"}'
sleep 1
check "[ \$(count sess-cc) -eq 0 ] && [ \$(count sess-cc2) -eq 0 ] && [ \$(count sess-cc3) -eq 0 ]" "nothing sent: Cursor's own hooks report that session"
check "[ -z \"\$(ls '$R'/*.state 2>/dev/null)\" ]" "no state written"
hook claude thinking '{"session_id":"sess-cq","cwd":"/u/proj-cq","prompt":"what is \"conversation_id\": in Cursor?"}'
check "await '[ \"\$(latest sess-cq 2)\" = thinking ]' 5" "Claude Code quoting those names in a prompt is still sent"

echo "- Panel moved: found again by its .local name"
fresh
moved
hook claude thinking "$(claude sess-m proj-m)"
check "await '[ \"\$(latest sess-m 2)\" = thinking ]' 10" "sent after finding the panel by its name"
check "[ \"\$(cat '$(P ip)')\" = 127.0.0.1 ]" "the new address is remembered"
check "grep -qs 'found at 127.0.0.1 (by name)' '$R/log.txt'" "the log says how it was found"

echo "- Networks where the panel was seen"
fresh
hook claude thinking "$(claude sess-w proj-w)"
check "await '[ \"\$(cut -d\" \" -f1,2 \"$(P nets)\" 2>/dev/null)\" = \"$NET $GW\" ]' 5" "recorded after a successful send: prefix and gateway address"
check "[ \"\$(awk '{print \$3}' '$(P nets)')\" -ge $(($(date +%s) - 5)) ]" "with the time it was seen"
fresh
i=8
while [ "$i" -ge 1 ]; do printf '10.0.%s aa:bb:cc:00:01:0%s %s\n' "$i" "$i" "$((100 + i))"; i=$((i - 1)); done > "$(P nets)"
hook claude thinking "$(claude sess-w8 proj-w8)"
check "await '[ \"\$(head -n 1 \"$(P nets)\" | cut -d\" \" -f1,2)\" = \"$NET $GW\" ]' 5" "a ninth network goes first"
check "[ \$(wc -l < '$(P nets)') -eq 8 ] && ! grep -q 'aa:bb:cc:00:01:01' '$(P nets)' && [ \"\$(sed -n 2p '$(P nets)')\" = '10.0.8 aa:bb:cc:00:01:08 108' ]" "eight kept, the oldest dropped"
fresh
printf '10.0.1 aa:bb:cc:00:01:01 300\n%s %s 200\n10.0.2 aa:bb:cc:00:01:02 100\n' "$NET" "$GW" > "$(P nets)"
hook claude thinking "$(claude sess-w2 proj-w2)"
check "await '[ \"\$(head -n 1 \"$(P nets)\" | cut -d\" \" -f1,2)\" = \"$NET $GW\" ]' 5" "a network seen again moves to the front"
check "[ \$(grep -c '^$NET $GW ' '$(P nets)') -eq 1 ] && [ \$(wc -l < '$(P nets)') -eq 3 ] && [ \"\$(sed -n 2p '$(P nets)')\" = '10.0.1 aa:bb:cc:00:01:01 300' ]" "without a duplicate"

echo "- Panel moved, alone on its network: found by carousel.local"
NAME=absent.invalid SHARED=localhost
fresh
moved
hook claude thinking "$(claude sess-m2 proj-m2)"
check "await '[ \"\$(latest sess-m2 2)\" = thinking ]' 15" "its own name unanswered, sent after asking carousel.local"
check "[ \"\$(cat '$(P ip)')\" = 127.0.0.1 ] && grep -qs 'found at 127.0.0.1 (by name)' '$R/log.txt'" "the new address is remembered"
SHARED=absent.invalid

echo "- mDNS blocked on a network where the panel was seen: the scan finds it"
NAME=absent.invalid
fresh
moved
printf '%s %s 100\n' "$NET" "$GW" > "$(P nets)"
hook claude thinking "$(claude sess-g proj-g)"
check "await '[ \"\$(latest sess-g 2)\" = thinking ]' 15" "sent after scanning the /24"
check "[ \"\$(cat '$(P ip)')\" = 127.0.0.1 ] && grep -qs 'found at 127.0.0.1 (by scan)' '$R/log.txt'" "the address found is remembered"

echo "- No scan on a network where the panel was never seen"
fresh
moved
printf '%s aa:bb:cc:99:99:99 100\n' "$NET" > "$(P nets)"   # same prefix, another router
hook claude thinking "$(claude sess-un proj-un)"
check "await 'grep -qs \"network not seen with the panel: no scan\" \"$R/log.txt\"' 10" "the log says why"
sleep 3
check "[ \$(count sess-un) -eq 0 ] && [ \"\$(cat '$(P ip)')\" = 127.0.0.2 ]" "nothing sent, the address unchanged"
quiet
GW=-
fresh
moved
printf '%s aa:bb:cc:00:00:01 100\n' "$NET" > "$(P nets)"
hook claude thinking "$(claude sess-ug proj-ug)"
check "await 'grep -qs \"no gateway address\" \"$R/log.txt\"' 10" "gateway address unreadable: the log says so"
sleep 3
check "[ \$(count sess-ug) -eq 0 ]" "and no scan"
quiet
GW=aa:bb:cc:00:00:01

echo "- The scan skips a panel with another name"
panel_off
panel_on "$TOKEN_A" carousel-0a01
fresh
moved
printf '%s %s 100\n' "$NET" "$GW" > "$(P nets)"
hook claude thinking "$(claude sess-sk proj-sk)"
check "await 'grep -qs \"panel not found on $NET.0/24\" \"$R/log.txt\"' 15" "the scan ends without it"
check "[ \$(count sess-sk) -eq 0 ] && [ \"\$(cat '$(P ip)')\" = 127.0.0.2 ]" "nothing sent, the address unchanged"
quiet
panel_off
panel_on "$TOKEN_A"

echo "- Two hosts claiming the panel's name: ambiguous, nothing sent"
# This machine's loopback holds one address only on macOS, so a stand-in
# curl makes the scan see two answers; every other call goes to the real one.
mkdir "$WORK/stub"
cat > "$WORK/stub/curl" <<STUB
#!/bin/sh
for a; do
  [ -n "\$o" ] && { o=\${a%/*}; break; }
  [ "\$a" = -o ] && o=next
done
case " \$* " in
  *" --parallel "*)
    for b in 7 9; do printf '{"device":"carousel","name":"carousel-695c"}' > "\$o/\$b"; done ;;
  *) exec $(command -v curl) "\$@" ;;
esac
STUB
chmod +x "$WORK/stub/curl"
fresh
moved
printf '%s %s 100\n' "$NET" "$GW" > "$(P nets)"
path=$PATH; PATH=$WORK/stub:$PATH
hook claude thinking "$(claude sess-am proj-am)"
PATH=$path
check "await 'grep -qs \"2 hosts on $NET.0/24 claim to be the panel: nothing sent\" \"$R/log.txt\"' 15" "the log says the answer is ambiguous"
check "[ \$(count sess-am) -eq 0 ] && [ \"\$(cat '$(P ip)')\" = 127.0.0.2 ]" "nothing sent, the address unchanged"
quiet
NAME=localhost

if command -v dash > /dev/null; then
  echo "- The same hook under dash"
  SH='dash'
  fresh
  hook claude thinking "$(claude sess-da proj-da)"
  check "[ $status -eq 0 ] && [ $last_ms -lt 400 ]" "exit 0 in $last_ms ms"
  check "await '[ \"\$(latest sess-da 2)\" = thinking ]' 5" "sent"
  hook gemini tool "$(gemini sess-da proj-da)"
  check "[ \"\$out\" = '{}' ] && await '[ \"\$(latest sess-da 2)\" = tool ]' 5" "Gemini {} and sent"
  hook cursor stop "$(cursor sess-dc proj-dc completed)"
  check "await '[ \"\$(latest sess-dc 2)\" = done ]' 5" "Cursor stop sent as done"
  await "[ ! -d '$R/lock' ]" 5
  hook claude end "$(claude sess-da proj-da)"
  check "[ \"\$(latest sess-da 2)\" = end ] && [ $last_ms -lt 1000 ]" "end at once"
  # dash keeps a function's original stdout open in a saved descriptor: a
  # sender started as a function would hold the agent's pipe while it retries
  panel_off
  hook claude thinking "$(claude sess-do proj-do)"
  check "[ $last_ms -lt 400 ]" "during an outage, the agent's stdout closes in $last_ms ms"
  quiet
  panel_on "$TOKEN_A"
  SH='sh'
fi

echo "- The proof under every awk installed"
# Linux runners have gawk, Debian and Ubuntu mawk, macOS the one true awk:
# the HMAC must come out the same with each
mkdir "$WORK/awk"
for a in gawk mawk nawk original-awk busybox; do
  bin=$(command -v "$a") || continue
  [ "$a" = busybox ] && { "$bin" awk 'BEGIN {}' 2> /dev/null || continue; }
  printf '#!/bin/sh\nexec %s %s "$@"\n' "$bin" "$([ "$a" = busybox ] && echo awk)" > "$WORK/awk/awk"
  chmod +x "$WORK/awk/awk"
  fresh
  path=$PATH; PATH=$WORK/awk:$PATH
  hook claude thinking "$(claude "sess-$a" proj-aw)"
  PATH=$path
  check "await '[ \"\$(latest sess-$a 2)\" = thinking ]' 5" "$a: the proof checks, sent"
  quiet
done

# No sender may outlive the test and post to the next run's panel
for f in "$WORK"/run.*; do rm -f "$f"/*/*.state; done
sleep 1
panel_off
rm -rf "$WORK"
echo
echo "$failures failure(s)"
exit $failures
