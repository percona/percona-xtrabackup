. inc/common.sh
. inc/binlog_common.sh

# Only run this test if we are copying the binlog index
# Otherwise PXB will fail the copy-back because we can't
# find the binlog index or binlog files (the binlog
# names are different).

# common options
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=bin
log-bin-index=idx
"
INDEX_FILE="logidx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="logbin."


vlog "------- TEST 202.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0
backup
restore --log-bin=logbin --log-bin-index=logidx


vlog "-------- TEST END"
