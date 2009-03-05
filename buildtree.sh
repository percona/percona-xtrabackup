# helper script to prepare MySQL tree for xtrabackup build
# tested with mysql-5.0.75 / .77
# $1 - path to mysql .tar.gz

PATHS=/mnt/data/vadim/mysql-5.0.77.tar.gz
DIRDST=`dirname $PATHS`
DRNAME=`basename $PATHS .tar.gz`

mkdir -p $DIRDST/xtrabackup-build

tar xfz $PATHS -C $DIRDST/xtrabackup-build
cp fix_innodb_for_backup.patch $DIRDST/xtrabackup-build/$DRNAME
mkdir -p  $DIRDST/xtrabackup-build/$DRNAME/innobase/xtrabackup
cp * $DIRDST/xtrabackup-build/$DRNAME/innobase/xtrabackup 
pushd $DIRDST/xtrabackup-build/$DRNAME
patch -p1 < fix_innodb_for_backup.patch
./configure
make -j8
cd innobase/xtrabackup
make
