#!/bin/sh

#This script creates a heavily optimized binary. For debugging, you're better off using 'make'.

#clean up
rm flips flips.exe floating.zip obj/*

FLAGS='-Wall -Werror -O3 -s -flto -fwhole-program -fweb -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden'
FLAGS=$FLAGS' -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables'
FLAGS=$FLAGS' -ffunction-sections -fdata-sections -Wl,--gc-sections -fprofile-dir=obj/'
FLAGS=$FLAGS' -Wl,-z,relro,--as-needed,--hash-style=gnu,--relax'

##create windows binary
#echo 'Windows/Resource (Wine warmup)'
#mingwver 5.3
#wine windres flips.rc rc.o
#
#echo 'Windows (1/3)'
#rm flips.exe; wine mingw32-make TARGET=windows OPTFLAGS="$FLAGS -fprofile-generate -lgcov"
#[ -e flips.exe ] || exit
#echo 'Windows (2/3)'
#profile/profile.sh 'wine flips.exe'
#echo 'Windows (3/3)'
#rm flips.exe; wine mingw32-make TARGET=windows OPTFLAGS="$FLAGS -fprofile-use"
#
##verify there are no unexpected dependencies
#objdump -p flips.exe | grep 'DLL Name' | \
#	grep -Pvi '(msvcrt|advapi32|comctl32|comdlg32|gdi32|kernel32|shell32|user32)' && \
#	echo "Invalid dependency" && exit
#
##test cli binaries
#echo CLI
#rm flips; make TARGET=cli DIVSUF=no
#[ -e flips ] || exit

#create linux binary
if [ -e profile/smw.smc ]; then
echo 'GTK+ (1/3)'
rm flips; make TARGET=gtk OPTFLAGS="$FLAGS -fprofile-generate -lgcov"
[ -e flips ] || exit
echo 'GTK+ (2/3)'
profile/profile.sh ./flips
echo 'GTK+ (3/3)'
rm flips; make TARGET=gtk OPTFLAGS="$FLAGS -fprofile-use"
#mv flips '~/bin/flips'
else
echo 'Warning: Missing ROMs for profiling, building without'
rm flips; make TARGET=gtk OPTFLAGS="$FLAGS"
fi

#echo Finishing
##compress source 
#7z a floating.zip flips.exe
#zipcrush floating.zip
#echo Size:    $(stat -c%s flips.exe)/96768
#echo \(Linux:  $(stat -c%s ~/bin/flips)\)
#echo \(Zipped: $(stat -c%s floating.zip)/59881\)
#
#./special.sh
#
#rm src.zip boring.zip
