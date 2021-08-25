. inc/common.sh
. inc/binlog_common.sh
# do the same, but with updating my.cnf

vlog "------- TEST 101 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
skip-log-bin
"
INDEX_FILE=""
FILES=""
backup_restore
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
