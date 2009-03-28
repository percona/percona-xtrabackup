# helper script to prepare MySQL tree for xtrabackup build
# tested with mysql-5.0.75 / .77
# $1 - path to mysql .tar.gz

PATHS=$1
DIRDST=`dirname $PATHS`
DRNAME=`basename $PATHS .tar.gz`

mkdir -p $DIRDST/xtrabackup-build

tar xfz $PATHS -C $DIRDST/xtrabackup-build
cp fix_innodb_for_backup51.patch $DIRDST/xtrabackup-build/$DRNAME/storage

mkdir -p  $DIRDST/xtrabackup-build/$DRNAME/storage/innobase/xtrabackup
cp * $DIRDST/xtrabackup-build/$DRNAME/storage/innobase/xtrabackup 
pushd $DIRDST/xtrabackup-build/$DRNAME
cd storage
patch -p1 < fix_innodb_for_backup51.patch
cd ..
./configure
make -j8
cd storage/innobase/xtrabackup
make
