. inc/common.sh
. inc/binlog_common.sh

# Do the same as binlog_9.sh, but using my.cnf

# common options
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir2/idx.index
"
INDEX_FILE="$topdir/binlog-dir2/idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="$topdir/binlog-dir1/bin."


vlog "------- TEST 109.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0
backup_restore


vlog "------- TEST 109.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1
backup_restore


vlog "-------- TEST END"
