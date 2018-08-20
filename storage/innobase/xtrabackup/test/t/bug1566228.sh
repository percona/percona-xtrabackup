#
# Bug 1566228: XB crashes with insufficient param
# 

start_server

xtrabackup --backup --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

run_cmd_expect_failure ${XB_BIN} --defaults-file= --defaults-group=mysqld.2 \
	  --no-version-check --move-back --force-non-empty-directories \
	  --target-dir=$topdir/backup 2>&1 | \
	  grep "Error: datadir must be specified"
