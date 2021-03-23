# Test incremental backups that use pagetracking with 1KB compressed pages

ib_inc_extra_args=" --page-tracking"
ib_full_backup_extra_args=" --page-tracking"
first_inc_suspend_command=

source t/xb_incremental_compressed.inc

test_incremental_compressed 8

check_pagetracking_inc_backup
