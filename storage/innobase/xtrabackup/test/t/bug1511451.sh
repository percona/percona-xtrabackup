#
# Bug 1511451: innobackupex permission error; --defaults-extra-file ignored?
#

run_cmd_expect_failure ${IB_BIN} \
	--datadir=$topdir/data \
	--defaults-file=$MYSQLD_VARDIR/my.cnf \
	$topdir/backup

run_cmd_expect_failure ${XB_BIN} \
	--datadir=$topdir/data \
	--defaults-file=$MYSQLD_VARDIR/my.cnf \
	--backup \
	--target-dir=$topdir/backup

run_cmd_expect_failure ${IB_BIN} \
	--datadir=$topdir/data \
	--defaults-extra-file=$MYSQLD_VARDIR/my.cnf \
	$topdir/backup

run_cmd_expect_failure ${XB_BIN} \
	--datadir=$topdir/data \
	--defaults-extra-file=$MYSQLD_VARDIR/my.cnf \
	--backup \
	--target-dir=$topdir/backup

if [ `grep -c 'must be specified first' $OUTFILE` != '4' ] ; then
	die 'Error message mush be repeated 4 times'
fi
