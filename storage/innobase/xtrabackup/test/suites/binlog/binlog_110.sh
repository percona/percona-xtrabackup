. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 110 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
"
INDEX_FILE="$topdir/binlog-dir1/bin.index"
FILES="$topdir/binlog-dir1/bin.000004"
backup_restore --log-bin=$topdir/binlog-dir1/bin
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
