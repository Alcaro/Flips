cp flips.exe special.exe
positions=`strings -td flips.exe | grep 'Flips v' | awk '{ print $1 }'`
for pos in $positions; do
	echo 'IPS FLIPPER' | dd if=/dev/stdin of=special.exe bs=1 count=11 conv=notrunc seek=$pos
done
flips --create --bps-delta flips.exe special.exe special.bps
rm special.exe
