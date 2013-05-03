############################################################################
# Bug #1175860: Orphaned xtrabackup_pid file Breaks Cluster SST
############################################################################

. inc/common.sh

start_server

echo "22993" > $MYSQLD_TMPDIR/xtrabackup_pid

mkdir -p $topdir/backup
innobackupex --stream=tar $topdir/backup > /dev/null

grep -q "A left over instance of xtrabackup pid file" $OUTFILE
