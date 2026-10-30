#!/usr/bin/env bash
# Download the official LGeneral 1.4.4 Windows package and extract the game data into ./data.
#
# By default only the free data is installed: the LGeneral engine files (themes, fonts,
# sounds) and K.u.K. General, a World War I campaign by Steve McGuba (Istvan Gubacsy)
# licensed CC BY-SA 3.0. Its licence and credits are copied into data/licenses/.
#
# Panzer General scenarios, maps and graphics are NOT installed unless you ask for them
# with WITH_PG=1. That data is converted from SSI's commercial game, and nothing in this
# repository grants you a licence to it: install it only if you own Panzer General and
# accept that responsibility yourself.
#
# Set LG_ZIP=/path/to/lgeneral-1.4.4-win32.zip to use a package you already downloaded.
set -euo pipefail
cd "$(dirname "$0")/.."
URL="https://downloads.sourceforge.net/project/lgeneral/lgeneral/lgeneral-1.4.4-win32.zip"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
if [ -n "${LG_ZIP:-}" ]; then
    cp "$LG_ZIP" "$TMP/lg.zip"
else
    echo "== downloading $URL"
    curl -fL -o "$TMP/lg.zip" "$URL"
fi
unzip -q "$TMP/lg.zip" -d "$TMP"
SRC="$TMP/lgeneral-1.4.4-win32"

if [ "${WITH_PG:-0}" = 1 ]; then
    echo "== WITH_PG=1: including the Panzer General data (you are responsible for having a licence to it)"
fi

rm -rf data
mkdir -p data
for d in units gfx scenarios sounds music nations terrain maps campaigns themes ai_modules; do
    cp -r "$SRC/$d" data/
done
find data -name 'Makefile*' -delete

if [ "${WITH_PG:-0}" != 1 ]; then
    # every Panzer General file is named pg / PG (folders) or pg.* / pg_* (files)
    find data \( -iname pg -o -iname 'pg.*' -o -iname 'pg_*' \) -prune -exec rm -rf {} +
fi

mkdir -p data/licenses/kukgen
cp "$SRC/docs/KuKGen.license.html" "$SRC/docs/README.kukgen" data/licenses/kukgen/ 2>/dev/null || true
cp -r "$SRC/kuk-campaign-credits/." data/licenses/kukgen/ 2>/dev/null || true
[ -f data/sounds/LICENSE.sounds ] && cp data/sounds/LICENSE.sounds data/licenses/
[ -f data/themes/default/COPYING ] && cp data/themes/default/COPYING data/licenses/theme-COPYING

echo "== data ready: $(du -sh data | cut -f1), $(find data -type f | wc -l) files"
if [ "${WITH_PG:-0}" != 1 ]; then
    echo "   (K.u.K. General only; WITH_PG=1 adds Panzer General if you own it)"
fi
