. inc/common.sh
. inc/binlog_common.sh
# PXB-1810 - restore binlog to the different location

vlog "------- TEST 201 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir1/idx
"
INDEX_FILE="idx.index"
FILES="bin.000004"
backup
restore --log-bin=bin --log-bin-index=idx
# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""
