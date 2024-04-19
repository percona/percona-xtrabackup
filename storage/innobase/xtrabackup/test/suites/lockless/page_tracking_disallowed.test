###############################################################################
# PXB-3246: Assertion failure: log0recv.cc:2141:!page || fil_page_type_is_index(page_type)
###############################################################################

. inc/common.sh

run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --target-dir=$topdir/backup_base --lock-ddl=REDUCED  --page-tracking 2> >(tee $topdir/backup_error.log)

if ! egrep -q "\--page-tracking and --lock-ddl=REDUCED cannot be enabled together" $topdir/backup_error.log ; then
    die "xtrabackup didn't handle page-tracking and lock-ddl=REDUCED"
fi
