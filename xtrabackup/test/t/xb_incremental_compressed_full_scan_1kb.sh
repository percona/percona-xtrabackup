# Test incremental backups that do full data scans with 1KB compressed pages

first_inc_suspend_command=

source t/xb_incremental_compressed.inc

test_incremental_compressed 1

check_full_scan_inc_backup
