# helper script to prepare MySQL tree for xtrabackup build
# tested with mysql-5.0.75 / .77
# $1 - path to mysql .tar.gz
set -e
PATHS=$1
DIRDST=`dirname $PATHS`
DRNAME=`basename $PATHS .tar.gz`

mkdir -p $DIRDST/xtrabackup-build

tar xfz $PATHS -C $DIRDST/xtrabackup-build
cp ../fix_innodb_for_backup.patch $DIRDST/xtrabackup-build/$DRNAME
mkdir -p  $DIRDST/xtrabackup-build/$DRNAME/innobase/xtrabackup
cp -R ../* $DIRDST/xtrabackup-build/$DRNAME/innobase/xtrabackup 
pushd $DIRDST/xtrabackup-build/$DRNAME
patch -p1 < fix_innodb_for_backup.patch

./configure --with-extra-charsets=complex
if test -f /proc/cpuinfo
then
        n_proc=`grep ^processor.: /proc/cpuinfo | wc -l`
else
        n_proc=1
fi
make -j$n_proc
cd innobase/xtrabackup
make
