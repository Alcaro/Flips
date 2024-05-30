#!/bin/sh

echo "This script creates a heavily optimized Macos binary. For debugging, you're better off using the Makefile directly."

FLAGS='-Wall -O3 -flto -fomit-frame-pointer -fmerge-all-constants -fvisibility=hidden'
FLAGS=$FLAGS' -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables'
FLAGS=$FLAGS' -ffunction-sections -fdata-sections'

PROFILE=yes

for i in "$@"; do
case "$i" in
  --harden=yes)
    FLAGS=$FLAGS' -fstack-protector-strong -pie -fPIE'
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
  *)
  echo "Unknown argument $1; valid arguments are:"
  echo "--harden=yes --profile=no --cflags=(...) --lflags=(...) --install=yes"
  exit 1
  ;;
esac
done

if [ $PROFILE = yes ]; then

echo 'GTK+ (1/3)'
rm obj/* flips; make CFLAGS="$FLAGS -fprofile-generate=obj/" || exit $?
[ -e flips ] || exit 1

echo 'GTK+ (2/3)'
# These files are downloaded and extracted from
# https://ftp.mozilla.org/pub/firefox/releases/10.0esr/linux-x86_64/en-US/firefox-10.0esr.tar.bz2
# https://ftp.mozilla.org/pub/firefox/releases/17.0esr/linux-x86_64/en-US/firefox-17.0esr.tar.bz2
if [ -x /usr/bin/time ]; then
TIME='/usr/bin/time'
else
TIME=''
fi
export OMP_NUM_THREADS=1 # -fprofile-generate isn't thread safe
$TIME ./flips --create --bps-delta         profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null
$TIME ./flips --create --bps-delta-moremem profile/firefox-10.0esr.tar profile/firefox-17.0esr.tar /dev/null

echo 'GTK+ (3/3)'

# FIXME: Currently this path is hardcoded for CI. May or may not be different on real machines.
/opt/homebrew/opt/llvm/bin/llvm-profdata merge -o default.profdata ./obj/*.profraw

rm flips; make CFLAGS="$FLAGS -fprofile-use" || exit $?
else
rm flips; make CFLAGS="$FLAGS" || exit $?
fi
