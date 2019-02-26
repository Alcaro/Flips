#!/bin/sh

if [ "$HOME" != "/home/alcaro" ]; then
  #This script is for making official releases only.
  #If you're intending to fork Floating IPS, or take over maintenance, you're welcome to edit this script.
  echo "Use ./make.sh instead."
  exit 1
fi

profile/download.sh || exit $?
# . rather than ./make.sh, so $FLAGS remains set
. ./make.sh
mv ./flips ~/bin/flips

#create windows binary
mingwver 64 || true # this is a script that sets the Wine PATH

echo 'Windows (1/3)'
rm obj/* flips.exe; wine mingw32-make TARGET=windows OPTFLAGS="$FLAGS -fprofile-generate -lgcov"
[ -e flips.exe ] || exit
echo 'Windows (2/3)'
profile/profile.sh 'wine flips.exe'
echo 'Windows (3/3)'
rm flips.exe; wine mingw32-make TARGET=windows OPTFLAGS="$FLAGS -fprofile-use"

#verify there are no unexpected dependencies
objdump -p flips.exe | grep 'DLL Name' | \
	grep -Pvi '(msvcrt|advapi32|comctl32|comdlg32|gdi32|kernel32|shell32|user32)' && \
	echo "Invalid dependency" && exit

#test cli binaries
echo "CLI"
make TARGET=cli DIVSUF=no
[ -e flips ] || exit
rm flips

#echo Finishing
#7z a floating.zip flips.exe
#zipcrush floating.zip
##The random numbers are the size of Lunar IPS, which I wish to stay below.
##(Or wished a while ago, but my feature set is so much greater than Lunar that I'll accept being bigger.)
#echo Size:    $(stat -c%s flips.exe)/96768
#echo \(Linux:  $(stat -c%s ~/bin/flips)\)
#echo \(Zipped: $(stat -c%s floating.zip)/59881\)

#./special.sh
