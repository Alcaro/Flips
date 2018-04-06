#!/bin/sh

#profiling plus threading tends to act weirdly, disable it
export OMP_NUM_THREADS=1

FLIPS=$1
TEMP=$(mktemp)

profile_one()
{
echo "$1/7"
SRC=$2
$FLIPS --apply                      $3    $SRC $TEMP
$FLIPS --create --bps-delta         $SRC $TEMP $TEMP
$FLIPS --apply                      $TEMP $SRC $TEMP
$FLIPS --create --bps-delta-moremem $SRC $TEMP $TEMP
$FLIPS --apply                      $TEMP $SRC $TEMP
$FLIPS --create --bps-linear        $SRC $TEMP $TEMP
$FLIPS --apply                      $TEMP $SRC $TEMP
$FLIPS --create --ips               $SRC $TEMP $TEMP
$FLIPS --apply                      $TEMP $SRC $TEMP
}

profile_one 1 profile/smw.sfc        profile/smw-2dland.bps
profile_one 2 profile/smw.sfc        profile/smw-kamek.bps
profile_one 3 profile/smw.sfc        profile/smw-senate.bps
profile_one 4 profile/smw.sfc        profile/smw-smwcp.bps
profile_one 5 profile/smw.sfc        profile/smw-tll.bps
profile_one 6 profile/langrisser.sfc profile/langrisser-eng.ups
profile_one 7 profile/sm64.z64       profile/sm64-star.bps

rm $TEMP
