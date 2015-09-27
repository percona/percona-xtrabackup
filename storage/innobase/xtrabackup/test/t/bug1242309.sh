#############################################################################
# Bug 1242309: xtrabackup_logfile not compressed in XtraBackup 2.1+
#############################################################################

start_server

innobackupex --no-timestamp --compress $topdir/backup

if [ ! -f $topdir/backup/xtrabackup_logfile.qp ] ; then
	die "xtrabackup_logfile is not compressed!"
fi

rm -rf $topdir/backup/*

innobackupex --no-timestamp --compress --stream=xbstream $topdir/backup | xbstream -xv -C $topdir/backup

if [ ! -f $topdir/backup/xtrabackup_logfile.qp ] ; then
	die "xtrabackup_logfile is not compressed!"
fi
