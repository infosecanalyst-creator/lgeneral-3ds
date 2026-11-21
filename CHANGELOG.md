# Changelog

## 2026-11-21

- Your options (dsui.conf) are only written when something changed (leaving Options, opening the menu, 3 seconds idle, exit), and safely through a temporary file: fewer SD card writes, fewer stalls.
- The battle forecast says "Both die" / "Both units likely destroyed" when both units would be destroyed, instead of only one side.
- Smoother play: the engine loop shows a new frame at most once every 15 ms.

## 2026-11-20

- HOME > Close saves the game in play into the autosave, and resuming from HOME or sleep is clean.
- Install path: the program is /3ds/lgeneral.3dsx.
- lgeneral.log is buffered and capped; the AI trace and status lines need an empty /3ds/lgeneral/debug.txt.
- The START menu shows the build version.

## 2026-11-19

- Saves are safer: written to a temporary file then moved into place, failures are reported, and Quit / Exit ask before leaving after a failed save.
- The battle forecast verdict shows on the map, not only on the top screen.
- Enemy strength badges have white side bars, and low ammo or fuel adds a "!".
