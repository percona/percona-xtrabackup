############################################################################
# Test uncompressed_backup_size reporting in xtrabackup_info and the error
# log when --compress is used.
#
# Validates byte-perfect invariants for every --compress transport /
# format combination (target-dir, xbstream, encrypt, incremental chain,
# RocksDB bypass, server-encrypted InnoDB, redo log encryption, sparse).
#
# Sampling note: uncompressed_backup_size is sampled inside
# get_xtrabackup_info() and embedded in the file content.  That sample
# is taken AGAIN when --extra-lsndir's xtrabackup_info is written by
# xtrabackup_write_info(), which happens AFTER backup_finish() has
# already written the target's xtrabackup_info through ds_data.  So the
# value we read from the extra-lsndir copy reflects every raw byte that
# flowed through ds_data before compression, including xtrabackup_info
# itself.  Therefore:
#
#   Invariant A  (target-dir):  backup_size == sum_file_bytes(target)
#   Invariant A' (stream     ):  backup_size == size of .xbs file
#   Invariant B  (--compress ):  uncompressed_backup_size ==
#                                sum_file_bytes(decompressed)
#
# All three are exact: zero tolerance.
#
# Sparse + --compress (scen15): Invariant A cannot be byte-perfectly
# compared on disk because backup_size on the compressed target counts
# packed compressed payload while the apparent file layout may round to
# filesystem-block boundaries.  Invariant B is still byte-perfect
# because the decompressor writes the data dense (no holes) and
# uncompressed_backup_size counts only packed payload bytes.
############################################################################

. inc/common.sh

require_zstd
require_lz4

############################################################################
# Helpers (get_field, file_size, sum_file_bytes, find_info_file,
# assert_positive, assert_no_field, assert_eq, assert_target_strict,
# assert_stream_strict, assert_decompressed_strict) are sourced from
# inc/common.sh above.
############################################################################

start_server --innodb_file_per_table

load_sakila

ENCKEY="percona_xtrabackup_is_awesome___"

############################################################################
# Scenario 1: Compressed (lz4) --target-dir
############################################################################
vlog "=== Scenario 1: Compressed (lz4) --target-dir ==="

mkdir -p $topdir/lsn1
xtrabackup --backup --compress=lz4 --target-dir=$topdir/backup1 \
    --extra-lsndir=$topdir/lsn1 \
    2> >(tee $topdir/log1 >&2)

bs1=$(get_field "$topdir/lsn1/xtrabackup_info" backup_size)
us1=$(get_field "$topdir/lsn1/xtrabackup_info" uncompressed_backup_size)
assert_positive "$bs1" "scen1 backup_size"
assert_positive "$us1" "scen1 uncompressed_backup_size"
grep -q "Backup size:"              $topdir/log1 || die "scen1: missing 'Backup size:' in log"
grep -q "Uncompressed backup size:" $topdir/log1 || die "scen1: missing 'Uncompressed backup size:' in log"
grep -q "Compression ratio:"        $topdir/log1 || die "scen1: missing 'Compression ratio:' in log"

assert_target_strict       "$topdir/backup1" "$bs1" "scen1 (compress target)"
assert_decompressed_strict "$topdir/backup1" "$us1" "scen1 (compress target)" ""

rm -rf $topdir/backup1 $topdir/lsn1 $topdir/log1

############################################################################
# Scenario 2: Compressed + xbstream
############################################################################
vlog "=== Scenario 2: --compress=lz4 --stream=xbstream ==="

mkdir -p $topdir/lsn2
xtrabackup --backup --stream=xbstream --compress=lz4 \
    --extra-lsndir=$topdir/lsn2 \
    > $topdir/backup2.xbs 2> >(tee $topdir/log2 >&2)

bs2=$(get_field "$topdir/lsn2/xtrabackup_info" backup_size)
us2=$(get_field "$topdir/lsn2/xtrabackup_info" uncompressed_backup_size)
assert_positive "$bs2" "scen2 backup_size"
assert_positive "$us2" "scen2 uncompressed_backup_size"

assert_stream_strict "$topdir/backup2.xbs" "$bs2" "scen2 (compress stream)"

mkdir -p $topdir/extract2
xbstream -x -C $topdir/extract2 < $topdir/backup2.xbs
assert_decompressed_strict "$topdir/extract2" "$us2" "scen2 (compress stream)" ""

rm -rf $topdir/backup2.xbs $topdir/extract2 $topdir/lsn2 $topdir/log2

############################################################################
# Scenario 3: Compressed + Encrypted + xbstream (the kitchen sink)
############################################################################
vlog "=== Scenario 3: --compress + --encrypt + --stream=xbstream ==="

