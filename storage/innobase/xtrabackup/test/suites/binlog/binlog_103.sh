. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 103 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog123
"
INDEX_FILE="binlog123.index"
FILES="binlog123.000004"
backup_restore
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
