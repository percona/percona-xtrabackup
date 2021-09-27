. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 7 -------"
INDEX_FILE="$topdir/binlog-dir1/idx.index"
FILES="binlog-abcd.000003"
backup_restore --log-bin=binlog-abcd --log-bin-index=$topdir/binlog-dir1/idx
