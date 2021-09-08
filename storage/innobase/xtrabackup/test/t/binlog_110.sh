. inc/common.sh
. inc/binlog_common.sh

# Do the same as binlog_10.sh, but using my.cnf

vlog "------- TEST 110.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
"
INDEX_FILE="$topdir/binlog-dir1/bin.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="$topdir/binlog-dir1/bin."
backup_restore


vlog "------- TEST 110.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
"
INDEX_FILE="$topdir/binlog-dir1/bin.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="$topdir/binlog-dir1/bin."
backup_restore


vlog "-------- TEST END"
