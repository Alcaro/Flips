#sorry guys, you'll have to supply these ROMs yourself
#expected files:
# md5                               crc32     filename            source
# 2142a3390ea54ecbfb14caf6b707859d  f3824cbe  2dland-dec2013.smc  https://smwc.me/s/6068
# 8134163af1e658a3f1af130afd66d17a  2144df1c  dl.ups              https://www.zophar.net/translations/snes/english/der-langrisser.html
# fa15ce41d3fd835c379d5d846707c1bd  8577d85f  kamek.smc           https://smwc.me/s/12161
# 91d62c4cb790fc2fb38b10b68616e228  35f9eecc  langrisser.sfc      
# 77e866c718d67f28a83012b29d5501b8  ebca0fab  nsmw-tll.smc        https://smwc.me/s/6410
# b278fec07b672853b9f31667ef9e1ddc  bf1d7697  senate13.smc        https://smwc.me/s/6161
# 20b854b239203baf6c961b850a4a51a2  3ce60709  sm64.z64            
# 84525c7acf9f7c0d869ba1a88062dd81  b61e3965  smwcp.smc           https://smwc.me/s/5420
# dbe1f3c8f3a0b2db52b7d59417891117  a31bead4  smw.smc             
# f3ae75790723c6c753c3583852b10c01  cf940969  star.z64            https://smwc.me/s/13353
#(don't worry too much if you don't get identical checksums, the download links may have updated)

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
