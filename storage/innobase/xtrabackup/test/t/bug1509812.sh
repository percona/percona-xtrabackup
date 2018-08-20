#
# Bug 1509812: backup fails with history on server enabled
#              and binary log disabled
#

start_server --skip-log-bin

mkdir $topdir/backup

xtrabackup --history --backup --target-dir=$topdir/backup/1

run_cmd $MYSQL $MYSQL_ARGS -e "SELECT * FROM PERCONA_SCHEMA.xtrabackup_history\G"

xtrabackup --backup --history --target-dir=$topdir/backup/2

run_cmd $MYSQL $MYSQL_ARGS -e "SELECT * FROM PERCONA_SCHEMA.xtrabackup_history\G"
