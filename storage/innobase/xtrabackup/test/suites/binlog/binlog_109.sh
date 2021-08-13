. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 109 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir2/idx.index
"
INDEX_FILE="$topdir/binlog-dir2/idx.index"
FILES="$topdir/binlog-dir1/bin.000004"
backup_restore
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
