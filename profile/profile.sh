#!/bin/sh

#profiling plus threading tends to act weirdly, disable it
export OMP_NUM_THREADS=1

FX45=profile/firefox-45.0esr.tar
FX52=profile/firefox-52.0esr.tar

#silly grep because GOutputStream can't write to /dev/null, so write fails
#the error is harmless, but confuses users, so let's hide it
#also ||true to mask Flips itself failing; doesn't really do anything, but no reason not to
$1 --create --bps-delta         $FX45 $FX52 /dev/null | sed 's/.*read-only.*/The patch was created successfully!/' || true
$1 --create --bps-delta-moremem $FX45 $FX52 /dev/null | sed 's/.*read-only.*/The patch was created successfully!/' || true
