#!/usr/bin/env bash
# Download the official LGeneral 1.4.4 Windows package and extract its game data
# (converted Panzer General + K.u.k General WWI) into ./data.
# The data is not kept in this repo: Panzer General's graphics belong to their owners.
set -euo pipefail
cd "$(dirname "$0")/.."
URL="https://downloads.sourceforge.net/project/lgeneral/lgeneral/lgeneral-1.4.4-win32.zip"
TMP="$(mktemp -d)"
echo "== downloading $URL"
curl -fL -o "$TMP/lg.zip" "$URL"
unzip -q "$TMP/lg.zip" -d "$TMP"
SRC="$TMP/lgeneral-1.4.4-win32"
mkdir -p data
for d in units gfx scenarios sounds music nations terrain maps campaigns themes ai_modules; do
    cp -r "$SRC/$d" data/
done
find data -name 'Makefile*' -delete
rm -rf "$TMP"
echo "== data ready: $(du -sh data | cut -f1), $(find data -type f | wc -l) files"
