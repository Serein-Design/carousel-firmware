#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-only
# shellcheck shell=sh
# Coding agent hook -> Carousel LED panel.
#
# The install command writes it to ~/.config/carousel/hook.sh, and each agent's
# hooks call it with the agent and the state:
#   sh ~/.config/carousel/hook.sh claude tool
# Agents: claude, codex, cursor, gemini. States: idle thinking tool attention
# error done failed end; Cursor's `stop` carries its outcome in the payload.
#
# POSIX sh, so it runs with the /bin/sh every Mac and Linux ships (stock macOS
# has bash 3.2 only). Needs curl 7.66+ (the scan's --parallel; headers read
# from a file since 7.55) and a SHA-256 tool (sha256sum, shasum or openssl).
#
# Why a script rather than an agent's "http" hook: the http hook posts the
# whole payload, complete tool output included (tens of KB), and the agent
# waits for the answer. Here only the session id and the folder name are
# kept, and the agent gets control back right away. It always exits 0.
#
# Only the last state of each session matters for the display. The hook
# writes it to a file and leaves; a single sender, in the background, sends
# it to the panel and retries for up to 5 min if the Wi-Fi falters. During an
# outage states replace each other instead of piling up: when the link comes
# back, the panel directly receives the current state. The timestamp lets it
# ignore a send that would arrive after a more recent one.
#
# The panel's hook token travels in the X-Hook-Token header, read by curl from
# a 0600 file: never in an address (logs, history) nor on a command line (ps).
# It goes only to an address whose /id names the followed panel and proves
# that it holds the token: see proves().
#
# When the panel no longer answers at its last address (new DHCP lease,
# another Wi-Fi), the sender asks its .local names, then scans the /24, but
# only on a network where the panel was seen: see search().
#
# Files: ~/.config/carousel (XDG_CONFIG_HOME if set): `default`, the panel
# followed, and per panel panels/<name>/header, ip and nets. CAROUSEL_HOST
# names another installed panel, in any case, with or without .local.
# Send log: $TMPDIR/carousel-<uid>/<name>/log.txt (/tmp without TMPDIR).
# The CAROUSEL_TEST_* variables are for the tests only.

agent=$1
state=$2
# Bytes, not characters: a payload need not be valid UTF-8. Nothing for the
# agent to show on stderr either.
LC_ALL=C
export LC_ALL
exec 2> /dev/null

# Start of the payload, then drain the rest: without a full read, the agent
# would write into a closed pipe. One line, without NUL bytes, for awk.
payload=$(head -c 2000 2>/dev/null | tr -d '\000' | tr '\n\r\t' '   ')
cat > /dev/null 2>&1

# Gemini CLI reads its hooks' stdout as JSON: an empty decision, the tool runs
[ "$agent" = gemini ] && printf '{}\n'
case $agent in claude | codex | cursor | gemini) ;; *) exit 0 ;; esac

# The first string value of a key, $2 a pattern between the colon and the
# opening quote (the bracket of an array). Nothing when absent or truncated.
field() {
  printf '%s\n' "$payload" | awk -v k="$1" -v pre="${2:-}" '{
    if (match($0, "\"" k "\"[ ]*:[ ]*" pre "\"[^\"]*\"")) {
      v = substr($0, RSTART, RLENGTH - 1); sub(/.*"/, "", v); print v
    }
    exit
  }'
}

if [ "$state" = stop ]; then
  [ "$agent" = cursor ] || exit 0
  case $(field status) in
    completed) state='done' ;;
    aborted) state=idle ;;
    error) state=error ;;
    *) exit 0 ;;
  esac
fi
case $state in idle | thinking | tool | attention | error | done | failed | end) ;; *) exit 0 ;; esac

# Cursor can also run Claude Code's hooks (its third-party hooks setting).
# Its own hooks already report that session, under another session id: a
# Claude call carrying Cursor's fields is dropped, or the panel would show
# the session twice. A quoted name inside a string is escaped, so not a key.
[ "$agent" = claude ] && printf '%s\n' "$payload" |
  grep -Eq '(^|[^\\])"(cursor_version|conversation_id)"[ ]*:' && exit 0

