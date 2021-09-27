. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 105 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog123
log-bin-index=binlog898.index
"
INDEX_FILE="binlog898.index"
FILES="binlog123.000004"
backup_restore
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
