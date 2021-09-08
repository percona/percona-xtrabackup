. inc/common.sh
. inc/binlog_common.sh

# Test for binlog name with two periods
# Entered as a MySQL bug : https://bugs.mysql.com/bug.php?id=103404
# MySQL creates a single unnumbered binlog file

vlog "------- TEST 302.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE="my.index"

if is_server_version_higher_than 5.7.0; then
  GET_BINLOG_SUFFIX_FROM_SERVER=0
  BINLOG_FILENAME="my.log"
else
  # If this is using a server with 5.6, then
  # we can use the normal rules.
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="my."
fi
backup_restore --log-bin=my.log.bin


vlog "------- TEST 302.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE="my.index"

if is_server_version_higher_than 5.7.0; then
  GET_BINLOG_SUFFIX_FROM_SERVER=0
  BINLOG_FILENAME="my.log"
else
  # If this is using a server with 5.6, then
  # we can use the normal rules.
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="my."
fi
backup_restore --log-bin=my.log.bin


vlog "-------- TEST END"