session=''
[ "$agent" = cursor ] && session=$(field conversation_id)
[ -n "$session" ] || session=$(field session_id)
[ -n "$session" ] || exit 0
folder=''
[ "$agent" = cursor ] && folder=$(field workspace_roots '[[][ ]*')
[ -n "$folder" ] || folder=$(field cwd)
folder=${folder##*\\}    # last segment of a Windows path (\\ in JSON)
folder=${folder##*/}     # or of a Unix path

# The panel followed: CAROUSEL_HOST if set, else the one installed last.
cfg=${XDG_CONFIG_HOME:-$HOME/.config}/carousel
[ -d "$cfg/panels" ] || exit 0          # not installed: nothing to do
uid=$(id -u)
base=${CAROUSEL_TEST_DIR:-${TMPDIR:-/tmp}/carousel-$uid}
# The panel is on the local network: never through a proxy, which would also
# see the token header (every curl below)
NO_PROXY="*" no_proxy="*"; export NO_PROXY no_proxy

port=${CAROUSEL_TEST_PORT:-80}

# Private to this user. In a shared /tmp another account could otherwise
# create it first, or a symlink in its place, then read the session names or
# have this hook overwrite files. Not ours, or a symlink: do nothing.
own() {
  [ -e "$1" ] || [ -L "$1" ] || mkdir -m 700 "$1" 2>/dev/null
  [ -d "$1" ] && [ ! -L "$1" ] && [ "$(ls -ldn "$1" 2>/dev/null | awk '{print $3}')" = "$uid" ]
}
stamp() { date +%H:%M:%S; }

name=$(printf '%s' "${CAROUSEL_HOST:-}" | tr 'A-Z' 'a-z')
name=${name%.local}
if [ -n "$name" ]; then
  case $name in carousel-*[!a-z0-9-]* | carousel-) name='' ;; carousel-*) ;; *) name='' ;; esac
  if [ -z "$name" ] || [ ! -f "$cfg/panels/$name/header" ]; then
    own "$base" && printf '%s CAROUSEL_HOST names no installed panel: nothing sent\n' "$(stamp)" >> "$base/log.txt"
    exit 0
  fi
else
  [ -f "$cfg/default" ] && { read -r name < "$cfg/default" || :; }
  case $name in carousel-*[!a-z0-9-]* | carousel- | '') exit 0 ;; carousel-*) ;; *) exit 0 ;; esac
fi
pdir=$cfg/panels/$name
header=$pdir/header
nets=$pdir/nets
[ -r "$header" ] || exit 0
own "$base" || exit 0
dir=$base/$name
own "$dir" || exit 0
lock=$dir/lock
file=$dir/$(printf '%s' "$session" | tr -c 'A-Za-z0-9-' '_').state

# Seconds times 10^6, plus one per event within the same second: sh has no
# clock below the second, and the panel ignores a timestamp that does not
# increase. Hooks run one after the other, so this file holds the last one.
t=$(($(date +%s) * 1000000))
prev=''
[ -f "$file" ] && { read -r _ prev _ < "$file" || :; }
case $prev in '' | *[!0-9]*) ;; *) [ "$prev" -ge "$t" ] && t=$((prev + 1)) ;; esac
printf '%s %s %s %s\n' "$state" "$t" "$session" "$folder" > "$file.$$" && mv -f "$file.$$" "$file"

log() { printf '%s %s\n' "$(stamp)" "$*" >> "$dir/log.txt"; }

# The panel's last address, from the install or a later search
lastIp() {
  ip=''
  [ -f "$pdir/ip" ] && { read -r ip < "$pdir/ip" || :; }
  case $ip in '' | *[!0-9.]*) ip='' ;; esac
}

