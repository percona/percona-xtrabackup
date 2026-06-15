############################################################################
# PXB-3804 : read-only "no tablespace extend" gate during --check-tables.
#
# The sibling-link bounds check (issue2) guards FIL_PAGE_NEXT/PREV, but a
# B-tree descent also follows NODE-POINTER child page numbers, which are not
# bounds-checked.  A node pointer whose child page number is corrupted to an
# out-of-bounds value (0xDEADBEEF) is followed by btr_node_ptr_get_child() ->
# btr_block_get() -> Fil_shard::do_io(); without the read-only gate that read
# would extend the tablespace (a multi-terabyte posix_fallocate that hangs
# --check-tables / grows the backup file).  The gate (fil_check_tables_no_
# extend) refuses to extend during validation, so the read fails fast instead
# of hanging and the backup file is left untouched.
#
# This test exists specifically to fail if the no-extend gate is removed:
#   - WITH the gate:    no hang, the .ibd is not extended.
#   - WITHOUT the gate:  ~61 TB posix_fallocate -> hang (timeout) / file grows.
#
# (The gate is a read-only safeguard; the out-of-bounds node pointer is still
# fatal corruption, so xtrabackup exits non-zero -- the point here is that it
# does so WITHOUT extending the backup or hanging.)
############################################################################

. inc/common.sh


start_server --innodb_file_per_table

vlog "Create test_users with enough rows for a multi-level B-tree (node ptrs)"
mysql test <<'EOF'
SET SESSION cte_max_recursion_depth = 20000;
CREATE TABLE test_users (id INT PRIMARY KEY, name VARCHAR(100));
INSERT INTO test_users
WITH RECURSIVE seq(n) AS (
  SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 10000
)
SELECT n, CONCAT('user', n) FROM seq;
EOF

vlog "Full backup + apply-log-only prepare"
xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup

IBD=$topdir/backup/test/test_users.ibd

vlog "Locate the leftmost node-pointer's child page number on the root page"
COORD=$(find_leftmost_node_ptr "$IBD")
echo "$COORD" | grep -q "^ERR" && die "could not set up node-ptr corruption: $COORD"
read ROOT OFF <<< "$COORD"
ORIG=$(mach_read_4 "$IBD" "$ROOT" "$OFF")
vlog "corrupting node-ptr child at root page $ROOT offset $OFF (was $ORIG) -> 0xDEADBEEF"
mach_write_4 "$IBD" "$ROOT" "$OFF" 0xDEADBEEF

SIZE_BEFORE=$(stat -c %s "$IBD")

vlog "Prepare with --check-tables under a timeout: the gate must prevent the extend/hang"
CHECK_TIMEOUT=90
set +e
timeout $CHECK_TIMEOUT $XB_BIN $XB_ARGS --prepare --check-tables \
  --innodb-checksum-algorithm=none \
  --target-dir=$topdir/backup 2>&1 | tee $topdir/check.log
RC=${PIPESTATUS[0]}
set -e
vlog "check-tables exit code: $RC"

SIZE_AFTER=$(stat -c %s "$IBD")

if [ "$RC" -eq 124 ]; then
  die "node_ptr_no_extend: --check-tables HUNG following a corrupt node-pointer child (no-extend gate ineffective, PXB-3804)"
fi
if grep -qi "posix_fallocate" $topdir/check.log; then
  die "node_ptr_no_extend: --check-tables attempted a tablespace extension during read-only validation (PXB-3804)"
fi
if [ "$SIZE_AFTER" != "$SIZE_BEFORE" ]; then
  die "node_ptr_no_extend: backup .ibd grew during --check-tables ($SIZE_BEFORE -> $SIZE_AFTER); the read-only gate did not hold"
fi

vlog "node_ptr_no_extend passed: out-of-bounds node pointer did not extend the backup or hang"
