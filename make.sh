#!/bin/sh

#This script creates a heavily optimized binary. For debugging, you're better off using 'make'.

FLAGS='-Wall -Werror -O3 -s -flto -fuse-linker-plugin -fweb -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden'
FLAGS=$FLAGS' -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables'
FLAGS=$FLAGS' -ffunction-sections -fdata-sections -Wl,--gc-sections -fprofile-dir=obj/'
#Linux flags - they don't make sense on Windows
#make-release.sh uses this
LINFLAGS=' -Wl,-z,relro,-z,now,--as-needed,--hash-style=gnu,--relax'

#if old profile data is present, download the new one instead
#do it before checking for arguments, so --profile=no with old data existing doesn't do anything silly
if [ -e profile/firefox-45.0esr.tar ]; then
  rm profile/firefox-45.0esr.tar profile/firefox-52.0esr.tar
  touch profile/firefox-10.0esr.tar
fi

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
  rm profile/firefox-10.0esr.tar
  rm profile/firefox-17.0esr.tar
  ;;
  --profile=yes)
  if [ ! -e profile/firefox-10.0esr.tar ]; then
    profile/download.sh || exit $?
  fi
  ;;
  *)
  echo "Unknown argument $1; valid arguments are: --harden=no --harden=yes --profile=no --profile=yes"
  exit 1
  ;;
esac
done


if [ ! -e profile/choice ]; then
  while true; do
    #              1         2         3         4         5         6         7         8
    #     12345678901234567890123456789012345678901234567890123456789012345678901234567890
    echo "Do you wish to use profile-guided optimization? This will download 40MB"
    echo "data from the internet and use 90MB disk space, and use 800MB RAM during"
    echo "compilation. The result is typically 2% faster."
    read -p "(y/n)" yn || exit $?
    case $yn in
      [Yy]*) profile/download.sh || exit $?; break;;
      [Nn]*) echo n > profile/choice; break;;
      *) echo "Please answer yes or no.";;
    esac
  done
fi

#if download was aborted, resume it
if [ -e profile/firefox-10.0esr.tar ]; then
  profile/download.sh || exit $?
fi

rm flips flips.exe floating.zip obj/*

if [ -e profile/firefox-10.0esr.tar ]; then
echo 'GTK+ (1/3)'
rm obj/* flips; TARGET=gtk make OPTFLAGS="$FLAGS$LINFLAGS -fprofile-generate -lgcov" || exit $?
[ -e flips ] || exit 1
echo 'GTK+ (2/3)'
profile/profile.sh ./flips
echo 'GTK+ (3/3)'
rm flips; TARGET=gtk make OPTFLAGS="$FLAGS$LINFLAGS -fprofile-use"
else
rm flips; TARGET=gtk make OPTFLAGS="$FLAGS$LINFLAGS" || exit $?
fi
