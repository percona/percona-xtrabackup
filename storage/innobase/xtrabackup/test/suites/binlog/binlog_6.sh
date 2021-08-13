. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 6 -------"
INDEX_FILE="log.index"
FILES="$topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=log
