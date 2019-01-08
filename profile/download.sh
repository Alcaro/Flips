#!/bin/sh

#why firefox? I need two large, freely available, mostly similar, binary files; two versions of an executable would be a good choice
#why 10/17? older firefoxes are smaller, so profiling needs less RAM; but I also want to skip a few versions,
# to ensure they're not too similar; and I want something chosen with a minimum of arbitrariness. The oldest ESRs satisfy those constraints.
SRC=profile/firefox-10.0esr.tar
DST=profile/firefox-17.0esr.tar
SRCSIZE=39526400
DSTSIZE=49192960
SRCURL=https://ftp.mozilla.org/pub/firefox/releases/10.0esr/linux-x86_64/en-US/firefox-10.0esr.tar.bz2
DSTURL=https://ftp.mozilla.org/pub/firefox/releases/17.0esr/linux-x86_64/en-US/firefox-17.0esr.tar.bz2

if [ ! -f $SRC ] || [ $(wc -c <$SRC) -lt $SRCSIZE ]; then
  wget $SRCURL -O- | bzcat > $SRC || curl $SRCURL | bzcat > $SRC || exit $?
fi
if [ ! -f $DST ] || [ $(wc -c <$DST) -lt $DSTSIZE ]; then
  wget $DSTURL -O- | bzcat > $DST || curl $DSTURL | bzcat > $DST || exit $?
fi

echo y > profile/choice
