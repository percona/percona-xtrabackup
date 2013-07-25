# Test incremental backups that use bitmaps with 4KB compressed pages

require_xtradb

# The test is disabled for Percona XtraDB Cluster until a version
# with bitmap user requests is released (containing PS 5.5.29-30.0 
# or later)
set +e
${MYSQLD} --basedir=$MYSQL_BASEDIR --user=$USER --help --verbose --wsrep-sst-method=rsync| grep -q wsrep
probe_result=$?
if [[ "$probe_result" == "0" ]]
    then
        skip_test "Incompatible test"
fi
set -e

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-track-changed-pages=TRUE
"
first_inc_suspend_command="FLUSH CHANGED_PAGE_BITMAPS;"

source t/xb_incremental_compressed.inc

test_incremental_compressed 4

check_bitmap_inc_backup