# A name in /id is easy to copy: a device that took the panel's address, or
# answers to its name, would get the token. So the hook asks /id?c=<nonce>,
# a fresh random one, and the panel adds "proof", the first 32 hex
# characters of HMAC-SHA256 keyed with the token over the nonce, ":" and its
# own address. Checked with the address curl connected to: a relay that
# passes the nonce on to the panel elsewhere brings back a proof for the
# panel's address, not its own, and gets nothing. Computed here with
# whichever SHA-256 tool the system has, the key fed on stdin: never on a
# command line. Without one, nothing is sent (see below).
sha() {
  case $shaTool in
    sha256sum) sha256sum ;;
    shasum) shasum -a 256 ;;
    openssl) openssl dgst -sha256 ;;
  esac | sed -n 's/.*\([0-9a-f]\{64\}\).*/\1/p'
}
# Bytes as printf escapes: with an argument, the 64-byte key (the token's
# characters, then zeros) xor that value; without, the hex digits read.
# bxor, not xor: gawk (Linux) has a built-in xor and refuses to redefine it.
esc() {
  awk -v x="${1:-}" 'BEGIN { for (i = 0; i < 16; i++) v[substr("0123456789abcdef", i + 1, 1)] = i }
  function bxor(a, b,   r, p) { r = 0; for (p = 1; p < 256; p *= 2) if ((int(a / p) + int(b / p)) % 2) r += p; return r }
  {
    s = ""
    if (x != "") for (i = 1; i <= 64; i++) {
      c = substr($0, i, 1); n = c == "" ? 0 : (c ~ /[0-9]/ ? 48 : 87) + v[c]
      s = s sprintf("\\%03o", bxor(n, x))
    } else for (i = 1; i < length($0); i += 2) s = s sprintf("\\%03o", v[substr($0, i, 1)] * 16 + v[substr($0, i + 1, 1)])
    printf "%s", s
  }'
}
key() { sed -n 's/^X-Hook-Token: *\([0-9a-f]*\).*/\1/p' "$header" | esc "$1"; }
# shellcheck disable=SC2059  # the formats are escapes made by esc
hmac() {  # <message>
  inner=$({ printf "$(key 54)"; printf '%s' "$1"; } | sha)
  [ -n "$inner" ] || return 1
  { printf "$(key 92)"; printf "$(printf '%s\n' "$inner" | esc)"; } | sha
}
newNonce() {
  nonce=$(od -An -tx1 -N16 /dev/urandom | tr -d ' \n')
  case $nonce in *[!0-9a-f]* | '') nonce='' ;; esac
}
# The /id answer of the address $1, its last line the address reached
idc=1 idm=2
askId() {
  newNonce
  a=$(curl -s --connect-timeout "$idc" -m "$idm" -w '\n%{remote_ip}' "http://$1:$port/id?c=$nonce" 2>/dev/null)
}
proofIn() { printf '%s\n' "$1" | sed -n 's/.*"proof":"\([0-9a-f]\{32\}\)".*/\1/p'; }
# The panel gives at most 4 proofs a second, to every asker together: an
# answer without one is asked again once, 300 ms later (patient empty: no
# time for that). Twice without, the panel is busy, or not the panel.
unproven='' busy='' patient=1
proves() {  # <the /id answer> <the address asked>
  [ -n "$nonce" ] || return 1
  got=$(proofIn "$1")
  if [ -z "$got" ] && [ -n "$patient" ]; then
    sleep 0.3
    askId "$2"
    case $a in *"\"name\":\"$name\""*) [ "$(printf '%s\n' "$a" | tail -n 1)" = "$2" ] && got=$(proofIn "$a") ;; esac
  fi
  if [ -z "$got" ]; then
    [ "$2" = "$busy" ] || log "$2 answers with the panel's name but no proof, twice (panel busy): nothing sent"
    busy=$2
    return 1
  fi
  want=$(hmac "$nonce:$2")
  [ -n "$got" ] && [ -n "$want" ] && [ "$got" = "$(printf '%.32s' "$want")" ] && return 0
  [ "$2" = "$unproven" ] ||
    log "$2 answers with the panel's name but not its token: nothing sent (token renewed? run the install command again)"
  unproven=$2
  return 1
}

