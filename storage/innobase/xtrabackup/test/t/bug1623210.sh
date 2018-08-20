#
# Bug 1623210: xtrabackup failed to write metadata but backup still succeeded
# Bug 1557027: Error message about missing metadata is misleading
#

start_server

mkdir $topdir/extra1
mkdir $topdir/extra2
chmod ugoa-w $topdir/extra1

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --backup --target-dir=$topdir/bak1 \
					    --extra-lsndir=$topdir/extra1

xtrabackup --backup --target-dir=$topdir/bak2 --extra-lsndir=$topdir/extra2

echo -n > $topdir/bak2/xtrabackup_checkpoints
run_cmd_expect_failure ${XB_BIN} --prepare --target-dir=$topdir/bak2

rm $topdir/bak2/xtrabackup_checkpoints
run_cmd_expect_failure ${XB_BIN} --prepare --target-dir=$topdir/bak2

cp $topdir/extra2/xtrabackup_checkpoints $topdir/bak2/xtrabackup_checkpoints
xtrabackup --prepare --target-dir=$topdir/bak2
