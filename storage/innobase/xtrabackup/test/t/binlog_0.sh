. inc/common.sh
. inc/binlog_common.sh

# Run the backup/restore test without the log-bin variable set
# in the config file.


vlog "------- TEST 0.1 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

INDEX_FILE=""
BINLOG_FILENAME=""
GET_BINLOG_SUFFIX_FROM_SERVER=0
MYSQLD_LOGBIN_OPTION=""
backup_restore
unset MYSQLD_LOGBIN_OPTION


vlog "------- TEST 0.2 -------"
REMOVE_INDEX_FILE_BEFORE_RESTORE=1

INDEX_FILE=""
BINLOG_FILENAME=""
GET_BINLOG_SUFFIX_FROM_SERVER=0
MYSQLD_LOGBIN_OPTION=""
backup_restore
unset MYSQLD_LOGBIN_OPTION

vlog "-------- TEST END"
