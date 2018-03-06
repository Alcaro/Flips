if [ "$1" = 'Debug' ]; then
    MODE=$1
else
    MODE='Release'
fi

BINPATH='./bin/'$MODE'/'
TEMPPATH='./obj/'$MODE'/'
MAKEFLAGS='MODE='$MODE
FLAGS='-Wall -Werror -O3 -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden'
FLAGS+=' -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables'
FLAGS+=' -ffunction-sections -fdata-sections -fprofile-dir='"$TEMPPATH"' -Wl,--gc-sections'

if [ "$MODE" = "Debug" ]; then
    FLAGS+= ' -g'
fi

#clean up
rm -rf ${BINPATH} ${TEMPPATH}
mkdir -p ${BINPATH} ${TEMPPATH}

##create windows binary
#echo 'Windows/Resource (Wine warmup)'
#mingwver 5.3
#wine windres ${TEMPPATH}flips.rc ${TEMPPATH}rc.o
#
#echo 'Windows (1/3)'
#rm .${BINPATH}flips.exe; CFLAGS=$FLAGS' -fprofile-generate' wine mingw32-make $MAKEFLAGS TARGET=windows LFLAGS='-lgcov'
#[ -e ${BINPATH}flips.exe ] || exit
#echo 'Windows (2/3)'
#profile/profile.sh 'wine ${BINPATH}flips.exe' NUL
#echo 'Windows (3/3)'
#rm ${BINPATH}flips.exe; CFLAGS=$FLAGS' -fprofile-use' wine mingw32-make $MAKEFLAGS TARGET=windows LFLAGS=''
#rm ${TEMPPATH}*.gcda ${TEMPPATH}rc.o
#
##verify there are no unexpected dependencies
#objdump -p ${BINPATH}flips.exe | grep 'DLL Name' | \
#	grep -Pvi '(msvcrt|advapi32|comctl32|comdlg32|gdi32|kernel32|shell32|user32)' && \
#	echo "Invalid dependency" && exit
#
##test cli binaries
#echo CLI
#rm ${BINPATH}flips; make $MAKEFLAGS TARGET=cli DIVSUF=no
#[ -e ${BINPATH}flips ] || exit

#create linux binary
echo 'GTK+ (1/3)'
rm ${BINPATH}flips; CFLAGS=$FLAGS' -fprofile-generate' make $MAKEFLAGS TARGET=gtk LFLAGS='-lgcov'
[ -e ${BINPATH}flips ] || exit
echo 'GTK+ (2/3)'
profile/profile.sh ${BINPATH}flips
echo 'GTK+ (3/3)'
rm ${BINPATH}flips; CFLAGS=$FLAGS' -fprofile-use' make $MAKEFLAGS TARGET=gtk LFLAGS=''
rm ${TEMPPATH}*.gcda

#echo Finishing
##compress source 
#7z a floating.zip ${BINPATH}flips.exe
#zipcrush floating.zip
#echo Size:    $(stat -c%s flips.exe)/96768
#echo \(Linux:  $(stat -c%s ~/bin/flips)\)
#echo \(Zipped: $(stat -c%s floating.zip)/59881\)
#
#./special.sh
#
#rm src.zip boring.zip
