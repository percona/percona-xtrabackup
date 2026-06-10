############################################################################
# PXB-3807 : --check-tables verifies the per-page checksums of the system
# tablespace (ibdata*) and all undo tablespaces, which the B-tree validation
# skips entirely.
#
# This debug test uses a DBUG injection (check_system_inject_corruption) for a
# deterministic positive: with the injection the checksum pass flags pages and
# --prepare --check-tables must fail.  It also includes a clean-run control to
# prove the pass does not false-positive on a healthy backup (the buffer-pool
# flush before the scan makes the on-disk image authoritative).
############################################################################

. inc/common.sh

require_debug_pxb_version

start_server

vlog "Create a table and generate some undo (rollback) so undo pages are used"
mysql test <<EOF
CREATE TABLE t1 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
EOF
for i in $(seq 1 100); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t1 (b) VALUES (REPEAT('x', 200));"
done
run_cmd $MYSQL $MYSQL_ARGS test -e \
  "BEGIN; UPDATE t1 SET b = REPEAT('y', 200); ROLLBACK;"

vlog "Take backup"
xtrabackup --backup --target-dir=$topdir/backup

#
# Control: a healthy backup must pass, and the new checksum pass must run.
#
vlog "=== Control: clean system/undo checksum pass ==="
cp -r $topdir/backup $topdir/backup_ok
xtrabackup --prepare --check-tables --target-dir=$topdir/backup_ok 2>&1 \
  | tee $topdir/ok.log
grep -q "verifying checksums of tablespace" $topdir/ok.log || \
  die "Control: system/undo checksum pass did not run"
grep -q "All table checks passed" $topdir/ok.log || \
  die "Control: clean backup unexpectedly failed --check-tables"
vlog "Control passed"

#
# Injected corruption: the checksum pass must report and fail the prepare.
#
vlog "=== Injected corruption: check_system_inject_corruption ==="
cp -r $topdir/backup $topdir/backup_inj
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --debug=d,check_system_inject_corruption \
  --target-dir=$topdir/backup_inj 2>&1 | tee $topdir/inj.log

grep -q "checksum mismatch" $topdir/inj.log || \
  die "Injected: checksum mismatch not reported"
grep -q "Table check failed" $topdir/inj.log || \
  die "Injected: Table check failed message not found"
vlog "Injected corruption sub-test passed"

vlog "All PXB-3807 debug sub-tests passed"
