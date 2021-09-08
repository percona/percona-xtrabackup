. inc/common.sh
. inc/binlog_common.sh

vlog "------- TEST 2.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE="mysql-bin.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="mysql-bin."
backup_restore --log-bin


vlog "------- TEST 2.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE="mysql-bin.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="mysql-bin."
backup_restore --log-bin


vlog "-------- TEST END"
