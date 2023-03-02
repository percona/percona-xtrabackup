. inc/common.sh

require_debug_pxb_version

start_server

load_dbase_schema sakila
load_dbase_data sakila

# Take backup
mkdir -p $topdir/backup

xtrabackup --backup --target-dir=${topdir}/backup 2>&1 |tee $topdir/pxb.log

record_db_state test
vlog "Run prepare with simualted error. The error is simulated after redo is applied but before rollback is finished"

run_cmd_expect_failure $XB_BIN --prepare --target-dir=$topdir/backup --debug=d,force_sdi_error 2>&1 |tee $topdir/pxb_prepare_error.log


vlog "Error message InnoDB: Assertion failure shouldn't be present in error log"
! grep -q "InnoDB: Assertion failure: log0log.cc:.*:srv_is_being_started || srv_shutdown_state.load() == SRV_SHUTDOWN_EXIT_THREADS" $topdir/pxb_prepare_error.log || die "Unexpected crash during prepare"

vlog "Resume prepare on the failed directory"
xtrabackup --prepare --target-dir=$topdir/backup

stop_server
rm -rf $mysql_datadir/*

xtrabackup --move-back --target-dir=${topdir}/backup
start_server

verify_db_state test