# The panel is recognised by its /id answer, which bears its unique name,
# from the very address asked
isPanel() {
  askId "$1"
  case $a in
    *"\"name\":\"$name\""*)
      [ "$(printf '%s\n' "$a" | tail -n 1)" = "$1" ] && proves "$a" "$1" && return 0 ;;
  esac
  return 1
}

# The network this machine is on: the /24 prefix of its address and the
# gateway's hardware address, in $net and $gw. Fails when either is unknown
# (Windows, a VPN without a gateway, no ARP entry yet).
netId() {
  net='' gw=''
  if [ -n "${CAROUSEL_TEST_NET:-}" ]; then
    net=$CAROUSEL_TEST_NET gw=${CAROUSEL_TEST_GW:-}
  else
    case $(uname -s) in
      Darwin)
        r=$(route -n get default)
        via=$(printf '%s\n' "$r" | awk '$1 == "gateway:" {print $2; exit}')
        itf=$(printf '%s\n' "$r" | awk '$1 == "interface:" {print $2; exit}')
        [ -n "$itf" ] && net=$(ipconfig getifaddr "$itf")
        [ -n "$via" ] && gw=$(arp -n "$via" | awk '{print $4; exit}')
        ;;
      Linux)
        r=$(ip -4 route get 1.1.1.1)
        via=$(printf '%s\n' "$r" | sed -n 's/.* via \([0-9.]*\).*/\1/p')
        net=$(printf '%s\n' "$r" | sed -n 's/.* src \([0-9.]*\).*/\1/p')
        [ -n "$via" ] && gw=$(ip neigh show "$via" | sed -n 's/.* lladdr \([0-9A-Fa-f:]*\).*/\1/p')
        ;;
    esac
    net=${net%.*}
  fi
  gw=$(printf '%s' "$gw" | tr 'A-F' 'a-f')
  case $net in *[!0-9.]* | '') return 1 ;; esac
  case $gw in *[!0-9a-f:]* | '') return 1 ;; *:*:*) ;; *) return 1 ;; esac
}

# The panel answered: this network is one where it was seen. The most
# recent first, at most 8 (home, office, a phone's hotspot...).
remember() {
  netId || return 0
  {
    printf '%s %s %s\n' "$net" "$gw" "$(date +%s)"
    [ -f "$nets" ] && awk -v n="$net" -v g="$gw" '!($1 == n && $2 == g)' "$nets"
  } | head -n 8 > "$nets.$$" && mv -f "$nets.$$" "$nets"
}

# The address that may get the token: confirmed by its /id once per sending
# run. The time of the confirmation lets the end of a session go out at once.
checked=''
target=''
found() {  # <address>: its /id named the panel just now
  checked=$1 target=$1
  printf '%s %s\n' "$(date +%s)" "$1" > "$dir/confirmed"
}
address() {
  lastIp
  target=''
  [ -n "$ip" ] && [ "$ip" = "$checked" ] && { target=$ip; return 0; }
  [ -n "$ip" ] && isPanel "$ip" && { found "$ip"; return 0; }
  search
}

moved() {  # <address> <how>: found elsewhere, its /id read
  case $1 in '' | *[!0-9.]*) return 1 ;; esac
  printf '%s\n' "$1" > "$pdir/ip.$$" && mv -f "$pdir/ip.$$" "$pdir/ip"
  ip=$1
  found "$1"
  log "panel found at $1 (by $2)"
}

