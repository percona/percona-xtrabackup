# PXB-2854 - Quicklz decompression memory corruption issue

head -c 100000 </dev/urandom > $topdir/payload.bin
qpress $topdir/payload.bin $topdir/payload.qp
printf '\x00\x01\x00' | dd of=$topdir/payload.qp bs=1 seek=8 count=3 conv=notrunc
printf '\x10' | dd of=$topdir/payload.qp bs=1 seek=49 count=1 conv=notrunc
printf -- 'A%.0s' {1..100040} | dd of=$topdir/payload.qp bs=1 seek=50 count=100040 conv=notrunc
xbstream -c $topdir/payload.qp > $topdir/payload.xbstream
run_cmd_expect_failure bash -c "xbstream --decompress -x -C $topdir/ < $topdir/payload.xbstream"
grep -q "compressed file was corrupted - header data size and actual data size mismatch" $OUTFILE
