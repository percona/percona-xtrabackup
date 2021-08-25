. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 203 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir1/bin
"
INDEX_FILE="$topdir/binlog-dir2/bin.index"
FILES="$topdir/binlog-dir1/logbin.000004"
backup
restore --log-bin=$topdir/binlog-dir1/logbin --log-bin-index=$topdir/binlog-dir2/bin.index
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
