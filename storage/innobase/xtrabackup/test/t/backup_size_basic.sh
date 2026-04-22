############################################################################
# Test backup_size reporting in xtrabackup_info and the error log.
#
# Validates byte-perfect invariants across non-compress transport /
# format combinations (target-dir, xbstream, encrypt, incremental chain,
# full restore, sparse InnoDB page-compressed tablespaces).
#
# Sampling note: backup_size is sampled inside get_xtrabackup_info() and
# embedded in the file content.  That sample is taken AGAIN when
# --extra-lsndir's xtrabackup_info is written by xtrabackup_write_info(),
# which happens AFTER backup_finish() has already written the target's
# xtrabackup_info through ds_data.  So the value read from the extra-
# lsndir copy reflects the FINAL leaf bytes_written -- it already counts
# every byte that ends up on disk, including the target's own
# xtrabackup_info[.xbcrypt|...].  Therefore:
#
#   Invariant A  (target-dir):  backup_size == sum_file_bytes(target)
#   Invariant A' (stream     ):  backup_size == size of .xbs file
#
# Both are byte-exact; zero tolerance.
#
# Sparse scenarios are LOOSE for backup_size because:
#   - backup_size counts packed data bytes only (no holes).
#   - Apparent on-disk size INCLUDES the punched holes.
#   - Allocated bytes (%b * 512) round to FS-block boundaries and the
#     +1-byte trailing-hole fix in local_close() allocates one whole FS
#     block (~4 KiB) which the counter records as 1 byte.
#   Neither apparent nor allocated equals backup_size to the byte for
#   sparse files.
############################################################################

. inc/common.sh

############################################################################
# Helpers (get_field, file_size, sum_file_bytes, find_info_file,
# assert_positive, assert_no_field, assert_eq, assert_target_strict,
# assert_stream_strict) are sourced from inc/common.sh above.
############################################################################

start_server --innodb_file_per_table

load_sakila

ENCKEY="percona_xtrabackup_is_awesome___"

############################################################################
# Scenario 1: Plain --target-dir
############################################################################
vlog "=== Scenario 1: Plain --target-dir ==="

mkdir -p $topdir/lsn1
xtrabackup --backup --target-dir=$topdir/backup1 --extra-lsndir=$topdir/lsn1 \
    2> >(tee $topdir/log1 >&2)

bs1=$(get_field "$topdir/lsn1/xtrabackup_info" backup_size)
assert_positive "$bs1" "scen1 backup_size"
assert_no_field "$topdir/lsn1/xtrabackup_info" uncompressed_backup_size
grep -q "Backup size:" $topdir/log1 || die "scen1: missing 'Backup size:' in log"

assert_target_strict "$topdir/backup1" "$bs1" "scen1 (plain target)"

rm -rf $topdir/backup1 $topdir/lsn1 $topdir/log1

############################################################################
# Scenario 2: Plain --stream=xbstream
############################################################################
vlog "=== Scenario 2: Plain --stream=xbstream ==="

mkdir -p $topdir/lsn2
xtrabackup --backup --stream=xbstream --extra-lsndir=$topdir/lsn2 \
    > $topdir/backup2.xbs 2> >(tee $topdir/log2 >&2)

bs2=$(get_field "$topdir/lsn2/xtrabackup_info" backup_size)
assert_positive "$bs2" "scen2 backup_size"
assert_no_field "$topdir/lsn2/xtrabackup_info" uncompressed_backup_size

assert_stream_strict "$topdir/backup2.xbs" "$bs2" "scen2 (plain stream)"

rm -rf $topdir/backup2.xbs $topdir/lsn2 $topdir/log2

############################################################################
# Scenario 3: Encrypted (no compress) --target-dir
############################################################################
vlog "=== Scenario 3: Encrypted --target-dir ==="

mkdir -p $topdir/lsn3
xtrabackup --backup --encrypt=AES256 --encrypt-key="$ENCKEY" \
    --target-dir=$topdir/backup3 --extra-lsndir=$topdir/lsn3 \
    2> >(tee $topdir/log3 >&2)

bs3=$(get_field "$topdir/lsn3/xtrabackup_info" backup_size)
assert_positive "$bs3" "scen3 backup_size"
assert_no_field "$topdir/lsn3/xtrabackup_info" uncompressed_backup_size

assert_target_strict "$topdir/backup3" "$bs3" "scen3 (encrypt target)"

rm -rf $topdir/backup3 $topdir/lsn3 $topdir/log3

############################################################################
# Scenario 4: Encrypted + xbstream (no compress)
############################################################################
vlog "=== Scenario 4: --encrypt + --stream=xbstream ==="

mkdir -p $topdir/lsn4
xtrabackup --backup --stream=xbstream --encrypt=AES256 --encrypt-key="$ENCKEY" \
    --extra-lsndir=$topdir/lsn4 \
    > $topdir/backup4.xbs 2> >(tee $topdir/log4 >&2)

bs4=$(get_field "$topdir/lsn4/xtrabackup_info" backup_size)
assert_positive "$bs4" "scen4 backup_size"
assert_no_field "$topdir/lsn4/xtrabackup_info" uncompressed_backup_size

assert_stream_strict "$topdir/backup4.xbs" "$bs4" "scen4 (encrypt stream)"

rm -rf $topdir/backup4.xbs $topdir/lsn4 $topdir/log4

############################################################################
# Scenario 5: Restore validation (plain backup, full prepare + copy-back)
############################################################################
vlog "=== Scenario 5: Restore validation ==="

