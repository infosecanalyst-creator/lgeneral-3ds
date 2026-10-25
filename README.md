# LGeneral 3DS

Port of [LGeneral](https://lgames.sourceforge.io/LGeneral/) 1.4.4 (Panzer General engine, GPL) to the New 3DS XL.
Game data (converted Panzer General + K.u.k General WWI) comes from the official LGeneral Windows package and is
not stored in this repo: run `bash tools/fetch-data.sh` once to put it in `./data`.

GitHub Actions builds `lgeneral.3dsx` on every push (Actions tab → latest run → artifact).

## Status: native dual-screen front end

- **Bottom screen**: the map at 50% (ZL/ZR toggles 100%), driven by the stylus. Side strips hold the
  unit actions (Supply, Undo, Air, Merge, Split, Info) and the deployment list.
- **Top screen**: selected unit, battle forecast, hex info, combat log, minimap and objectives.
- Native touch menus for everything the PC version does with windows: title, scenario and campaign
  pickers with setup, campaign briefings, purchase, deploy, save/load (10 slots + autosave),
  unit list, overview map, options and a 4-page How to play.
- Autosave at the start of each of your turns; **Continue** on the title screen resumes it.

Not yet: stereoscopic 3D (SDL 1.2 on 3DS has no stereo output), sound (`SOUND=1` once
`3ds-sdl_mixer` is wired up). `make DSUI=0` still builds the old single-screen 640x480 port.

## Build (devkitPro MSYS2 shell)

    cd /c/devkitPro/3ds-projects/lgeneral-3ds
    bash tools/fetch-data.sh      # first time only
    bash build-lgeneral.sh

Output goes to one place only: `C:\devkitPro\3ds-projects\SD-CARD\3ds\lgeneral\`.
Needs `3ds-sdl` (SDL 1.2) from portlibs. Sound is off until `3ds-sdl_mixer` is added (`SOUND=1`).

## Install over Wi-Fi

1. First time (data, ~24 MB): start **ftpd** on the 3DS, then `bash deploy-lgeneral.sh <3DS-IP> ftp`
2. Each new build: open the Homebrew Launcher, press **Y**, then `bash deploy-lgeneral.sh <3DS-IP>`

## Controls

| Input | Action |
| --- | --- |
| Tap your unit | Select it (tap again: switch air/ground unit on that hex, or deselect) |
| Tap a hex | Move there |
| Tap an enemy | Battle forecast on the top screen; tap again to attack (Options: one-tap attack) |
| Drag a unit | Move it in one motion |
| Drag empty map | Pan |
| Long-press a unit | Full statistics |
| Tap an enemy, nothing selected | Where it can move (red) and attack (orange) |
| Circle Pad / D-pad + A | Hex cursor and act on it |
| B | Cancel / deselect |
| X | Undo last move |
| Y | End turn (warns if units still have orders) |
| R | Next unit with orders |
| Hold L | Show every enemy's threat range |
| ZL / ZR | Zoom out / in |
| C-stick | Pan |
| Start | Menu |
| Select | Overview map (tap to jump) |

Green dot = unit can still move, amber dot = can still attack.

## When something breaks

- `sdmc:/3ds/lgeneral/lgeneral.log` - startup steps and memory use (unbuffered, so the last line is where it stopped)
- `sdmc:/3ds/lgeneral/crash.txt` - crash type and addresses; turn them into source lines with
  `bash tools/addr2line.sh <pc> <lr>`

## Desktop test build

`make -f Makefile.desktop` builds a 400x480 desktop simulator of the 3DS screens (`./lgeneral-sim`;
mouse on the lower half = stylus; keys: z=A x=B s=X a=Y q=L w=R 1/2=ZL/ZR Enter=Start Tab=Select,
arrows = D-pad, ijkl = C-stick). `make -f Makefile.desktop SIM=0` builds the plain PC version.
