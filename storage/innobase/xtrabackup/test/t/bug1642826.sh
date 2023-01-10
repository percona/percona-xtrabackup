#
# Bug 1642826: incremental backup fails with relative path / xtrabackup doesn't expand tilde
#

start_server

cd $topdir

mkdir backup

xtrabackup --backup --target-dir=backup/full --extra-lsndir=backup/lsn
xtrabackup --backup --incremental-basedir=backup/full --target-dir=backup/inc_1
xtrabackup --prepare --apply-log-only --target-dir=backup/full
xtrabackup --prepare --target-dir=backup/full --incremental-dir=backup/inc_1

rm -rf backup/*

cd -

export HOME=$topdir

run_cmd $XB_BIN $XB_ARGS --backup --target-dir=~/backup/full --extra-lsndir=~/backup/lsn
run_cmd $XB_BIN $XB_ARGS --backup --incremental-basedir=~/backup/full --target-dir=~/backup/inc_1
xtrabackup --prepare --apply-log-only --target-dir=~/backup/full
xtrabackup --prepare --target-dir=~/backup/full --incremental-dir=~/backup/inc_1

