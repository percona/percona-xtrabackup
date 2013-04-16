# Test incremental backups that use bitmaps with 1KB compressed pages

if [ -z "$XTRADB_VERSION" ]; then
    echo "Requires XtraDB" > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

# The test is disabled for Percona XtraDB Cluster until a version
# with bitmap user requests is released (containing PS 5.5.29-30.0 
# or later)
set +e
${MYSQLD} --basedir=$MYSQL_BASEDIR --user=$USER --help --verbose --wsrep-sst-method=rsync| grep -q wsrep
probe_result=$?
if [[ "$probe_result" == "0" ]]
    then
        vlog "Incompatible test" > $SKIPPED_REASON
        exit $SKIPPED_EXIT_CODE
fi
set -e

mysqld_extra_args=--innodb-track-changed-pages=TRUE
first_inc_suspend_command="FLUSH CHANGED_PAGE_BITMAPS;"

source t/xb_incremental_compressed.inc

test_incremental_compressed 1

check_bitmap_inc_backup
