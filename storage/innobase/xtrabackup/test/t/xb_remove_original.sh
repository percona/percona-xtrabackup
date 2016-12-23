#
# Test for --remove-original option
#

start_server --innodb_file_per_table

$MYSQL $MYSQL_ARGS -e "CREATE TABLE t1 (a INT) engine=InnoDB" test
$MYSQL $MYSQL_ARGS -e "INSERT INTO t1 VALUES (1), (2), (3), (4)" test

xtrabackup --backup --encrypt=AES256 \
	   --encrypt-key=percona_xtrabackup_is_awesome___ \
	   --encrypt-threads=4 --encrypt-chunk-size=8K \
	   --compress --target-dir=$topdir/backup

cp -r $topdir/backup $topdir/backup1

run_cmd $XB_BIN --decrypt=AES256 --encrypt-key=percona_xtrabackup_is_awesome___ \
		--parallel=4 --encrypt-chunk-size=8K \
		--target-dir=$topdir/backup1

test -f $topdir/backup1/test/t1.ibd.qp.xbcrypt || die "Files removed"

rm -rf $topdir/backup1
cp -r $topdir/backup $topdir/backup1

run_cmd $XB_BIN --decrypt=AES256 --encrypt-key=percona_xtrabackup_is_awesome___ \
		--parallel=4 --encrypt-chunk-size=8K \
		--remove-original \
		--target-dir=$topdir/backup1

test -f $topdir/backup1/test/t1.ibd.qp.xbcrypt && die "Files not removed"

rm -rf $topdir/backup1
cp -r $topdir/backup $topdir/backup1

run_cmd_expect_failure $XB_BIN --decrypt=AES256 \
			       --encrypt-key=percona_xtrabackup_Is_awesome___ \
			       --parallel=4 --encrypt-chunk-size=8K \
			       --remove-original \
			       --target-dir=$topdir/backup1

test -f $topdir/backup1/test/t1.ibd.qp.xbcrypt || die "Files removed"
