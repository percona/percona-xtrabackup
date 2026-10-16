###############################################################################
# File-operation and marker-handler errors must fail backup/prepare. Exercise
# actual filesystem failures and check the phase, exit status, and .ren lifetime.
###############################################################################

. inc/common.sh
require_debug_pxb_version
start_server

expect_prepare_failure() {
  local dir=$1 pattern=$2
  shift 2
  run_cmd_expect_failure "$XB_BIN" $XB_ARGS --prepare --target-dir="$dir" "$@" \
    >"$dir.prepare.log" 2>&1
  if ! grep -q "$pattern" "$dir.prepare.log"; then
    cat "$dir.prepare.log"
    die "missing expected failure: $pattern"
  fi
  if grep -q 'completed OK!' "$dir.prepare.log"; then
    die "prepare reported success after $pattern"
  fi
}

$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.t1(id INT PRIMARY KEY) ENGINE=InnoDB;
  INSERT INTO test.t1 VALUES(1),(2),(3);"
innodb_wait_for_flush_all
xtrabackup --backup --target-dir="$topdir/raw" --lock-ddl=REDUCED \
  --debug-sync=ddl_tracker_before_lock_ddl >"$topdir/raw.log" 2>&1 &
job_pid=$!
wait_for_xb_to_suspend "$topdir/raw/xtrabackup_debug_sync"
$MYSQL $MYSQL_ARGS -e "RENAME TABLE test.t1 TO test.t2"
kill -CONT "$(cat "$topdir/raw/xtrabackup_debug_sync")"
run_cmd wait $job_pid

ren_files=("$topdir/raw/test/"*.ren)
[ "${#ren_files[@]}" = 1 ] && [ -f "${ren_files[0]}" ] || die "expected one genuine .ren"
ren_name=$(basename "${ren_files[0]}")

# Callback failure must propagate from both schema and top-level entries.
for location in schema root; do
  dir="$topdir/bad_version_$location"
  cp -a "$topdir/raw" "$dir"
  marker="$dir/test/$ren_name"
  if [ "$location" = root ]; then
    mv "$marker" "$dir/$ren_name"
    marker="$dir/$ren_name"
  fi
  printf '999\ntest/t2.ibd\n' >"$marker"
  expect_prepare_failure "$dir" 'Unexpected ren file version'
  [ -f "$marker" ] && [ -f "$dir/test/t1.ibd" ] || die "failed callback changed input"
  [ ! -e "$dir/test/t1.ibd.temp" ] || die "rename ran after invalid marker"
done

# Make the first InnoDB rename return its real failure value.
dir="$topdir/staging_failure"
cp -a "$topdir/raw" "$dir"
expect_prepare_failure "$dir" 'prepare_handle_ren_files: Cannot rename' \
  --debug=d,fil_rename_tablespace_failure_2
[ -f "$dir/test/$ren_name" ] && [ -f "$dir/test/t1.ibd" ] || die "staging consumed marker"

# Only the second rename fails: the first must leave a staged .ibd file.
dir="$topdir/finalize_failure"
cp -a "$topdir/raw" "$dir"
expect_prepare_failure "$dir" 'prepare_finalize_ren_files: Cannot rename' \
  --debug=d,ddl_tracker_fail_finalize_rename
[ -f "$dir/test/$ren_name" ] && [ -f "$dir/test/t1.ibd.temp" ] || die "finalize consumed marker"

# A failing .crpt callback happens before tablespace discovery.
dir="$topdir/corrupt_delete_failure"
cp -a "$topdir/raw" "$dir"
mv "$dir/test/t1.ibd" "$dir/test/t1.saved"
mkdir "$dir/test/t1.ibd"
touch "$dir/test/t1.ibd.crpt"
expect_prepare_failure "$dir" 'prepare_handle_corrupt_files: Can not delete'
[ -f "$dir/test/t1.ibd.crpt" ] || die "failed delete consumed .crpt"

# The same backup with no fault must restore all data.
cp -a "$topdir/raw" "$topdir/good"
xtrabackup --prepare --target-dir="$topdir/good"
[ -f "$topdir/good/test/t2.ibd" ] || die "rename did not complete"
[ ! -e "$topdir/good/test/$ren_name" ] || die "completed rename left marker"
record_db_state test
stop_server
rm -rf "$mysql_datadir"
mkdir "$mysql_datadir"
xtrabackup --copy-back --target-dir="$topdir/good"
start_server
verify_db_state test

# Force the actual marker create to fail even when the tests run as root.
# Do not accept merely writing an error to the log and returning rc=0.
xtrabackup --backup --target-dir="$topdir/blocked" --lock-ddl=REDUCED \
  --debug-sync=ddl_tracker_before_lock_ddl >"$topdir/blocked.log" 2>&1 &
job_pid=$!
wait_for_xb_to_suspend "$topdir/blocked/xtrabackup_debug_sync"
$MYSQL $MYSQL_ARGS -e "RENAME TABLE test.t2 TO test.t3"
mkdir "$topdir/blocked/test/$ren_name"
kill -CONT "$(cat "$topdir/blocked/xtrabackup_debug_sync")"
run_cmd_expect_failure wait $job_pid
grep -q 'failed to write DDL marker' "$topdir/blocked.log" || die "marker failure was not reached"
if grep -q 'completed OK!' "$topdir/blocked.log"; then
  die "backup reported success after marker create failed"
fi
stop_server
