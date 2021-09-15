. inc/common.sh
. inc/binlog_common.sh
# Test for binlog name with two periods
# Entered as a MySQL bug : https://bugs.mysql.com/bug.php?id=103404
# MySQL creates a single unnumbered binlog file
vlog "------- TEST 302 -------"
INDEX_FILE="my.index"
FILES="my.log"
backup_restore --log-bin=my.log.bin
