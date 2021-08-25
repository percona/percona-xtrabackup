. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 4 -------"
INDEX_FILE="binlog898.index"
FILES="binlog123.000003"
backup_restore --log-bin=binlog123 --log-bin-index=binlog898
