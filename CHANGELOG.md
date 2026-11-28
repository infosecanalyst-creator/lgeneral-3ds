# Changelog

## 2026-11-27

- Loading a battle is about three times faster and uses far less memory: the Panzer General set names each terrain picture four times (12 weather types, 3 pictures), and every name was loaded again. Each picture is now loaded once.
- The load screen shows a progress bar on the touch screen and the four steps on the top screen instead of a still "Loading...".
- START menu in the house shape, all on one screen: RESUME, MANUAL, SAVE GAME, LOAD GAME, SETTINGS, ORDERS AND INFO, EXIT GAME. Unit list, purchase, deploy, end turn, disband, scenario info, victory conditions, encyclopedia, restart and quit to title are on the ORDERS AND INFO page. START also closes the menu.
- Every question: YES on the left, NO on the right, the highlight starts on NO.
- Options is now called Settings.
- Manual: one scrolling page per chapter (the sticks scroll line by line), the D-pad opens the chapter it lands on, and a Left hand button sits next to Close.
- Saving: the highlight starts on the first empty slot, "Saved to slot N" shows on the map, and each save shows the day and time it was made. SAVE GAME says why it is grey.
- Load game during a battle asks first.
- No dark band at the west edge of the map, and none in deployment once every unit is placed; the top screen then says "All your units are placed. Tap Done".
- The turn card goes on by itself after a moment (a tap or A hurries it) unless it has news.
- Greyed rows are skipped by the D-pad. Help text on the top screen is white instead of grey. The objectives box names the nearest objective still to take.
- The title picture fills the top screen.

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
