. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 107 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog-abcd
log-bin-index=$topdir/binlog-dir1/idx
"
INDEX_FILE="$topdir/binlog-dir1/idx.index"
FILES="binlog-abcd.000004"
backup_restore
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
