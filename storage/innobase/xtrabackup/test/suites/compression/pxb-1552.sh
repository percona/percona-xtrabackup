#
# PXB-1552: Preparing an incremental backup will crash if compressed InnoDB undo
#           tablespaces are not removed beforehand
#
start_server
mysql -e "CREATE UNDO TABLESPACE undo_001 ADD DATAFILE 'undo_001.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo_002 ADD DATAFILE 'undo_002.ibu'"

load_sakila

xtrabackup --backup --compress --target-dir=$topdir/backup

mysql -e "DELETE FROM payment LIMIT 100" sakila

xtrabackup --backup --compress --target-dir=$topdir/inc --incremental-basedir=$topdir/backup

xtrabackup --decompress --target-dir=$topdir/backup
xtrabackup --decompress --target-dir=$topdir/inc

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup --incremental-dir=$topdir/inc
xtrabackup --prepare --target-dir=$topdir/backup