# Every address of the /24 asked in parallel by a single curl (about 2 s,
# negligible load), each answer in a file named after the last byte. Works
# where the router filters mDNS (multicast). Needs curl 7.66+ (--parallel).
# Names are unique, so two hosts claiming the panel's name mean one of them
# lies: neither gets the token.
scan() {
  sd=$dir/scan hit='' hits=0 answer=''
  rm -rf "$sd"
  mkdir -m 700 "$sd" || return 1
  newNonce
  curl -s --parallel --parallel-max 128 --connect-timeout 1 -m 2 \
    -o "$sd/#1" "http://$net.[1-254]:$port/id?c=$nonce"
  for sf in "$sd"/*; do
    [ -f "$sf" ] && grep -qF "\"name\":\"$name\"" "$sf" &&
      { hit=$net.${sf##*/}; hits=$((hits + 1)); answer=$(head -c 512 "$sf"); }
  done
  rm -rf "$sd"
  # Each address is asked by its number, so the one to check the proof with
  # is the hit's own
  if [ "$hits" -gt 1 ]; then
    log "$hits hosts on $net.0/24 claim to be the panel: nothing sent"
    return 1
  fi
  [ -n "$hit" ] && { proves "$answer" "$hit" || return 1; } && moved "$hit" scan && return 0
  log "panel not found on $net.0/24"
  return 1
}

# The panel's .local names first, instant where the network lets multicast
# through: its own, then carousel.local, the one it takes alone on its
# network (one name at a time, firmware/src/panel_name.h). Two panels: that
# one is the other panel, which fails the name check. Then the scan, only on
# a network where the panel answered before:
# a /24 prefix alone matches many homes and cafés (192.168.1.0/24), so the
# network is the prefix plus the gateway's hardware address, and with no
# readable gateway there is no scan. At most one search every 30 s.
lastSearch=0
search() {
  now=$(date +%s)
  [ $((now - lastSearch)) -ge 30 ] || return 1
  lastSearch=$now
  for host in "${CAROUSEL_TEST_NAME:-$name.local}" "${CAROUSEL_TEST_SHARED:-carousel.local}"; do
    newNonce
    a=$(curl -s --connect-timeout 4 -m 5 -w '\n%{remote_ip}' "http://$host:$port/id?c=$nonce")
    case $a in
      *"\"name\":\"$name\""*)
        via=$(printf '%s\n' "$a" | tail -n 1)
        proves "$a" "$via" && moved "$via" name && return 0
        ;;
    esac
  done
  if ! netId; then
    log "no gateway address on this network: no scan"
  elif [ -f "$nets" ] && awk -v n="$net" -v g="$gw" '$1 == n && $2 == g {f = 1} END {exit !f}' "$nets"; then
    scan && return 0
  else
    log "network not seen with the panel: no scan"
  fi
  return 1
}

# One attempt, no waiting: the sender is the one that retries.
# Answers 0 when done with that state (sent, or refused for good), 1 to retry.
connect=2 most=4
deliver() {  # <state> <t> <session> <folder> <state file>
  code=$(curl -s -o /dev/null -w '%{http_code}' --connect-timeout "$connect" -m "$most" -G \
    -H "@$header" --data-urlencode "s=$3" --data-urlencode "n=$4" --data-urlencode "t=$2" \
    "http://$target:$port/agent/$1" 2>/dev/null)
  case $code in
    2??) log "$1 sent ($4)"; answered=1 ;;
    401) log "$1 refused by the panel (token): run the install command again" ;;
    000 | 5?? | '') checked=''; return 1 ;;   # its /id is read again, else searched
    *) log "$1 refused by the panel (HTTP $code)" ;;
  esac
  if [ "$1" = end ]; then
    rm -f "$5" "$5.sent"
  else
    printf '%s\n' "$2" > "$5.sent"
  fi
}

