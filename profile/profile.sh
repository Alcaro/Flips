#sorry guys, you'll have to supply the ROMs yourself.
#not sure if these compile scripts work for anyone else, either.
OMP_NUM_THREADS=1 $1 --apply profile/dl.ups profile/langrisser.sfc profile/tmp
echo 1/6
profile/profile1.sh "$1" profile/smw.smc profile/2dland-dec2013.smc
echo 2/6
profile/profile1.sh "$1" profile/smw.smc profile/smwcp.smc
echo 3/6
profile/profile1.sh "$1" profile/smw.smc profile/nsmw-tll.smc
echo 4/6
profile/profile1.sh "$1" profile/smw.smc profile/senate13.smc
echo 5/6
profile/profile1.sh "$1" profile/smw.smc profile/kamek.smc
echo 6/6
profile/profile1.sh "$1" profile/sm64.z64 profile/star.z64
rm profile/tmp
