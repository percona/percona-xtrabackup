. inc/common.sh
. inc/binlog_common.sh
vlog "------- TEST 11 -------"
# PXB-2106 : we have an absolute path for the log-bin and it
# is within the datadir
INDEX_FILE="idx.index"
FILES="$mysql_datadir/bin.000003"
backup_restore --log-bin=$mysql_datadir/bin --log-bin-index=idx
