# Test incremental backups that do full data scans with 2KB compressed pages

# Skip until https://bugs.launchpad.net/percona-xtrabackup/+bug/1691083 is fixed.
# For details please see:
# https://blueprints.launchpad.net/percona-xtrabackup/+spec/test-framework-mariadb-support
is_mariadb && skip_test "disabled for MariaDB"

first_inc_suspend_command=

source t/xb_incremental_compressed.inc

test_incremental_compressed 2

check_full_scan_inc_backup
