#!/bin/sh

#profiling plus threading tends to act weirdly, disable it
export OMP_NUM_THREADS=1

$1 --create --bps-delta         profile/firefox-45.0esr.tar profile/firefox-52.0esr.tar /dev/null
$1 --create --bps-delta-moremem profile/firefox-45.0esr.tar profile/firefox-52.0esr.tar /dev/null
