# Test incremental backups that use bitmaps with 1KB compressed pages

require_xtradb
is_64bit || skip_test "Disabled on 32-bit hosts due to LP bug #1359182"

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-track-changed-pages=TRUE
"
first_inc_suspend_command="FLUSH CHANGED_PAGE_BITMAPS;"

source t/xb_incremental_compressed.inc

test_incremental_compressed 1

check_bitmap_inc_backup
