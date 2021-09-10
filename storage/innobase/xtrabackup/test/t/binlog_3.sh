. inc/common.sh
. inc/binlog_common.sh

# common options
INDEX_FILE="binlog123.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="binlog123."


vlog "------- TEST 3.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0
backup_restore --log-bin=binlog123


vlog "------- TEST 3.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1
backup_restore --log-bin=binlog123

vlog "-------- TEST END"
