#!/bin/sh

#profiling plus threading tends to act weirdly, disable it
export OMP_NUM_THREADS=1

FX45=profile/firefox-45.0esr.tar
FX52=profile/firefox-52.0esr.tar

$1 --create --bps-delta         $FX45 $FX52 /dev/null
$1 --create --bps-delta-moremem $FX45 $FX52 /dev/null
