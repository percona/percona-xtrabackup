#
# PXB-3758: --apply-redo-only replaces the confusingly named --apply-log-only.
#
# Both spellings drive the same knob, so this test pins down all three cases:
# the new name works (and stops at redo, so an incremental still applies on
# top), the old name still works but warns, and passing both is rejected.
#

start_server

mysql -e "CREATE TABLE t1 (a INT) ENGINE=InnoDB" test
mysql -e "INSERT INTO t1 (a) VALUES (1), (2), (3)" test

xtrabackup --backup --target-dir=$topdir/full

mysql -e "INSERT INTO t1 (a) VALUES (10), (20), (30)" test

xtrabackup --backup --backup-incremental-base=$topdir/full \
	   --target-dir=$topdir/inc

record_db_state test

# --apply-redo-only is the recommended spelling: it must not nag the operator.
xtrabackup --prepare --apply-redo-only --target-dir=$topdir/full

if grep -q "apply-log-only is deprecated" $OUTFILE ; then
	die "--apply-redo-only must not emit the --apply-log-only deprecation warning"
fi

# It has to leave the backup at "redo applied, undo skipped" - that is the
# whole point of the option, and what makes the incremental below apply.
grep -q "backup_type = log-applied" $topdir/full/xtrabackup_checkpoints \
	|| die "--apply-redo-only did not stop after redo apply"

# The old spelling keeps working, on the same backup, and warns once.
xtrabackup --prepare --apply-log-only --prepare-incremental-from-dir=$topdir/inc \
	   --target-dir=$topdir/full

grep -q "apply-log-only is deprecated and will be removed in a future release" \
	$OUTFILE || die "--apply-log-only did not emit a deprecation warning"

xtrabackup --prepare --target-dir=$topdir/full

# Both spellings at once is operator confusion, never intent.
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --apply-redo-only \
		       --apply-log-only --target-dir=$topdir/full

grep -q "are the same option; pass only one" $OUTFILE \
	|| die "passing both spellings was not rejected with the expected error"

# Order must not matter, and neither should an explicit value.
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --apply-log-only \
		       --apply-redo-only --target-dir=$topdir/full

# The chain prepared through both spellings restores to the recorded state.
stop_server
rm -rf $mysql_datadir/*

xtrabackup --copy-back --target-dir=$topdir/full

start_server

verify_db_state test

rm -rf $topdir/full $topdir/inc
