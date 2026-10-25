#!/usr/bin/env bash
# Build lgeneral.3dsx and sync it + game data to the one SD-CARD location:
#   C:\devkitPro\3ds-projects\SD-CARD\3ds\lgeneral\
# Run from the devkitPro MSYS2 shell:  bash build-lgeneral.sh   (extra args go to make, e.g. SOUND=1)
set -uo pipefail
cd "$(dirname "$0")"

: "${DEVKITPRO:=/opt/devkitpro}"
export DEVKITPRO
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"

SD_ROOT="${SD_ROOT:-$(cd .. && pwd)/SD-CARD}"
OUT="$SD_ROOT/3ds/lgeneral"

echo "== building (log: build.log)"
if ! make -j"$(nproc)" "$@" 2>&1 | tee build.log; then
    echo "== BUILD FAILED - see build.log"
    exit 1
fi

echo "== syncing to $OUT"
mkdir -p "$OUT/config"
cp -f lgeneral.3dsx "$OUT/"
cp -ru data/. "$OUT/"          # game data: only newer files are copied

echo "== OK: $OUT/lgeneral.3dsx ($(du -h lgeneral.3dsx | cut -f1)), data $(du -sh "$OUT" | cut -f1)"
echo "   next: bash deploy-lgeneral.sh <3DS-IP> ftp   (first time: data over ftpd)"
echo "         bash deploy-lgeneral.sh <3DS-IP>       (each new build: 3dslink)"
