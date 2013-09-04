##########################################################################
# Bug #1190716: innobackupex --safe-slave-backup hangs when used on master
##########################################################################

. inc/common.sh

start_server

innobackupex --no-timestamp --safe-slave-backup $topdir/backup

grep -q "Not checking slave open temp tables for --safe-slave-backup because host is not a slave" $OUTFILE
