# LGeneral 3DS

Port of [LGeneral](https://lgames.sourceforge.io/LGeneral/) 1.4.4 (Panzer General engine, GPL) to the New 3DS XL.
Game data (converted Panzer General + K.u.k General WWI) comes from the official LGeneral Windows package and is
not stored in this repo: run `bash tools/fetch-data.sh` once to put it in `./data`.

GitHub Actions builds `lgeneral.3dsx` on every push (Actions tab → latest run → artifact).

## Status: phase 2 - first 3DS boot

The engine renders at 640x480 and is scaled onto the top screen; the touchscreen acts as a
touchpad over that image. This build exists to prove the engine runs on hardware.
Phase 3 replaces it with the dual-screen layout (map + stylus on the bottom screen, info on top).

## Build (devkitPro MSYS2 shell)

    bash tools/fetch-data.sh      # first time only
    cd /c/devkitPro/3ds-projects/lgeneral-3ds
    bash build-lgeneral.sh

Output goes to one place only: `C:\devkitPro\3ds-projects\SD-CARD\3ds\lgeneral\`.
Needs `3ds-sdl` (SDL 1.2) from portlibs. Sound is off until `3ds-sdl_mixer` is added (`SOUND=1`).

## Install over Wi-Fi

1. First time (data, ~24 MB): start **ftpd** on the 3DS, then `bash deploy-lgeneral.sh <3DS-IP> ftp`
2. Each new build: open the Homebrew Launcher, press **Y**, then `bash deploy-lgeneral.sh <3DS-IP>`

## Controls (first boot)

| Input | Action |
| --- | --- |
| Touch | Left click (touchpad over the top-screen image) |
| A / B | Left / right click at the pointer |
| D-pad, Circle Pad | Scroll the map |
| L / R | Previous / next unit with moves left |
| X | Strategic map |
| Y | End turn |
| Start | Menu / cancel (Esc) |
| Select | Scenario info |

## When something breaks

- `sdmc:/3ds/lgeneral/lgeneral.log` - startup steps and memory use (unbuffered, so the last line is where it stopped)
- `sdmc:/3ds/lgeneral/crash.txt` - crash type and addresses; turn them into source lines with
  `bash tools/addr2line.sh <pc> <lr>`

## Desktop test build

`make -f Makefile.desktop` builds the same sources for Linux/MSYS2 with desktop SDL 1.2 (`./lgeneral-desktop`).
