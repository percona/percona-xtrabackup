. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 111 -------"
# PXB-2106 : we have an absolute path for the log-bin and it
# is within the datadir
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$mysql_datadir/bin
log-bin-index=idx
"
INDEX_FILE="idx.index"
FILES="$mysql_datadir/bin.000004"
backup_restore
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
