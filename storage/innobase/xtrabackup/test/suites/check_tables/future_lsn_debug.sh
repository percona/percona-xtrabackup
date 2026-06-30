############################################################################
# PXB-3807 : during --check-tables the system/undo checksum pass also flags a
# page whose LSN is AHEAD of the recovered system LSN.  Such a page carries
# changes beyond the redo that was applied (an incomplete/over-applied backup,
# the wrong redo log, or a corrupt LSN field).  Unlike innochecksum -- which
# has no system LSN to compare against -- xtrabackup runs inside the engine
# after recovery and knows log_get_lsn(*log_sys), so it can detect this.
#
# A checksum-valid page with a forged future LSN is impractical to build on
# disk (it would need a recomputed checksum), so this uses the
# check_system_inject_future_lsn DBUG point for a deterministic positive.
############################################################################

. inc/common.sh

require_debug_pxb_version

start_server

mysql test <<EOF
CREATE TABLE t1 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
EOF
for i in $(seq 1 50); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t1 (b) VALUES (REPEAT('x', 200));"
done

vlog "Take backup"
xtrabackup --backup --target-dir=$topdir/backup

#
# Control: clean backup must pass (the future-LSN check must NOT false-positive
# on legitimate pages, whose LSN is <= the recovered system LSN).
#
vlog "=== Control: clean backup passes (no false future-LSN) ==="
cp -r $topdir/backup $topdir/backup_ok
xtrabackup --prepare --check-tables --target-dir=$topdir/backup_ok 2>&1 \
  | tee $topdir/ok.log
grep -q "All table checks passed" $topdir/ok.log || \
  die "Control: clean backup unexpectedly failed --check-tables"
vlog "Control passed"

#
# Injected future LSN: a system/undo page reported as ahead of the system LSN
# must fail the prepare.
#
vlog "=== Injected future LSN: check_system_inject_future_lsn ==="
cp -r $topdir/backup $topdir/backup_fl
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --debug=d,check_system_inject_future_lsn \
  --target-dir=$topdir/backup_fl 2>&1 | tee $topdir/fl.log

grep -q "ahead of the recovered system LSN" $topdir/fl.log || \
  die "Injected: future-LSN page not reported"
grep -q "Table check failed" $topdir/fl.log || \
  die "Injected: Table check failed message not found"
vlog "Injected future-LSN sub-test passed"

vlog "All PXB-3807 future-LSN sub-tests passed"