xtrabackup --backup --target-dir=$topdir/backup5
record_db_state sakila
xtrabackup --prepare --target-dir=$topdir/backup5
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup5
start_server
verify_db_state sakila
rm -rf $topdir/backup5

############################################################################
# Scenario 6: Incremental chain (no compress)
############################################################################
vlog "=== Scenario 6: Incremental chain (no compress) ==="

mysql -e "CREATE TABLE t_inc (a INT PRIMARY KEY AUTO_INCREMENT, b TEXT) ENGINE=InnoDB;" test
for i in $(seq 1 500) ; do
  echo "INSERT INTO t_inc (b) VALUES (REPEAT(UUID(), 20));"
done | mysql test

mkdir -p $topdir/lsn6full
xtrabackup --backup --target-dir=$topdir/backup6full \
    --extra-lsndir=$topdir/lsn6full

bs6full=$(get_field "$topdir/lsn6full/xtrabackup_info" backup_size)
assert_target_strict "$topdir/backup6full" "$bs6full" "scen6 (inc full)"

for i in $(seq 1 100) ; do
  echo "INSERT INTO t_inc (b) VALUES (REPEAT(UUID(), 20));"
done | mysql test

mkdir -p $topdir/lsn6inc1
xtrabackup --backup --incremental-basedir=$topdir/backup6full \
    --target-dir=$topdir/backup6inc1 --extra-lsndir=$topdir/lsn6inc1

bs6inc1=$(get_field "$topdir/lsn6inc1/xtrabackup_info" backup_size)
assert_target_strict "$topdir/backup6inc1" "$bs6inc1" "scen6 (inc1)"
[ "$bs6inc1" -lt "$bs6full" ] || die "scen6: inc1 ($bs6inc1) should be < full ($bs6full)"

ls $topdir/backup6inc1/test/*.delta > /dev/null 2>&1 || die "scen6: missing .delta in inc1"
ls $topdir/backup6inc1/test/*.meta  > /dev/null 2>&1 || die "scen6: missing .meta  in inc1"

for i in $(seq 1 50) ; do
  echo "INSERT INTO t_inc (b) VALUES (REPEAT(UUID(), 20));"
done | mysql test

mkdir -p $topdir/lsn6inc2
xtrabackup --backup --incremental-basedir=$topdir/backup6inc1 \
    --target-dir=$topdir/backup6inc2 --extra-lsndir=$topdir/lsn6inc2

bs6inc2=$(get_field "$topdir/lsn6inc2/xtrabackup_info" backup_size)
assert_target_strict "$topdir/backup6inc2" "$bs6inc2" "scen6 (inc2)"

# Restore the chain end-to-end.
record_db_state test
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup6full
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/backup6inc1 \
    --target-dir=$topdir/backup6full
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/backup6inc2 \
    --target-dir=$topdir/backup6full
xtrabackup --prepare --target-dir=$topdir/backup6full
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup6full
start_server
verify_db_state test

rm -rf $topdir/backup6full $topdir/backup6inc1 $topdir/backup6inc2 \
       $topdir/lsn6full   $topdir/lsn6inc1   $topdir/lsn6inc2

############################################################################
# Scenario 7: Sparse files, no compress.  See header comment for why
# backup_size cannot be byte-perfectly compared on disk for sparse files.
############################################################################
if grep -q 'PUNCH HOLE support not available' $MYSQLD_ERRFILE ; then
  vlog "=== Scenario 7: SKIPPED (no PUNCH HOLE support) ==="
else
  vlog "=== Scenario 7: Sparse files, no compress ==="

  mysql -e "CREATE TABLE t_sparse (c1 INT AUTO_INCREMENT PRIMARY KEY, c2 BLOB) COMPRESSION='zlib' ENGINE=InnoDB;" test
  mysql -e "INSERT INTO t_sparse (c2) VALUES (REPEAT('x', 5000));" test
  for i in $(seq 1 10) ; do
    mysql -e "INSERT INTO t_sparse (c2) SELECT c2 FROM t_sparse;" test
  done
  innodb_wait_for_flush_all

  if ! is_sparse_file $mysql_datadir/test/t_sparse.ibd ; then
    die "t_sparse.ibd is expected to be sparse but is NOT"
  fi
  record_db_state test

  mkdir -p $topdir/lsn7
  xtrabackup --backup --target-dir=$topdir/backup7 --extra-lsndir=$topdir/lsn7

  bs7=$(get_field "$topdir/lsn7/xtrabackup_info" backup_size)
  assert_positive "$bs7" "scen7 backup_size"
  assert_no_field "$topdir/lsn7/xtrabackup_info" uncompressed_backup_size

  is_sparse_file $topdir/backup7/test/t_sparse.ibd \
      || die "scen7: backed-up t_sparse.ibd is not sparse"

  # Sanity: backup_size must be smaller than apparent on-disk total because
  # apparent includes the holes that backup_size omits.
  apparent7=$(sum_file_bytes "$topdir/backup7")
  [ "$bs7" -lt "$apparent7" ] \
      || die "scen7: backup_size ($bs7) should be < apparent total ($apparent7) due to holes"

  xtrabackup --prepare --target-dir=$topdir/backup7
  stop_server
  rm -rf $mysql_datadir/*
  xtrabackup --copy-back --target-dir=$topdir/backup7
  start_server
  verify_db_state test

  rm -rf $topdir/backup7 $topdir/lsn7
fi

vlog "All non-compress backup_size scenarios passed (byte-perfect where strict)."
