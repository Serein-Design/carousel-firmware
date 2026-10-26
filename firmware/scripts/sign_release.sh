#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# Signs a built firmware image for the panels' updater (src/updater.h) and
# writes the release files into an output folder:
#   firmware.bin    the image the panels download
#   manifest.json   version, URL, size and signature
# The private key comes from $SIGNING_KEY (PEM text, the release workflow's
# secret) or from a file given as 5th argument. Before writing anything the
# signature is checked against the public key built into the firmware, so a
# wrong key cannot ship.
#
#   sign_release.sh <firmware.bin> <version> <github owner/repo> <out dir> [key.pem]
set -euo pipefail

BIN=$1 VERSION=$2 REPO=$3 OUT=$4 KEYFILE=${5:-}
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

[[ $VERSION =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "version must look like v1.2.3: $VERSION" >&2; exit 1; }
if [[ -z $KEYFILE ]]; then
  [[ -n ${SIGNING_KEY:-} ]] || { echo "no key: set SIGNING_KEY or pass a key file" >&2; exit 1; }
  (umask 077; printf '%s\n' "$SIGNING_KEY" > "$WORK/key.pem")
  KEYFILE=$WORK/key.pem
fi

SHA=$(openssl dgst -sha256 -r "$BIN" | cut -d' ' -f1)
# Same text as signedMessage() in src/update_logic.cpp, no newline
printf 'carousel-firmware %s %s' "$VERSION" "$SHA" > "$WORK/msg"
openssl dgst -sha256 -sign "$KEYFILE" -out "$WORK/sig.der" "$WORK/msg"

# The public key as the firmware has it
sed -n '/BEGIN PUBLIC KEY/,/END PUBLIC KEY/p' "$HERE/../src/updater.cpp" |
  sed -E 's/^[[:space:]]*"//; s/\\n";?$//' > "$WORK/pub.pem"
openssl dgst -sha256 -verify "$WORK/pub.pem" -signature "$WORK/sig.der" "$WORK/msg" >/dev/null ||
  { echo "signature does not match the key built into the firmware" >&2; exit 1; }

mkdir -p "$OUT"
cp "$BIN" "$OUT/firmware.bin"
printf '{"version":"%s","url":"https://github.com/%s/releases/download/%s/firmware.bin","size":%s,"sig":"%s"}\n' \
  "$VERSION" "$REPO" "$VERSION" "$(wc -c < "$BIN" | tr -d ' ')" \
  "$(od -An -v -tx1 "$WORK/sig.der" | tr -d ' \n')" > "$OUT/manifest.json"
echo "signed $VERSION, sha256 $SHA"
