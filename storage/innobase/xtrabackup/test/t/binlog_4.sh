. inc/common.sh
. inc/binlog_common.sh

vlog "------- TEST 4.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE="binlog898.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="binlog123."
backup_restore --log-bin=binlog123 --log-bin-index=binlog898


vlog "------- TEST 4.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE="binlog898.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="binlog123."
backup_restore --log-bin=binlog123 --log-bin-index=binlog898

vlog "-------- TEST END"
