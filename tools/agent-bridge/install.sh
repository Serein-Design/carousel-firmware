#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-only
# shellcheck shell=sh
# Sets this computer up so that its coding agents drive a Carousel panel's CODE
# screen. The panel serves this script with its token, address and name in
# front, from the command on its Panel page:
#   curl -sS --compressed --noproxy '*' "http://<panel IP>/install?k=<token>" | sh
# From a clone of the repository, the same three come from the environment:
#   CAROUSEL_TOKEN=<token> CAROUSEL_IP=<panel IP> CAROUSEL_NAME=carousel-xxxx sh tools/agent-bridge/install.sh
# To remove everything, panel or not:
#   sh tools/agent-bridge/install.sh --uninstall    (or: sh ~/.config/carousel/setup.sh uninstall)
#
# It writes ${XDG_CONFIG_HOME:-~/.config}/carousel/ (0700): hook.sh, setup.sh,
# "default" (the panel followed) and panels/<name>/ with "header" (the token,
# 0600) and "ip". Then setup.sh writes the hooks of each agent found, and
# lists in "created" the agent files it created, for the uninstall.
#
# Everything runs from main(), called on the last line: a download cut short
# runs nothing.

refuse() { echo "carousel: $1" >&2; exit 1; }

# Each region writes a sibling file on stdout. The panel's copy has each region
# replaced by the file itself (tools/agent-bridge/embed.mjs).
hook_sh() {
  # >>> embed hook.sh
  cat "${CAROUSEL_TEST_SRC:-$(dirname "$0")}/hook.sh"
  # <<< embed
}
setup_sh() {
  # >>> embed setup.sh
  cat "$(dirname "$0")/setup.sh"
  # <<< embed
}

# Writes the output of the command after $1 to the file $1, replaced at once: an
# agent may be running the hook at that moment.
put() {
  f=$1; shift
  { "$@" > "$f.new" && mv "$f.new" "$f"; } || { rm -f "$f.new"; refuse "cannot write $f"; }
}

main() {
  umask 077
  C=${XDG_CONFIG_HOME:-$HOME/.config}/carousel
  if [ "${1:-}" = --uninstall ]; then
    setup_sh | sh -s uninstall
    return
  fi
  again="copy the install command again from the Panel page"
  t=${CAROUSEL_TOKEN:-} ip=${CAROUSEL_IP:-} name=${CAROUSEL_NAME:-}
  case $t in *[!0-9a-f]*) t= ;; esac
  [ ${#t} = 32 ] || refuse "no valid panel token: $again"
  case $ip in '' | *[!0-9.]*) refuse "no valid panel address: $again" ;; esac
  case $name in '' | *[!a-z0-9-]*) refuse "no valid panel name: $again" ;; esac
  case $C in *[\"\'\\]*) refuse "$C has a quote or a backslash in its path: nothing written" ;; esac
  [ ! -L "$C" ] || refuse "$C is a symbolic link: nothing written"
  P=$C/panels/$name
  { mkdir -p "$P" && chmod 700 "$C" "$C/panels" "$P"; } || refuse "cannot write $C"
  put "$C/hook.sh" hook_sh
  put "$C/setup.sh" setup_sh
  put "$P/header" printf 'X-Hook-Token: %s\n' "$t"
  put "$P/ip" echo "$ip"
  put "$C/default" echo "$name"
  echo "Carousel: this computer follows $name at $ip"
  sh "$C/setup.sh" install < /dev/null
}

main "$@"
