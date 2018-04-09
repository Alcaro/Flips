#!/bin/sh

FX45=https://ftp.mozilla.org/pub/firefox/releases/45.0esr/linux-x86_64/en-US/firefox-45.0esr.tar.bz2
FX52=https://ftp.mozilla.org/pub/firefox/releases/52.0esr/linux-x86_64/en-US/firefox-52.0esr.tar.bz2

wget $FX45 -O- | bzcat > profile/firefox-45.0esr.tar || exit $?
wget $FX52 -O- | bzcat > profile/firefox-52.0esr.tar || exit $?
echo y > profile/choice
