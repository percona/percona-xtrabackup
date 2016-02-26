#
# Bug 1509812: backup fails with history on server enabled
#              and binary log disabled
#

start_server --skip-log-bin

xtrabackup --history --backup --target-dir=$topdir/backup

cat $topdir/backup/xtrabackup_info

run_cmd $MYSQL $MYSQL_ARGS -e "SELECT * FROM PERCONA_SCHEMA.xtrabackup_history\G"

innobackupex --history $topdir/backup

cat $topdir/backup/xtrabackup_info

run_cmd $MYSQL $MYSQL_ARGS -e "SELECT * FROM PERCONA_SCHEMA.xtrabackup_history\G"
