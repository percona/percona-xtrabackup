##########################################################################
# Bug #1190716: innobackupex --safe-slave-backup hangs when used on master
##########################################################################

. inc/common.sh

start_server

xtrabackup --backup --safe-slave-backup --target-dir=$topdir/backup

grep -q "Not checking slave open temp tables for --safe-slave-backup because host is not a slave" $OUTFILE
