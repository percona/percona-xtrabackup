. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 2 -------"
INDEX_FILE="mysql-bin.index"
FILES="mysql-bin.000004"
backup_restore --log-bin
