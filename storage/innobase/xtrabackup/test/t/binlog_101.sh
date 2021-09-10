. inc/common.sh
. inc/binlog_common.sh

# Run the backup/restore test with skip-log-bin set
# (so binlogs are not generated)
#
# Do the same, but using my.cnf

# common options
MYSQLD_EXTRA_MY_CNF_OPTS="
skip-log-bin
"
INDEX_FILE=""
BINLOG_FILENAME=""
GET_BINLOG_SUFFIX_FROM_SERVER=0


vlog "------- TEST 101.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0
backup_restore


vlog "------- TEST 101.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1
backup_restore


vlog "-------- TEST END"
