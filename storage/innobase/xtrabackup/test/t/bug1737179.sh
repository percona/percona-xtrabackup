start_server

vlog "Full backup"

mysql -e "CREATE TEMPORARY TABLE test.tmp (a INT) ENGINE=InnoDB"
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
xtrabackup --prepare  \
    --throttle=40 \
    --target-dir=$topdir/data/full --incremental-dir=$topdir/data/delta >> $topdir/pxb.log 2>&1

run_cmd grep 'InnoDB: xtrabackup: Last MySQL binlog file position' $topdir/pxb.log