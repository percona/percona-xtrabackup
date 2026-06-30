############################################################################
# PXB-3807 : regression guard for the future-LSN check on INCREMENTAL prepare.
#
# Incremental --prepare merges .delta pages whose LSN is ahead of the current
# system LSN; the system LSN only catches up afterwards, when the incremental's
# redo is applied.  The system/undo future-LSN check (which fails a page whose
# LSN > log_get_lsn(*log_sys)) runs in the --check-tables block, AFTER redo
# apply, so the system LSN has already reached the merged pages' LSN -- it must
# NOT false-positive on a valid incremental backup.
#
# This test merges an incremental and runs --check-tables both right after the
# delta merge and on the final prepare; both must pass.
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

mysql test <<EOF
CREATE TABLE t1 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
EOF
for i in $(seq 1 200); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t1 (b) VALUES (REPEAT('x', 200));"
done

vlog "Full backup"
xtrabackup --backup --target-dir=$topdir/full

vlog "Make changes after the full backup (generates higher-LSN delta pages)"
for i in $(seq 1 400); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t1 (b) VALUES (REPEAT('y', 200));"
done
run_cmd $MYSQL $MYSQL_ARGS test -e "UPDATE t1 SET b = REPEAT('z', 200);"
# Force a checkpoint so the changed pages are flushed with their new LSNs.
shutdown_server
start_server --innodb_file_per_table
run_cmd $MYSQL $MYSQL_ARGS test -e \
  "BEGIN; UPDATE t1 SET b = REPEAT('w', 200); ROLLBACK;"

vlog "Incremental backup"
xtrabackup --backup --incremental-basedir=$topdir/full \
  --target-dir=$topdir/inc1

vlog "Prepare full (apply-log-only)"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full

vlog "Merge incremental + --check-tables (delta pages just written, redo applied)"
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
  --check-tables --target-dir=$topdir/full 2>&1 | tee $topdir/merge.log
grep -q "verifying checksums of tablespace" $topdir/merge.log || \
  die "checksum pass did not run during incremental merge"
grep -q "ahead of the recovered system LSN" $topdir/merge.log && \
  die "FALSE POSITIVE: future-LSN check fired on a valid incremental merge"
# Engine oracle: the B-tree phase reads pages through the buffer pool with
# InnoDB's own future-LSN check active (recv_lsn_checks_on is on post-recovery).
# It must report NO "in the future" page once recovery has caught the system
# LSN up to the merged delta pages -- the condition under which our check fires.
grep -q "is in the future" $topdir/merge.log && \
  die "a page was still in the future when check-tables ran on the incremental merge"
grep -q "All table checks passed" $topdir/merge.log || \
  die "incremental merge --check-tables failed unexpectedly"
vlog "Incremental merge check passed (no false future-LSN)"

vlog "Final prepare + --check-tables"
xtrabackup --prepare --check-tables --target-dir=$topdir/full 2>&1 \
  | tee $topdir/final.log
grep -q "ahead of the recovered system LSN" $topdir/final.log && \
  die "FALSE POSITIVE: future-LSN check fired on the final prepare"
grep -q "is in the future" $topdir/final.log && \
  die "a page was still in the future when check-tables ran on the final prepare"
grep -q "All table checks passed" $topdir/final.log || \
  die "final --check-tables failed unexpectedly"
vlog "Final prepare check passed"

vlog "PXB-3807 incremental future-LSN regression guard passed"
