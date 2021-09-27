. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 9 -------"
INDEX_FILE="$topdir/binlog-dir2/idx.index"
FILES="$topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=$topdir/binlog-dir2/idx.index
