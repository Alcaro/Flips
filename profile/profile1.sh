OMP_NUM_THREADS=1 $1 --create --ips $2 $3 profile/tmp
OMP_NUM_THREADS=1 $1 --apply  --ips profile/tmp $2 profile/tmp

OMP_NUM_THREADS=1 $1 --create --bps-linear $2 $3 profile/tmp
OMP_NUM_THREADS=1 $1 --apply  --bps-linear profile/tmp $2 profile/tmp

OMP_NUM_THREADS=1 $1 --create --bps-delta $2 $3 profile/tmp
OMP_NUM_THREADS=1 $1 --create --bps-delta-moremem $2 $3 profile/tmp
OMP_NUM_THREADS=1 $1 --apply  --bps-delta profile/tmp $2 profile/tmp
