. inc/common.sh
. inc/binlog_common.sh

# Only run this test if we are copying the binlog index
# Otherwise PXB will fail the copy-back because we can't
# find the binlog index or binlog files (the binlog
# names are different).

# common options
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir1/bin
"
INDEX_FILE="$topdir/binlog-dir2/bin.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="$topdir/binlog-dir1/logbin."


vlog "------- TEST 203.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0
backup
restore --log-bin=$topdir/binlog-dir1/logbin --log-bin-index=$topdir/binlog-dir2/bin.index


vlog "-------- TEST END"
