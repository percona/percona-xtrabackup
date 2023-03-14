# Test for incremental backups that use page tracking

is_64bit || skip_test "Disabled on 32-bit hosts due to LP bug #1359182"

ib_inc_extra_args=--page-tracking
ib_full_backup_extra_args=--page-tracking

. inc/ib_incremental_common.sh

check_pagetracking_inc_backup
