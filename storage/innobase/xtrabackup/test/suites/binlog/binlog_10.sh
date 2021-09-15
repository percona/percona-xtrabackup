. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 10 -------"
INDEX_FILE="$topdir/binlog-dir1/bin.index"
FILES="$topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin
