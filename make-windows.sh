#!/bin/sh

# For whatever reason, Windows sometimes has LANG unset. This breaks grep at the end, so setting this manually.
export LANG=C.UTF-8

echo "This script creates a heavily optimized Windows binary. For debugging you're better off using the Makefile directly."

# Set GCC specific optimization flags. These may need to be revisited when the project is build using MVSC.
FLAGS='-Wall -O3 -flto -fuse-linker-plugin -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden'
FLAGS=$FLAGS' -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables'
FLAGS=$FLAGS' -ffunction-sections -fdata-sections -Wl,--gc-sections -fprofile-dir=obj/'

# Can be overridden to build Windows binaries from a Linux host
# for example WINE=wine MAKE=mingw32-make ./make-windows.sh
MAKE="${MAKE:-make}"
WINE="${WINE:-}"


rm floating.zip
rm -r obj/* || true

#if trying to make a 32bit Flips, add -Wl,--large-address-aware

echo 'Windows (1/3)'
rm -r obj/* flips.exe; $MAKE CFLAGS="$FLAGS -fprofile-generate -lgcov"
[ -e flips.exe ] || exit
echo 'Windows (2/3)'
$WINE ./flips.exe --create --bps-delta         profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null
$WINE ./flips.exe --create --bps-delta-moremem profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null
echo 'Windows (3/3)'
rm flips.exe; $MAKE CFLAGS="$FLAGS -fprofile-use -s"

#verify that there are no unexpected dependencies
objdump -p flips.exe | grep 'DLL Name' | \
	grep -Pvi '(msvcrt|advapi32|comctl32|comdlg32|gdi32|kernel32|shell32|user32|api-ms-win-crt)' && \
	echo "Invalid dependency" && exit 1

# a script's exit status is the same as the last command or pipeline run; if that's the above grep, things break
true
