@echo off
rem Double-click to build LGeneral for 3DS (runs build-lgeneral.sh in devkitPro MSYS2)
set MSYSTEM=MSYS
"C:\devkitPro\msys2\usr\bin\bash.exe" -lc "cd /c/devkitPro/3ds-projects/lgeneral-3ds && bash build-lgeneral.sh"
pause
