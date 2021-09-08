. inc/common.sh
. inc/binlog_common.sh

# PXB-1810 - restore binlog to the different location


vlog "------- TEST 201.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir1/idx
"
INDEX_FILE="idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="bin."
backup
restore --log-bin=bin --log-bin-index=idx


vlog "------- TEST 201.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir1/idx
"
INDEX_FILE="idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="bin."
backup
restore --log-bin=bin --log-bin-index=idx


vlog "-------- TEST END"