mkdir -p $topdir/lsn3
xtrabackup --backup --compress=lz4 --encrypt=AES256 --encrypt-key="$ENCKEY" \
    --stream=xbstream --extra-lsndir=$topdir/lsn3 \
    > $topdir/backup3.xbs 2> >(tee $topdir/log3 >&2)

bs3=$(get_field "$topdir/lsn3/xtrabackup_info" backup_size)
us3=$(get_field "$topdir/lsn3/xtrabackup_info" uncompressed_backup_size)
assert_positive "$bs3" "scen3 backup_size"
assert_positive "$us3" "scen3 uncompressed_backup_size"

assert_stream_strict "$topdir/backup3.xbs" "$bs3" "scen3 (compress+encrypt stream)"

mkdir -p $topdir/extract3
xbstream -x -C $topdir/extract3 < $topdir/backup3.xbs
assert_decompressed_strict "$topdir/extract3" "$us3" "scen3 (compress+encrypt stream)" "$ENCKEY"

rm -rf $topdir/backup3.xbs $topdir/extract3 $topdir/lsn3 $topdir/log3

############################################################################
# Scenario 4: Incremental chain with --compress=lz4
############################################################################
vlog "=== Scenario 4: Incremental chain (compress=lz4) ==="

mysql -e "CREATE TABLE t_inc (a INT PRIMARY KEY AUTO_INCREMENT, b TEXT) ENGINE=InnoDB;" test
for i in $(seq 1 500) ; do
  echo "INSERT INTO t_inc (b) VALUES (REPEAT(UUID(), 20));"
done | mysql test

mkdir -p $topdir/lsn4full
xtrabackup --backup --compress=lz4 --target-dir=$topdir/backup4full \
    --extra-lsndir=$topdir/lsn4full

bs4full=$(get_field "$topdir/lsn4full/xtrabackup_info" backup_size)
us4full=$(get_field "$topdir/lsn4full/xtrabackup_info" uncompressed_backup_size)
assert_positive "$us4full" "scen4 full uncompressed_backup_size"

assert_target_strict       "$topdir/backup4full" "$bs4full" "scen4 (full compress target)"
assert_decompressed_strict "$topdir/backup4full" "$us4full" "scen4 (full compress target)" ""

for i in $(seq 1 100) ; do
  echo "INSERT INTO t_inc (b) VALUES (REPEAT(UUID(), 20));"
done | mysql test

mkdir -p $topdir/lsn4inc1
xtrabackup --backup --compress=lz4 \
    --incremental-basedir=$topdir/lsn4full \
    --target-dir=$topdir/backup4inc1 --extra-lsndir=$topdir/lsn4inc1

bs4inc1=$(get_field "$topdir/lsn4inc1/xtrabackup_info" backup_size)
us4inc1=$(get_field "$topdir/lsn4inc1/xtrabackup_info" uncompressed_backup_size)
assert_positive "$us4inc1" "scen4 inc1 uncompressed_backup_size"

assert_target_strict       "$topdir/backup4inc1" "$bs4inc1" "scen4 (inc1 compress target)"
assert_decompressed_strict "$topdir/backup4inc1" "$us4inc1" "scen4 (inc1 compress target)" ""

[ "$bs4inc1" -lt "$bs4full" ] || die "scen4: inc1 ($bs4inc1) should be < full ($bs4full)"

rm -rf $topdir/backup4full $topdir/backup4inc1 $topdir/lsn4full $topdir/lsn4inc1

############################################################################
# Scenario 5: --compress + RocksDB (bypass via ds_uncompressed_data)
############################################################################
if test -f $(dirname ${MYSQLD})/../lib/plugin/ha_rocksdb.so ; then
  vlog "=== Scenario 5: --compress + RocksDB ==="

  stop_server
  rm -rf $mysql_datadir
  start_server --innodb_file_per_table

  init_rocksdb

  mysql -e "CREATE TABLE t_rocks (a INT PRIMARY KEY AUTO_INCREMENT, b INT, c VARCHAR(200)) ENGINE=ROCKSDB;" test
  for i in $(seq 1 500) ; do
    echo "INSERT INTO t_rocks (b, c) VALUES (FLOOR(RAND() * 1000000), UUID());"
  done | mysql test

  mkdir -p $topdir/lsn5
  xtrabackup --backup --compress=lz4 --target-dir=$topdir/backup5 \
      --extra-lsndir=$topdir/lsn5

  bs5=$(get_field "$topdir/lsn5/xtrabackup_info" backup_size)
  us5=$(get_field "$topdir/lsn5/xtrabackup_info" uncompressed_backup_size)
  assert_positive "$us5" "scen5 uncompressed_backup_size"

  assert_target_strict       "$topdir/backup5" "$bs5" "scen5 (rocksdb+compress target)"
  assert_decompressed_strict "$topdir/backup5" "$us5" "scen5 (rocksdb+compress target)" ""

  rm -rf $topdir/backup5 $topdir/lsn5
