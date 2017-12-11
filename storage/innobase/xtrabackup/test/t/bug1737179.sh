start_server

vlog "Full backup"

mysql -e "CREATE TABLE test.tmp (a INT) ENGINE=InnoDB"
mysql -e "INSERT INTO test.tmp VALUES(1)"


xtrabackup --backup \
    --target-dir=$topdir/data/full

vlog "Incremental backup"

mysql -e "INSERT INTO test.tmp VALUES(2)"
mysql -e "INSERT INTO test.tmp VALUES(3)"
mysql -e "INSERT INTO test.tmp VALUES(4)"
mysql -e "INSERT INTO test.tmp VALUES(5)"

xtrabackup --backup \
    --target-dir=$topdir/data/delta \
    --incremental-basedir=$topdir/data/full

vlog "Prepare full"

xtrabackup --prepare --apply-log-only \
    --throttle=40 \
    --target-dir=$topdir/data/full > $topdir/pxb.log 2>&1

vlog "Prepare Increment"
xtrabackup --prepare --apply-log-only  \
    --throttle=40 \
    --target-dir=$topdir/data/full --incremental-dir=$topdir/data/delta >> $topdir/pxb2.log 2>&1

[ `grep 'Last MySQL binlog' $topdir/pxb.log | wc -l` =  "1" ] || die '...'
[ `grep 'Last MySQL binlog' $topdir/pxb2.log | wc -l` =  "1" ] || die '...'