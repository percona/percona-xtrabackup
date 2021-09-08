. inc/common.sh
. inc/binlog_common.sh

# Test for binlog name with a single period

vlog "------- TEST 301.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE="my.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="my."
backup_restore --log-bin=my.bin


vlog "------- TEST 301.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE="my.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="my."
backup_restore --log-bin=my.bin


vlog "-------- TEST END"
