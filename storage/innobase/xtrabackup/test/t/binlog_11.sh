. inc/common.sh
. inc/binlog_common.sh

# PXB-2106 : we have an absolute path for the log-bin and it
# is within the datadir

# common options
INDEX_FILE="idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="${mysql_datadir}/bin."


vlog "------- TEST 11.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0
backup_restore --log-bin=$mysql_datadir/bin --log-bin-index=idx


vlog "------- TEST 11.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1
backup_restore --log-bin=$mysql_datadir/bin --log-bin-index=idx


vlog "-------- TEST END"
