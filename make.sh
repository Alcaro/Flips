#!/bin/sh

#This script creates a heavily optimized binary. For debugging, you're better off using 'make'.

FLAGS='-Wall -Werror -O3 -s -flto -fuse-linker-plugin -fweb -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden'
FLAGS=$FLAGS' -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables'
FLAGS=$FLAGS' -ffunction-sections -fdata-sections -Wl,--gc-sections -fprofile-dir=obj/'
FLAGS=$FLAGS' -Wl,-z,relro,-z,now,--as-needed,--hash-style=gnu,--relax'

for i in "$@"; do
case "$i" in
  --harden=no)
  ;;
  --harden=yes)
    FLAGS=$FLAGS' -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4 -pie -fPIE -D_FORTIFY_SOURCE=2'
    true | gcc -E - -mmitigate-rop > /dev/null 2>&1 &&
      FLAGS=$FLAGS' -mmitigate-rop'
  ;;
  --profile=no)
  echo n > profile/choice
  ;;
  --profile=yes)
  if [ ! -e profile/firefox-10.0esr.tar ]; then
    profile/download.sh
  fi
  ;;
  *)    # unknown option
  echo "Unknown argument $1; valid arguments are: --harden=no --harden=yes --profile=no --profile=yes"
  exit 1
  ;;
esac
done


#if old profile data is present, download the new one
if [ -e profile/firefox-45.0esr.tar ]; then
  rm profile/firefox-45.0esr.tar profile/firefox-52.0esr.tar
  touch profile/firefox-10.0esr.tar
fi

if [ ! -e profile/choice ]; then
  while true; do
    read -p "Do you wish to use profile-guided optimization? This will download 40MB data from the internet and use 90MB disk space, and 800MB RAM during compilation. (y/n)" yn
    case $yn in
      [Yy]* ) profile/download.sh; break;;
      [Nn]* ) echo n > profile/choice; break;;
      * ) echo "Please answer yes or no."; exit 1;;
    esac
  done
fi

#if download was aborted, resume it
if [ -e profile/firefox-10.0esr.tar ]; then
  profile/download.sh
fi

#clean up
rm flips flips.exe floating.zip obj/*

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
if [ -e profile/firefox-10.0esr.tar ]; then
echo 'GTK+ (1/3)'
rm flips; TARGET=gtk make OPTFLAGS="$FLAGS -fprofile-generate -lgcov" || exit $?
[ -e flips ] || exit 1
echo 'GTK+ (2/3)'
profile/profile.sh ./flips
echo 'GTK+ (3/3)'
rm flips; TARGET=gtk make OPTFLAGS="$FLAGS -fprofile-use"
#mv flips '~/bin/flips'
else
rm flips; TARGET=gtk make OPTFLAGS="$FLAGS" || exit $?
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
