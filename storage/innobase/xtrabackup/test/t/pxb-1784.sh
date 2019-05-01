#
# PXB-1784: ALTER UNDO TABLESPACE fails after restore when undo tablespace
#           state is changed during backup
#

start_server

mysql -e "CREATE UNDO TABLESPACE undo1 ADD DATAFILE 'undo1.ibu'"

while true ; do
    mysql -e "ALTER UNDO TABLESPACE undo1 SET INACTIVE"
    mysql -e "ALTER UNDO TABLESPACE undo1 SET ACTIVE"
done &

xtrabackup --no-backup-locks --lock-ddl --backup --target-dir=$topdir/backup

mysql -e "CREATE UNDO TABLESPACE undo2 ADD DATAFILE 'undo2.ibu'"

while true ; do
    mysql -e "ALTER UNDO TABLESPACE undo2 SET INACTIVE"
    mysql -e "ALTER UNDO TABLESPACE undo2 SET ACTIVE"
done &

xtrabackup --no-backup-locks --lock-ddl --backup --incremental-basedir=$topdir/backup --target-dir=$topdir/inc

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup --incremental-dir=$topdir/inc

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

ls -al $topdir/backup

start_server

mysql -e "ALTER UNDO TABLESPACE undo1 SET INACTIVE"
mysql -e "ALTER UNDO TABLESPACE undo1 SET ACTIVE"

mysql -e "ALTER UNDO TABLESPACE undo2 SET INACTIVE"
mysql -e "ALTER UNDO TABLESPACE undo2 SET ACTIVE"
