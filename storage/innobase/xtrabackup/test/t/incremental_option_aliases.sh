#
# PXB-3840: the incremental options were renamed so the name says which
# phase they belong to.
#
#   --incremental-basedir -> --backup-incremental-base   (--backup)
#   --incremental-dir     -> --prepare-incremental-from-dir  (--prepare)
#
# Both spellings drive the same variable, so this pins down all three
# cases: the new names work, the old names still work but warn, and
# passing an old name together with its new one is rejected.
#

start_server

mysql -e "CREATE TABLE t1 (a INT) ENGINE=InnoDB" test
mysql -e "INSERT INTO t1 (a) VALUES (1), (2), (3)" test

xtrabackup --backup --target-dir=$topdir/full

mysql -e "INSERT INTO t1 (a) VALUES (10), (20), (30)" test

# The new names are the recommended spelling, so they must not nag.
xtrabackup --backup --backup-incremental-base=$topdir/full \
	   --target-dir=$topdir/inc

record_db_state test

xtrabackup --prepare --apply-redo-only --target-dir=$topdir/full
xtrabackup --prepare --apply-redo-only \
	   --prepare-incremental-from-dir=$topdir/inc --target-dir=$topdir/full
xtrabackup --prepare --target-dir=$topdir/full

for opt in incremental-basedir incremental-dir ; do
	if grep -q -- "--$opt is deprecated" $OUTFILE ; then
		die "--$opt warning emitted when only the new names were used"
	fi
done

# The chain built and prepared through the new names has to restore.
stop_server
rm -rf $mysql_datadir/*

xtrabackup --copy-back --target-dir=$topdir/full

start_server

verify_db_state test

# The old names keep working, and each warns once.
mysql -e "INSERT INTO t1 (a) VALUES (100), (200)" test

xtrabackup --backup --target-dir=$topdir/full2
xtrabackup --backup --incremental-basedir=$topdir/full2 \
	   --target-dir=$topdir/inc2

grep -q -- "--incremental-basedir is deprecated and will be removed in a future release" \
	$OUTFILE || die "--incremental-basedir did not emit a deprecation warning"
grep -q -- "Please use --backup-incremental-base instead" $OUTFILE \
	|| die "--incremental-basedir warning does not name the new option"

xtrabackup --prepare --apply-redo-only --target-dir=$topdir/full2
xtrabackup --prepare --apply-redo-only --incremental-dir=$topdir/inc2 \
	   --target-dir=$topdir/full2

grep -q -- "--incremental-dir is deprecated and will be removed in a future release" \
	$OUTFILE || die "--incremental-dir did not emit a deprecation warning"
grep -q -- "Please use --prepare-incremental-from-dir instead" $OUTFILE \
	|| die "--incremental-dir warning does not name the new option"

# An old name together with its new one is operator confusion, never intent.
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup \
		       --incremental-basedir=$topdir/full2 \
		       --backup-incremental-base=$topdir/full2 \
		       --target-dir=$topdir/inc3

grep -q -- "--incremental-basedir and --backup-incremental-base are the same option; pass only one" \
	$OUTFILE || die "passing both backup spellings was not rejected"

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare \
		       --incremental-dir=$topdir/inc2 \
		       --prepare-incremental-from-dir=$topdir/inc2 \
		       --target-dir=$topdir/full2

grep -q -- "--incremental-dir and --prepare-incremental-from-dir are the same option; pass only one" \
	$OUTFILE || die "passing both prepare spellings was not rejected"

rm -rf $topdir/full $topdir/inc $topdir/full2 $topdir/inc2
