############################################################################
# PXB-3804 : --check-tables must not hang during B-tree descent.
#
# Theory (Gap 3): btr_validate_level()'s descent loop
#   while (level != btr_page_get_level(page)) { ... follow first node ptr ... }
# has no iteration bound and no requirement that the level strictly DECREASES.
# btr_descent_level_is_sane() only checks the level is in range (<=
# BTR_MAX_NODE_LEVEL), not that descent makes progress. So a node pointer whose
# child-page-number is corrupted to point to an in-bounds same/higher page
# makes the descent loop forever (check-tables passes trx=nullptr, so
# trx_is_interrupted() never breaks out).
#
# Repro (2-level tree): point the ROOT's leftmost node pointer's child-page
# number back at the ROOT itself. Validating level 0 then descends
# root -> (child = root) -> root -> ... forever.
# Expectation BEFORE fix: timeout. AFTER fix: bounded descent reports the
# cycle and exits non-zero.
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

mysql test <<'EOF'
SET SESSION cte_max_recursion_depth = 20000;
CREATE TABLE t1 (id INT PRIMARY KEY, pad VARCHAR(100));
INSERT INTO t1
WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 5000)
SELECT n, CONCAT('p', n) FROM seq;
EOF

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
IBD=$topdir/backup/test/t1.ibd

PAGE_SIZE=$(get_page_size "$IBD")

# ROOT page and the in-page byte offset of the leftmost node pointer's
# child-page-number field.
read ROOT OFF <<< "$(find_leftmost_node_ptr "$IBD")"
[ "$ROOT" != "ERR" ] && [ -n "$OFF" ] || \
  die "need a multi-level clustered index with node pointers ($ROOT $OFF)"
vlog "root=$ROOT, leftmost node-ptr child-page field at in-page offset $OFF"

#
# Control: clean backup passes.
#
vlog "=== Control: clean --check-tables ==="
cp -r $topdir/backup $topdir/backup_ctrl
xtrabackup --prepare --check-tables --target-dir=$topdir/backup_ctrl 2>&1 \
  | tee $topdir/ctrl.log
grep -q "All table checks passed" $topdir/ctrl.log || \
  die "Control: clean backup unexpectedly failed --check-tables"
vlog "Control passed"

#
# Negative: point the root's first node pointer back at the root.
#
vlog "=== Negative: root node-ptr child -> root (descent self-cycle) ==="
cp -r $topdir/backup $topdir/backup_bad
CIBD=$topdir/backup_bad/test/t1.ibd
ORIG=$(mach_read_n "$CIBD" "$ROOT" "$OFF" 4)
mach_write_n "$CIBD" "$ROOT" "$OFF" "$ROOT" 4
vlog "child page $ORIG -> $ROOT"

SIZE_BEFORE=$(stat -c %s "$CIBD")
set +e
timeout 90 $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup_bad 2>&1 | tee $topdir/bad.log
RC=${PIPESTATUS[0]}
set -e
SIZE_AFTER=$(stat -c %s "$CIBD")
vlog "check-tables exit code: $RC"

[ "$RC" -ne 124 ] || die "descent_nodeptr_cycle: --check-tables HUNG during descent"
grep -qiE "Assertion failure|got signal|ut_error|ib::fatal triggered" $topdir/bad.log && \
  die "descent_nodeptr_cycle: --check-tables ABORTED"
[ "$SIZE_AFTER" = "$SIZE_BEFORE" ] || \
  die "descent_nodeptr_cycle: .ibd grew during --check-tables ($SIZE_BEFORE -> $SIZE_AFTER)"
[ "$RC" -ne 0 ] || die "descent_nodeptr_cycle: --check-tables passed a descent cycle"
grep -qiE "descent|cycle|too deep|B-tree corruption|Table check failed" $topdir/bad.log || \
  die "descent_nodeptr_cycle: corruption not reported"

vlog "descent_nodeptr_cycle passed: descent cycle reported gracefully (rc=$RC)"
