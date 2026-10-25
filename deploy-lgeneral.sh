#!/usr/bin/env bash
# Wi-Fi install, no SD card swaps.
#   bash deploy-lgeneral.sh <3DS-IP> ftp   upload changed files (data + .3dsx) to sdmc:/3ds/lgeneral via ftpd (port 5000)
#   bash deploy-lgeneral.sh <3DS-IP>       send the current build with 3dslink (press Y in the Homebrew Launcher first)
set -uo pipefail
cd "$(dirname "$0")"

IP="${1:-}"
MODE="${2:-link}"
[ -n "$IP" ] || { echo "usage: bash deploy-lgeneral.sh <3DS-IP> [ftp]"; exit 1; }

: "${DEVKITPRO:=/opt/devkitpro}"
export PATH="$DEVKITPRO/tools/bin:$PATH"
SD_ROOT="${SD_ROOT:-$(cd .. && pwd)/SD-CARD}"
OUT="$SD_ROOT/3ds/lgeneral"
STAMP="$(pwd)/.ftp-uploaded-$IP"

if [ "$MODE" = "ftp" ]; then
    echo "== uploading changed files to ftp://$IP:5000/3ds/lgeneral (start ftpd on the 3DS first)"
    cd "$OUT"
    if [ -f "$STAMP" ]; then
        FILES=$(find . -type f ! -path './config/*' -newer "$STAMP")
    else
        FILES=$(find . -type f ! -path './config/*')
    fi
    n=0; total=$(echo "$FILES" | grep -c . || true)
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        n=$((n+1)); printf "\r   %d/%d %s\033[K" "$n" "$total" "${f#./}"
        curl -s --ftp-create-dirs -T "$f" "ftp://$IP:5000/3ds/lgeneral/${f#./}" || { echo; echo "== upload failed at $f"; exit 1; }
    done <<< "$FILES"
    cd - >/dev/null
    touch "$STAMP"
    echo; echo "== done ($total files). Exit ftpd, open the Homebrew Launcher and start LGeneral."
else
    echo "== sending lgeneral.3dsx to $IP with 3dslink (Homebrew Launcher must be waiting: press Y)"
    3dslink -a "$IP" "$OUT/lgeneral.3dsx"
fi
