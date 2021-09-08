. inc/common.sh
. inc/binlog_common.sh

# Do the same as binlog_7.sh, but using my.cnf

vlog "------- TEST 107.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog-abcd
log-bin-index=$topdir/binlog-dir1/idx
"
INDEX_FILE="$topdir/binlog-dir1/idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="binlog-abcd."
backup_restore


vlog "------- TEST 107.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog-abcd
log-bin-index=$topdir/binlog-dir1/idx
"
INDEX_FILE="$topdir/binlog-dir1/idx.index"
GET_BINLOG_SUFFIX_FROM_SERVER=1
BINLOG_PREFIX="binlog-abcd."
backup_restore


vlog "-------- TEST END"
