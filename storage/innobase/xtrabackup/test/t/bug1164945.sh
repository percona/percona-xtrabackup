##############################################################################
# Bug #1164945: move-back/copy-back should allow non-empty directories in some
#               cases
##############################################################################

start_server

innobackupex --no-timestamp $topdir/backup

stop_server

innobackupex --apply-log $topdir/backup

cp -a $topdir/backup $topdir/backup_copy

function test_force_non_empty_dirs()
{
    rm -rf $MYSQLD_DATADIR/*

    mkdir $MYSQLD_DATADIR/empty_subdir

    touch $MYSQLD_DATADIR/non_existing_file

    vlog "check that $1 fails without --force-non-empty-directories"
    run_cmd_expect_failure $IB_BIN $IB_ARGS $1 $topdir/backup

    rm -rf $topdir/backup
    cp -a $topdir/backup_copy $topdir/backup

    vlog "check that $1 works with --force-non-empty-directories"
    innobackupex $1 --force-non-empty-directories $topdir/backup

    rm -rf $topdir/backup
    cp -a $topdir/backup_copy $topdir/backup

    vlog "check that $1 --force-non-empty-directories does not overwrite files"
    touch $MYSQLD_DATADIR/ibdata1
    run_cmd_expect_failure $IB_BIN $IB_ARGS $1 --force-non-empty-directories \
                           $topdir/backup

    rm -rf $topdir/backup
    cp -a $topdir/backup_copy $topdir/backup
}

test_force_non_empty_dirs --copy-back

test_force_non_empty_dirs --move-back
