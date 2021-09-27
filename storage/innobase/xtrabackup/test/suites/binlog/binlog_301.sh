. inc/common.sh
. inc/binlog_common.sh
# Test for binlog name with a single period
vlog "------- TEST 301 -------"
INDEX_FILE="my.index"
FILES="my.000003"
backup_restore --log-bin=my.bin
