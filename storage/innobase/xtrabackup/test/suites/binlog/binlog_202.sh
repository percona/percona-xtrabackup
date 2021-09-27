. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 202 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=bin
log-bin-index=idx
"
INDEX_FILE="logidx.index"
FILES="logbin.000004"
backup
restore --log-bin=logbin --log-bin-index=logidx
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
