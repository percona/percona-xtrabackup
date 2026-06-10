############################################################################
# PXB-3807 : --check-tables verifies the per-page checksums of the system
# tablespace (ibdata*) and all undo tablespaces.  These spaces are skipped by
# the B-tree validation, so without this pass their pages get no integrity
# check at all.
#
# Real on-disk corruption (no debug build required): corrupt a COLD page -- one
# InnoDB does not read during startup -- near the end of an undo tablespace and
# of ibdata1, leaving the stored checksum stale.  --prepare --check-tables must
# detect the mismatch, report it, and exit non-zero, WITHOUT extending the file
# or hanging.  A clean-run control proves there are no false positives (the
# buffer-pool flush before the scan makes the on-disk image authoritative).
#
# Cold pages are chosen near the high end of each file (free/unused pages that
# recovery does not touch) so the corruption is caught by the read-only
# checksum scan rather than by a crashing buffer-pool read at startup.
############################################################################

. inc/common.sh

PAGE_SZ=16384

# corrupt_last_page <file> -- flip a body byte of the file's last page, leaving
# the stored checksum stale.  A free (all-zero) last page becomes a non-empty
# page with a zero checksum field, which is a checksum mismatch.
corrupt_last_page() {
  local F="$1"
  local SZ LASTPAGE
  SZ=$(stat -c %s "$F")
  LASTPAGE=$(( SZ / PAGE_SZ - 1 ))
  vlog "corrupting $F : last page $LASTPAGE at byte offset 200"
  mach_write_8 "$F" "$LASTPAGE" 200 0xDEADBEEFDEADBEEF
}

start_server

vlog "Create a table and generate undo (rollback) so undo pages are allocated"
mysql test <<EOF
CREATE TABLE t1 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
EOF
for i in $(seq 1 200); do
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
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_ok
xtrabackup --prepare --check-tables --target-dir=$topdir/backup_ok 2>&1 \
  | tee $topdir/ok.log
grep -q "verifying checksums of tablespace" $topdir/ok.log || \
  die "Control: system/undo checksum pass did not run"
grep -q "All table checks passed" $topdir/ok.log || \
  die "Control: clean backup unexpectedly failed --check-tables"
vlog "Control passed"

#
# Negative (undo): corrupt a cold page of an undo tablespace.
#
vlog "=== Negative: corrupt undo tablespace ==="
cp -r $topdir/backup $topdir/backup_undo
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_undo
UNDO=$(ls $topdir/backup_undo/undo_* 2>/dev/null | head -1)
[ -n "$UNDO" ] || die "could not find an undo tablespace in the backup"
corrupt_last_page "$UNDO"

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --target-dir=$topdir/backup_undo 2>&1 | tee $topdir/undo.log
grep -q "checksum mismatch" $topdir/undo.log || \
  die "Negative(undo): checksum mismatch not reported"
grep -q "Table check failed" $topdir/undo.log || \
  die "Negative(undo): Table check failed message not found"
grep -qi "posix_fallocate" $topdir/undo.log && \
  die "Negative(undo): validation attempted to extend a file"
vlog "Negative(undo) passed"

#
# Negative (ibdata): corrupt a cold page of the system tablespace.
#
vlog "=== Negative: corrupt system tablespace (ibdata1) ==="
cp -r $topdir/backup $topdir/backup_sys
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup_sys
corrupt_last_page "$topdir/backup_sys/ibdata1"

run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --target-dir=$topdir/backup_sys 2>&1 | tee $topdir/sys.log
grep -q "checksum mismatch" $topdir/sys.log || \
  die "Negative(ibdata): checksum mismatch not reported"
grep -q "Table check failed" $topdir/sys.log || \
  die "Negative(ibdata): Table check failed message not found"
vlog "Negative(ibdata) passed"

#
# Negative (ibdata startup-range / "hot" page): page 5 of the system tablespace
# is TRX_SYS, read by InnoDB during startup recovery -- BEFORE the checksum
# scan.  Corrupting it is therefore caught by the normal buffer-pool read
# during recovery (which aborts after retries with "Unable to read page"),
# not by our graceful scan.  Either way --prepare must fail (non-zero); this
# documents that startup-range corruption is surfaced, just via a different
# path than the cold-page scan above.
#
vlog "=== Negative: corrupt ibdata1 startup-range page (TRX_SYS, page 5) ==="
cp -r $topdir/backup $topdir/backup_hot
# Do NOT --apply-log-only first: we want recovery to read the corrupt page.
mach_write_8 "$topdir/backup_hot/ibdata1" 5 200 0xDEADBEEFDEADBEEF
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --target-dir=$topdir/backup_hot 2>&1 | tee $topdir/hot.log
grep -Eqi "Unable to read page|page corruption|checksum mismatch|is corrupted|corrupt" \
  $topdir/hot.log || \
  die "Negative(ibdata hot): startup-range corruption not surfaced"
vlog "Negative(ibdata hot) passed"

vlog "All PXB-3807 sub-tests passed"
