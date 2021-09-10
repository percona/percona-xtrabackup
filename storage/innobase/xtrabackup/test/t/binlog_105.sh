. inc/common.sh
. inc/binlog_common.sh

# Do the same as binlog_5.sh, but using my.cnf

# common options
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog123
log-bin-index=binlog898.index
"
INDEX_FILE="binlog898.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="./binlog123."


vlog "------- TEST 105.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0
backup_restore


vlog "------- TEST 105.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1
backup_restore


vlog "-------- TEST END"
