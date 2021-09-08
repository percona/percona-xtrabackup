. inc/common.sh
. inc/binlog_common.sh

# Run the backup/restore test with skip-log-bin set
# (so binlogs are not generated)

vlog "------- TEST 1.1.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE=""
BINLOG_FILENAME=""
GET_BINLOG_SUFFIX_FROM_SERVER=0
backup_restore --skip-log-bin


vlog "------- TEST 1.1.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE=""
BINLOG_FILENAME=""
GET_BINLOG_SUFFIX_FROM_SERVER=0
MYSQLD_LOGBIN_OPTION=""
backup_restore --skip-log-bin
unset MYSQLD_LOGBIN_OPTION



vlog "------- TEST 1.2.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE=""
BINLOG_FILENAME=""
GET_BINLOG_SUFFIX_FROM_SERVER=0
backup_restore --skip-log-bin


vlog "------- TEST 1.2.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE=""
BINLOG_FILENAME=""
GET_BINLOG_SUFFIX_FROM_SERVER=0
MYSQLD_LOGBIN_OPTION=""
backup_restore --skip-log-bin
unset MYSQLD_LOGBIN_OPTION


vlog "-------- TEST END"
