. inc/common.sh
. inc/binlog_common.sh

# Do the same as binlog_11.sh, but using my.cnf
# PXB-2106 : we have an absolute path for the log-bin and it
# is within the datadir

vlog "------- TEST 111.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$mysql_datadir/bin
log-bin-index=idx
"
INDEX_FILE="idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="$mysql_datadir/bin."
backup_restore


vlog "------- TEST 111.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$mysql_datadir/bin
log-bin-index=idx
"
INDEX_FILE="idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="$mysql_datadir/bin."
backup_restore


vlog "-------- TEST END"