else
  vlog "=== Scenario 5: SKIPPED (RocksDB not available) ==="
fi

############################################################################
# Scenario 6: --compress + server-encrypted InnoDB tablespaces
############################################################################
vlog "=== Scenario 6: --compress + server-encrypted InnoDB ==="

stop_server

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

mysql -e "CREATE TABLE t_enc (a INT PRIMARY KEY AUTO_INCREMENT, b TEXT) ENCRYPTION='y' ENGINE=InnoDB;" test
for i in $(seq 1 500) ; do
  echo "INSERT INTO t_enc (b) VALUES (REPEAT(UUID(), 10));"
done | mysql test

mkdir -p $topdir/lsn6
xtrabackup --backup --compress=lz4 --target-dir=$topdir/backup6 \
    --extra-lsndir=$topdir/lsn6 ${keyring_args}

bs6=$(get_field "$topdir/lsn6/xtrabackup_info" backup_size)
us6=$(get_field "$topdir/lsn6/xtrabackup_info" uncompressed_backup_size)
assert_positive "$us6" "scen6 uncompressed_backup_size"

assert_target_strict       "$topdir/backup6" "$bs6" "scen6 (enc-innodb+compress target)"
assert_decompressed_strict "$topdir/backup6" "$us6" "scen6 (enc-innodb+compress target)" ""

rm -rf $topdir/backup6 $topdir/lsn6
cleanup_keyring

############################################################################
# Scenario 7: --compress + server redo log encryption
############################################################################
vlog "=== Scenario 7: --compress + redo log encryption ==="

stop_server

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_encrypt=ON
innodb_undo_log_encrypt=ON
"
KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

mysql -e "CREATE TABLE t_redo_enc (a INT PRIMARY KEY AUTO_INCREMENT, b TEXT) ENGINE=InnoDB;" test
for i in $(seq 1 500) ; do
  echo "INSERT INTO t_redo_enc (b) VALUES (REPEAT(UUID(), 10));"
done | mysql test

mkdir -p $topdir/lsn7
xtrabackup --backup --compress=lz4 --target-dir=$topdir/backup7 \
    --extra-lsndir=$topdir/lsn7 ${keyring_args}

bs7=$(get_field "$topdir/lsn7/xtrabackup_info" backup_size)
us7=$(get_field "$topdir/lsn7/xtrabackup_info" uncompressed_backup_size)
assert_positive "$us7" "scen7 uncompressed_backup_size"

assert_target_strict       "$topdir/backup7" "$bs7" "scen7 (redo-enc+compress target)"
assert_decompressed_strict "$topdir/backup7" "$us7" "scen7 (redo-enc+compress target)" ""

rm -rf $topdir/backup7 $topdir/lsn7
cleanup_keyring

############################################################################
# Scenario 8: Sparse files + --compress=zstd.  Invariant B (decompress)
# gives a byte-perfect check because the decompressor writes the data
# dense (no holes) and uncompressed_backup_size counts only the packed
# payload bytes.
############################################################################
stop_server
rm -rf $mysql_datadir
MYSQLD_EXTRA_MY_CNF_OPTS=""
start_server

if grep -q 'PUNCH HOLE support not available' $MYSQLD_ERRFILE ; then
  vlog "=== Scenario 8: SKIPPED (no PUNCH HOLE support) ==="
else
  vlog "=== Scenario 8: Sparse files + --compress=zstd ==="

  mysql -e "CREATE TABLE t_sparse (c1 INT AUTO_INCREMENT PRIMARY KEY, c2 BLOB) COMPRESSION='zlib' ENGINE=InnoDB;" test
  mysql -e "INSERT INTO t_sparse (c2) VALUES (REPEAT('x', 5000));" test
  for i in $(seq 1 10) ; do
    mysql -e "INSERT INTO t_sparse (c2) SELECT c2 FROM t_sparse;" test
  done
  innodb_wait_for_flush_all

  if ! is_sparse_file $mysql_datadir/test/t_sparse.ibd ; then
    die "t_sparse.ibd is expected to be sparse but is NOT"
  fi

  mkdir -p $topdir/lsn8
  xtrabackup --backup --compress=zstd --target-dir=$topdir/backup8 \
      --extra-lsndir=$topdir/lsn8

  bs8=$(get_field "$topdir/lsn8/xtrabackup_info" backup_size)
  us8=$(get_field "$topdir/lsn8/xtrabackup_info" uncompressed_backup_size)
  assert_positive "$us8" "scen8 uncompressed_backup_size"

  assert_decompressed_strict "$topdir/backup8" "$us8" "scen8 (sparse+compress target)" ""

  rm -rf $topdir/backup8 $topdir/lsn8
fi

vlog "All --compress backup_size scenarios passed (byte-perfect)."