pendingWork() {
  for f in "$dir"/*.state; do
    [ -e "$f" ] || continue
    t='' sentT=''
    { read -r _ t _ < "$f" || :; } 2>/dev/null
    [ -f "$f.sent" ] && { read -r sentT < "$f.sent" || :; }
    [ -n "$t" ] && [ "$t" != "$sentT" ] && return 0
  done
  return 1
}

sender() {
  end=$(($(date +%s) + 300)) pause=1
  [ -f "$dir/log.txt" ] && [ "$(wc -c < "$dir/log.txt")" -gt 100000 ] &&
    mv -f "$dir/log.txt" "$dir/log.old.txt"
  while [ "$(date +%s)" -lt "$end" ]; do
    date +%s > "$lock/seen"
    address
    all=1 answered=0
    for f in "$dir"/*.state; do
      [ -e "$f" ] || continue
      e='' t='' s='' p='' sentT=''
      { read -r e t s p < "$f" || :; } 2>/dev/null
      [ -n "$e" ] && [ -n "$t" ] && [ -n "$s" ] || continue
      [ -f "$f.sent" ] && { read -r sentT < "$f.sent" || :; }
      [ "$sentT" = "$t" ] && continue
      if [ -n "$target" ] && deliver "$e" "$t" "$s" "$p" "$f"; then
        pause=1
      else
        all=0
        log "$e pending, panel not reached at ${ip:-no address} ($p)"
      fi
    done
    [ "$answered" = 1 ] && remember
    if [ "$all" = 1 ]; then
      rm -rf "$lock"
      # A hook may have written just before the lock was released, seeing the
      # sender still there: check before leaving.
      pendingWork && mkdir "$lock" 2>/dev/null && continue
      return 0
    fi
    sleep "$pause"
    [ "$pause" -lt 5 ] && pause=$((pause + 1))
  done
  log "giving up after 5 min: the next hook will restart sending"
  rm -rf "$lock"
}

# No SHA-256 tool: no proof can be checked, and a name alone is easy to copy,
# so nothing is sent. Said once, with the fix.
shaTool=''
for x in sha256sum shasum openssl; do command -v "$x" > /dev/null && { shaTool=$x; break; }; done
[ "${CAROUSEL_TEST_SHA:-}" = none ] && shaTool=''
if [ -z "$shaTool" ]; then
  [ -f "$dir/nosha" ] || { : > "$dir/nosha"; log "no SHA-256 tool (sha256sum, shasum or openssl): nothing sent; install one of them"; }
  exit 0
fi
rm -f "$dir/nosha"

# End of session: Codex gives its hook 1 s, and the agent exits right after,
# which could take a background sender down with it. To an address whose
# proof was checked in the last 10 min, one attempt capped at 0.7 s; if it
# fails, the sender takes over as for the other states. Otherwise the proof
# comes first, in the same 0.7 s, without asking twice; not proven in time,
# the end is dropped (the panel lets the session go after 15 min).
if [ "$state" = end ]; then
  conf='' cip=''
  [ -f "$dir/confirmed" ] && { read -r conf cip < "$dir/confirmed" || :; }
  case $conf in '' | *[!0-9]*) conf=0 ;; esac
  lastIp
  if [ -n "$ip" ] && [ "$cip" = "$ip" ] && [ $(($(date +%s) - conf)) -lt 600 ]; then
    target=$ip connect=0.5 most=0.7
  elif idc=0.3 idm=0.3 patient='' && [ -n "$ip" ] && isPanel "$ip"; then
    found "$ip"
    target=$ip connect=0.3 most=0.3
  else
    rm -f "$file" "$file.sent"
    log "end dropped ($folder): the panel did not prove its token in time"
    exit 0
  fi
  deliver end "$t" "$session" "$folder" "$file" && exit 0
  connect=2 most=4 idc=1 idm=2 patient=1
fi

# One sender at a time: mkdir is atomic, it serves as a lock. A lock nobody
# has refreshed for 6 min comes from a killed sender (computer put to sleep,
# session force-closed): take it over.
if ! mkdir "$lock" 2>/dev/null; then
  since=''
  [ -f "$lock/seen" ] && { read -r since < "$lock/seen" || :; }
  case $since in '' | *[!0-9]*) since=0 ;; esac
  [ $(($(date +%s) - since)) -gt 360 ] || exit 0
  rm -rf "$lock"
  mkdir "$lock" 2>/dev/null || exit 0
fi
date +%s > "$lock/seen"
# Redirected for good inside a subshell: dash runs a function's redirections
# by keeping the originals in saved descriptors, so "sender > /dev/null &"
# would hold the agent's stdout open for as long as it retries.
( exec < /dev/null > /dev/null 2>&1; sender ) &
exit 0
