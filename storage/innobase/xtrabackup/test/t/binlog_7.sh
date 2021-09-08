. inc/common.sh
. inc/binlog_common.sh

vlog "------- TEST 7.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE="$topdir/binlog-dir1/idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="binlog-abcd."
backup_restore --log-bin=binlog-abcd --log-bin-index=$topdir/binlog-dir1/idx


vlog "------- TEST 7.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE="$topdir/binlog-dir1/idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="binlog-abcd."
backup_restore --log-bin=binlog-abcd --log-bin-index=$topdir/binlog-dir1/idx


vlog "-------- TEST END"
