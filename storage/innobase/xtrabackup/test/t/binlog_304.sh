. inc/common.sh
. inc/binlog_common.sh

# Test for binlog index name with a two periods

# common options
INDEX_FILE="my.sql.idx"

# The log-bin-index must also be specified in the config file
# becauxe the server will not provide the correct value
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=my.sql.bin
log-bin-index=my.sql.idx
"
if is_server_version_higher_than 5.7.0; then
  GET_BINLOG_SUFFIX_FROM_SERVER=0
  BINLOG_FILENAME="my.sql"
else
  # If this is using a server with 5.6, then
  # we can use the normal rules.
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="my."
fi


vlog "------- TEST 304.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0
backup_restore


vlog "------- TEST 304.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1
backup_restore


vlog "-------- TEST END"
