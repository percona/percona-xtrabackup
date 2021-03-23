# Test for fast incremental backups with the --incremental-lsn option

is_64bit || skip_test "Disabled on 32-bit hosts due to LP bug #1359182"

ib_inc_extra_args=--page-tracking
ib_full_backup_extra_args=--page-tracking
ib_inc_use_lsn=1

. inc/ib_incremental_common.sh

check_pagetracking_inc_backup
