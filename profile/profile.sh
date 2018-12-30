#!/bin/sh

#profiling plus threading tends to act weirdly, disable it
export OMP_NUM_THREADS=1

SRC=profile/firefox-10.0esr.tar
DST=profile/firefox-17.0esr.tar

/usr/bin/time --verbose $1 --create --bps-delta         $SRC $DST /dev/null
/usr/bin/time --verbose $1 --create --bps-delta-moremem $SRC $DST /dev/null
