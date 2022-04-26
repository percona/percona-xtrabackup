
start_server

xtrabackup --backup --target-dir=$topdir/backup

stop_server
vlog "case#1 check copy-back should not work on new backup "
rm -rf $MYSQLD_DATADIR/*
run_cmd_expect_failure $XB_BIN $XB_ARGS --copy-back --target-dir=$topdir/backup
run_cmd_expect_failure $XB_BIN $XB_ARGS --move-back --target-dir=$topdir/backup
