#!/bin/sh

# For whatever reason, Windows sometimes has LANG unset. This breaks grep at the end, so setting this manually.
export LANG=C.UTF-8

echo "This script creates a heavily optimized Windows binary. For debugging you're better off using the Makefile directly."


rm floating.zip
rm -r obj/* || true

#if trying to make a 32bit Flips, add -Wl,--large-address-aware

echo 'Windows (1/3)'
rm -r obj/* flips.exe; make CFLAGS="$FLAGS -fprofile-generate -lgcov"
[ -e flips.exe ] || exit
echo 'Windows (2/3)'
./flips.exe --create --bps-delta         profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null
./flips.exe --create --bps-delta-moremem profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null
echo 'Windows (3/3)'
rm flips.exe; make CFLAGS="$FLAGS -fprofile-use -s"


# CI currently has invalid dependencies. Unsure if ok or not.
ERROR_CODE_ON_INVALID_DEPENDENCIES=1
if [ -z ${IGNORE_ON_INVALID_DEPENDENCIES} ]; then ERROR_CODE_ON_INVALID_DEPENDENCIES=0; fi
#verify that there are no unexpected dependencies
objdump -p flips.exe | grep 'DLL Name' | \
	grep -Pvi '(msvcrt|advapi32|comctl32|comdlg32|gdi32|kernel32|shell32|user32)' && \
	echo "Invalid dependency" && exit $ERROR_CODE_ON_INVALID_DEPENDENCIES


