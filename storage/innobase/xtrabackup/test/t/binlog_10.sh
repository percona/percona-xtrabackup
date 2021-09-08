. inc/common.sh
. inc/binlog_common.sh

vlog "------- TEST 10.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE="$topdir/binlog-dir1/bin.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="${topdir}/binlog-dir1/bin."
backup_restore --log-bin=$topdir/binlog-dir1/bin


vlog "------- TEST 10.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE="$topdir/binlog-dir1/bin.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="${topdir}/binlog-dir1/bin."
backup_restore --log-bin=$topdir/binlog-dir1/bin


vlog "-------- TEST END"
