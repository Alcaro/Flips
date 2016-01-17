#clean up
rm flips.exe floating.zip flips rc.o *.gcda

FLAGS='-Wall -Werror -O3 -fomit-frame-pointer -fmerge-all-constants -fno-exceptions -fno-asynchronous-unwind-tables'

#create windows binary
echo 'Windows/Resource (Wine warmup)'
wine windres flips.rc rc.o

echo 'Windows (1/3)'
rm flips.exe; CFLAGS=$FLAGS' -fprofile-generate' wine mingw32-make TARGET=windows LFLAGS='-s -lgcov'
#wine gcc -pipe -std=c99 $FLAGS -fprofile-generate *.c rc.o -mwindows -lgdi32 -lcomdlg32 -lcomctl32 -s -lgcov -oflips.exe
[ -e flips.exe ] || exit
echo 'Windows (2/3)'
profile/profile.sh 'wine flips.exe' NUL
echo 'Windows (3/3)'
rm flips.exe; CFLAGS=$FLAGS' -fprofile-use' wine mingw32-make TARGET=windows LFLAGS='-s'
#wine g++ -pipe -std=c99 $FLAGS -fprofile-use      *.c rc.o -mwindows -lgdi32 -lcomdlg32 -lcomctl32 -s        -oflips.exe
rm *.gcda rc.o

#verify there are no unexpected dependencies
objdump -p flips.exe | grep 'DLL Name' | \
	grep -Pvi '(msvcrt|advapi32|comctl32|comdlg32|gdi32|kernel32|shell32|user32)' && \
	echo "Invalid dependency" && exit

#test cli binaries
echo CLI
rm flips; make TARGET=cli DIVSUF=no
[ -e flips ] || exit

#create linux binary
echo 'GTK+ (1/3)'
rm flips; CFLAGS=$FLAGS' -fprofile-generate' make TARGET=gtk LFLAGS='-s -lgcov'
[ -e flips ] || exit
echo 'GTK+ (2/3)'
profile/profile.sh ./flips
echo 'GTK+ (3/3)'
rm flips; CFLAGS=$FLAGS' -fprofile-use' make TARGET=gtk LFLAGS='-s'
rm *.gcda
mv flips ~/bin/flips # keeping this one for myself

echo Finishing
#compress source 
7z a -mx0 src.zip *.cpp *.h Makefile flips.rc flips.Manifest *.ico make.sh profile/profile.sh profile/profile1.sh special.sh
7z a -mx9 boring.zip license-*.txt
zipcrush boring.zip

7z a floating.zip flips.exe src.zip boring.zip license.txt
zipcrush floating.zip
echo Size:    $(stat -c%s flips.exe)/96768
echo \(Linux:  $(stat -c%s ~/bin/flips)\)
echo \(Zipped: $(stat -c%s floating.zip)/59881\)

./special.sh

rm src.zip boring.zip
