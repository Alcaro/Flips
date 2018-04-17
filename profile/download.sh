#!/bin/sh

SRC=profile/firefox-45.0esr.tar
DST=profile/firefox-52.0esr.tar
SRCSIZE=101529600
DSTSIZE=113336320
SRCURL=https://ftp.mozilla.org/pub/firefox/releases/45.0esr/linux-x86_64/en-US/firefox-45.0esr.tar.bz2
DSTURL=https://ftp.mozilla.org/pub/firefox/releases/52.0esr/linux-x86_64/en-US/firefox-52.0esr.tar.bz2

if [ ! -f $SRC ] || [ $(wc -c <$SRC) -lt $SRCSIZE ]; then
  wget $SRCURL -O- | bzcat > $SRC || exit $?
fi
if [ ! -f $DST ] || [ $(wc -c <$DST) -lt $DSTSIZE ]; then
  wget $DSTURL -O- | bzcat > $DST || exit $?
fi

echo y > profile/choice
