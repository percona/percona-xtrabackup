. inc/common.sh
. inc/binlog_common.sh

vlog "------- TEST 6.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE="log.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="${topdir}/binlog-dir1/bin."
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=log


vlog "------- TEST 6.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE="log.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="${topdir}/binlog-dir1/bin."
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=log


vlog "-------- TEST END"
