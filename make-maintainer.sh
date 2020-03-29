#!/bin/sh

if [ "$HOME" != "/home/alcaro" ]; then
  #This script is for making official releases, including a Windows build.
  #If you're intending to fork Floating IPS, or take over maintenance, you're welcome to edit this script.
  #If not, you don't want it. make.sh enables all optimizations for a local build.
  echo "Use ./make.sh instead."
  exit 1
fi

rm floating.zip

# . rather than ./make.sh, so $FLAGS remains set
. ./make.sh
mv flips flips-linux
7z a floating.zip flips-linux
mv ./flips-linux ~/bin/flips

#create windows binary
mingwver 64 || true # this is a script that sets the Wine PATH
#if trying to make a 32bit Flips, add -Wl,--large-address-aware

#I am not completely sure if this is a good idea.
#FLAGS=$FLAGS' -fopenmp'
#FLAGS=$FLAGS' Z:/home/alcaro/tools/mingw64-8.1.0/lib/gcc/x86_64-w64-mingw32/8.1.0/libgomp.a'
#FLAGS=$FLAGS' Z:/home/alcaro/tools/mingw64-8.1.0/lib/gcc/x86_64-w64-mingw32/8.1.0/libgcc_eh.a'
#FLAGS=$FLAGS' Z:/home/alcaro/tools/mingw64-8.1.0/x86_64-w64-mingw32/lib/libwinpthread.a'

echo 'Windows (1/3)'
rm obj/* flips.exe; wine mingw32-make TARGET=windows CFLAGS="$FLAGS -fprofile-generate -lgcov"
[ -e flips.exe ] || exit
echo 'Windows (2/3)'
/usr/bin/time --verbose wine flips.exe --create --bps-delta         profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null
/usr/bin/time --verbose wine flips.exe --create --bps-delta-moremem profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null
echo 'Windows (3/3)'
rm flips.exe; wine mingw32-make TARGET=windows CFLAGS="$FLAGS -fprofile-use -s"

#verify that there are no unexpected dependencies
objdump -p flips.exe | grep 'DLL Name' | \
	grep -Pvi '(msvcrt|advapi32|comctl32|comdlg32|gdi32|kernel32|shell32|user32)' && \
	echo "Invalid dependency" && exit

#test cli binaries
echo "CLI"
make TARGET=cli
[ -e flips ] || exit
rm flips

echo Finishing
7z a floating.zip flips.exe
zipcrush floating.zip
#The random numbers are the size of Lunar IPS v1.02, which I wish to stay below.
#(Or wished a while ago, but my feature set is so much greater than Lunar that I'll accept being bigger.)
echo Size:    $(stat -c%s flips.exe)/155648
echo \(Linux:  $(stat -c%s ~/bin/flips)\)
echo \(Zipped: $(stat -c%s floating.zip)/59881\)

/usr/bin/time --format='time:     %E' flips --create --bps-delta sm64.z64 star.z64 /dev/null > /dev/null
echo 'expected: 0:04.56'

#./special.sh
