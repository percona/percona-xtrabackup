########################################################################
# Bug #489290: xtrabackup defaults to mysql datadir
# Bug 1611568: target-dir no longer relative to current directory
########################################################################

. inc/common.sh

start_server

cwd=`pwd`
cd $topdir

# Backup
mkdir $topdir/backup1
xtrabackup --datadir=$mysql_datadir --backup --target-dir=backup1

mkdir $topdir/backup2
xtrabackup --datadir=$mysql_datadir --backup --target-dir=./backup2

xtrabackup --datadir=$mysql_datadir --backup --target-dir=backup3

xtrabackup --datadir=$mysql_datadir --backup --target-dir=./backup4

xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup5

cd backup5
xtrabackup --datadir=$mysql_datadir --backup --target-dir=../backup6
cd ..

# Test
for i in {1..6} ; do
  test -f $topdir/backup$i/ibdata1 || die "Backup not found in $topdir/backup$i"
  rm -rf $topdir/backup$i
done

cd $cwd
