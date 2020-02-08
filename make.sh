#!/bin/sh

#This script creates a heavily optimized Linux binary. For debugging or Windows, you're better off using the Makefile directly.

PREFIX=/usr

FLAGS='-Wall -Werror -O3 -flto -fuse-linker-plugin -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden'
FLAGS=$FLAGS' -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables'
FLAGS=$FLAGS' -ffunction-sections -fdata-sections -Wl,--gc-sections -fprofile-dir=obj/'
#Linux flags, they don't make sense on Windows
#make-maintainer.sh uses this
LINFLAGS=' -Wl,-z,relro,-z,now,--as-needed,--hash-style=gnu,--relax'

PROFILE=yes

INSTALL=no

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
  --cflags=*|--lflags=*)
    FLAGS=$FLAGS" ${i#*=}"
  ;;
  --harden=no|--profile=yes)
  ;;
  --install=yes)
    INSTALL=yes
  ;;
  --flatpak=yes)
    PREFIX=/app
    FLAGS=$FLAGS" -DFLATPAK"
  ;;
  *)
  echo "Unknown argument $1; valid arguments are:"
  echo "--harden=yes --profile=no --cflags=(...) --lflags=(...) --install=yes --flatpak=yes"
  exit 1
  ;;
esac
done

if [ $INSTALL = yes ]; then

echo 'GTK+ (install)'
TARGET=gtk make install PREFIX=$PREFIX || exit $?

elif [ $PROFILE = yes ]; then

echo 'GTK+ (1/3)'
rm obj/* flips; TARGET=gtk make PREFIX=$PREFIX CFLAGS="$FLAGS$LINFLAGS -fprofile-generate -lgcov" || exit $?
[ -e flips ] || exit 1
echo 'GTK+ (2/3)'

# These files are downloaded and extracted from
# https://ftp.mozilla.org/pub/firefox/releases/10.0esr/linux-x86_64/en-US/firefox-10.0esr.tar.bz2
# https://ftp.mozilla.org/pub/firefox/releases/17.0esr/linux-x86_64/en-US/firefox-17.0esr.tar.bz2
if [ -x /usr/bin/time ]; then
TIME='/usr/bin/time --verbose'
else
TIME=''
fi
$TIME ./flips --create --bps-delta         profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null
$TIME ./flips --create --bps-delta-moremem profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null

echo 'GTK+ (3/3)'
rm flips; TARGET=gtk make PREFIX=$PREFIX CFLAGS="$FLAGS$LINFLAGS -fprofile-use" || exit $?
else
rm flips; TARGET=gtk make PREFIX=$PREFIX CFLAGS="$FLAGS$LINFLAGS" || exit $?
fi
