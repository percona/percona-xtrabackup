. inc/common.sh
. inc/binlog_common.sh

# Do the same as binlog_2.sh, but using my.cnf

vlog "------- TEST 102.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin
"
INDEX_FILE="mysql-bin.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="./mysql-bin."
backup_restore


vlog "------- TEST 102.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin
"
INDEX_FILE="mysql-bin.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="./mysql-bin."
backup_restore


vlog "-------- TEST END"
