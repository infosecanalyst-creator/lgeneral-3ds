#!/usr/bin/env bash
# Turn crash.txt addresses into source lines:  bash tools/addr2line.sh <pc> <lr>
cd "$(dirname "$0")/.."
: "${DEVKITPRO:=/opt/devkitpro}"
"$DEVKITPRO/devkitARM/bin/arm-none-eabi-addr2line" -f -p -C -e build/lgeneral.elf "$@"
