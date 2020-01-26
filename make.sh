#!/bin/sh

#This script creates a heavily optimized Linux binary. For debugging or Windows, you're better off using the Makefile directly.

FLAGS='-Wall -Werror -O3 -flto -fuse-linker-plugin -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden'
FLAGS=$FLAGS' -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables'
FLAGS=$FLAGS' -ffunction-sections -fdata-sections -Wl,--gc-sections -fprofile-dir=obj/'
#Linux flags, they don't make sense on Windows
#make-release.sh uses this
LINFLAGS=' -Wl,-z,relro,-z,now,--as-needed,--hash-style=gnu,--relax'

PROFILE=yes
STRIP="-s"

for i in "$@"; do
case "$i" in
  --harden=yes)
    FLAGS=$FLAGS' -fstack-protector-strong -pie -fPIE -D_FORTIFY_SOURCE=2'
    true | gcc -E - -mmitigate-rop > /dev/null 2>&1 &&
      FLAGS=$FLAGS' -mmitigate-rop'
  ;;
  --profile=no|--profile=no-once)
    # With PGO, compilation takes about 75 seconds for me, with a peak memory use of 814MB.
    # Without PGO, it takes 2.5 seconds and 75MB RAM. However, the resulting binary is about 2% slower.
    PROFILE=no
  ;;
  --strip=no)
    STRIP="-g"
  ;;
  --harden=no|--strip=yes|--profile=yes)
  ;;
  *)
  echo "Unknown argument $1; valid arguments are:"
  echo "--harden=no --harden=yes --profile=no --profile=no-once --profile=yes"
  exit 1
  ;;
esac
done

if [ $PROFILE = yes ]; then

echo 'GTK+ (1/3)'
rm obj/* flips; TARGET=gtk make OPTFLAGS="$FLAGS$LINFLAGS -fprofile-generate -lgcov" || exit $?
[ -e flips ] || exit 1
echo 'GTK+ (2/3)'

# These files are downloaded and extracted from
# https://ftp.mozilla.org/pub/firefox/releases/10.0esr/linux-x86_64/en-US/firefox-10.0esr.tar.bz2
# https://ftp.mozilla.org/pub/firefox/releases/17.0esr/linux-x86_64/en-US/firefox-17.0esr.tar.bz2
/usr/bin/time --verbose ./flips --create --bps-delta         profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null
/usr/bin/time --verbose ./flips --create --bps-delta-moremem profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null

echo 'GTK+ (3/3)'
rm flips; TARGET=gtk make OPTFLAGS="$FLAGS$LINFLAGS -fprofile-use" || exit $?
else
rm flips; TARGET=gtk make OPTFLAGS="$FLAGS$LINFLAGS" || exit $?
fi
