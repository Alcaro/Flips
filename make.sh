#clean up
rm flips.exe floating.zip flips rc.o *.gcda

FLAGS='-Wall -Werror -O3 -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden'
FLAGS+=' -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables'
FLAGS+=' -ffunction-sections -fdata-sections -Wl,--gc-sections'

##create windows binary
#echo 'Windows/Resource (Wine warmup)'
#mingwver 5.3
#wine windres flips.rc rc.o
#
#echo 'Windows (1/3)'
#rm flips.exe; CFLAGS=$FLAGS' -fprofile-generate' wine mingw32-make TARGET=windows LFLAGS='-lgcov'
#[ -e flips.exe ] || exit
#echo 'Windows (2/3)'
#profile/profile.sh 'wine flips.exe' NUL
#echo 'Windows (3/3)'
#rm flips.exe; CFLAGS=$FLAGS' -fprofile-use' wine mingw32-make TARGET=windows LFLAGS=''
#rm *.gcda rc.o
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
echo 'GTK+ (1/3)'
rm flips; CFLAGS=$FLAGS' -fprofile-generate' make TARGET=gtk LFLAGS='-lgcov'
[ -e flips ] || exit
echo 'GTK+ (2/3)'
profile/profile.sh ./flips
echo 'GTK+ (3/3)'
rm flips; CFLAGS=$FLAGS' -fprofile-use' make TARGET=gtk LFLAGS=''
rm *.gcda
mv flips ~/bin/flips # keeping this one for myself

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
