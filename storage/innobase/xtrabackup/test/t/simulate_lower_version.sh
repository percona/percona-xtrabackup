########################################################################
# PXB should error out with upper version of MySQL or PS
########################################################################

. inc/common.sh

require_debug_pxb_version

start_server

vlog "case#1 backup should fail with lower version"
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --debug=d,simulate_lower_version --target-dir=$topdir/backup

vlog "case#2 backup should succesful with no-server-version-check lower version"
xtrabackup --backup --debug=d,simulate_lower_version --no-server-version-check --target-dir=$topdir/backup
rm -r $topdir/backup
