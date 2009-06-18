# helper script to prepare MySQL tree for xtrabackup build
# tested with mysql-5.0.75 / .77
# $1 - path to mysql .tar.gz

BV=0.8

PATHS=$1
DIRDST=`dirname $PATHS`
DRNAME=`basename $PATHS .tar.gz`

mkdir -p $DIRDST/xtrabackup-build

rm -fr $DIRDST/xtrabackup-build/$DRNAME 

tar xfz $PATHS -C $DIRDST/xtrabackup-build
cp ../fix_innodb_for_backup.patch $DIRDST/xtrabackup-build/$DRNAME
mkdir -p  $DIRDST/xtrabackup-build/$DRNAME/innobase/xtrabackup
cp ../* $DIRDST/xtrabackup-build/$DRNAME/innobase/xtrabackup 
pushd $DIRDST/xtrabackup-build/$DRNAME
patch -p1 < fix_innodb_for_backup.patch

cd ..
mv $DIRDST/xtrabackup-build/$DRNAME/ $DIRDST/xtrabackup-build/xtrabackup-$BV

tar cfz xtrabackup-$BV.tar.gz xtrabackup-$BV
cp xtrabackup-$BV.tar.gz /usr/src/redhat/SOURCES/

#./configure
#make -j8
#cd innobase/xtrabackup
#make
