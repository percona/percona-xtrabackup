. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 102 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin
"
INDEX_FILE="mysql-bin.index"
FILES="mysql-bin.000004"
backup_restore
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
