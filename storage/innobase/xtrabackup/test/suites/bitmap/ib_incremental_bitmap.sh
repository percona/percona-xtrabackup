# Test for incremental backups that use changed page bitmaps

require_xtradb
is_64bit || skip_test "Disabled on 32-bit hosts due to LP bug #1359182"

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-track-changed-pages=TRUE
"
ib_inc_extra_args=

. inc/ib_incremental_common.sh

check_bitmap_inc_backup
