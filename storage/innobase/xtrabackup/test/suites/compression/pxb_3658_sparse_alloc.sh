#
# PXB-3658: Verify that page-compressed tables remain sparse after
# backup/restore, and that allocated disk size (du) exactly matches
# the original server files.
#
# Strategy: restart the server after inserting data to force a clean
# shutdown (all dirty pages flushed, checkpoint complete). This gives
# us a deterministic .ibd state. After backup + prepare (which is a
# no-op for data pages since redo log is empty) + copy-back, the
# restored .ibd must have the exact same allocated size.
#
# Tests five restore paths:
#   1. Local backup (no streaming)
#   2. xbstream extract (no xtrabackup compression) -- SPARSE chunks
#   3. xbstream extract --decompress (with lz4 compression) -- dense + restore_sparseness via xbstream
#   4. Local backup with --compress=zstd + xtrabackup --decompress -- restore_sparseness via xtrabackup
#   5. Streamed backup with --compress=zstd + xbstream extract (no decompress) + xtrabackup --decompress
#

. inc/common.sh
. inc/keyring_file.sh

require_lz4
require_zstd

get_allocated_size() {
  du --block-size=1 "$1" | awk '{print $1}'
}

get_apparent_size() {
  stat --printf "%s" "$1"
}

check_sparse_exact() {
  local filepath=$1
  local label=$2
  local expected_alloc=$3
  local tolerance=${4:-0}

  if ! is_sparse_file "$filepath" ; then
    die "$label: $filepath is NOT sparse"
  fi

  local apparent=$(get_apparent_size "$filepath")
  local allocated=$(get_allocated_size "$filepath")

  vlog "$label: apparent=$apparent allocated=$allocated expected=$expected_alloc tolerance=$tolerance"

  local diff=$(( allocated - expected_alloc ))
  # absolute value
  if [ "$diff" -lt 0 ] ; then
    diff=$(( -diff ))
  fi

  if [ "$diff" -gt "$tolerance" ] ; then
    die "$label: allocated size mismatch: got $allocated, expected $expected_alloc (diff=$diff, tolerance=$tolerance)"
  fi

  vlog "$label: PASS (diff=$diff within tolerance=$tolerance)"
}

setup_and_get_baseline() {
  run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t_zlib (c1 INT AUTO_INCREMENT PRIMARY KEY, c2 BLOB) COMPRESSION='zlib';
CREATE TABLE t_plain (c1 INT AUTO_INCREMENT PRIMARY KEY, c2 BLOB);
EOF

  for tbl in t_zlib t_plain ; do
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
INSERT INTO $tbl (c2) VALUES (REPEAT('x', 5000));
INSERT INTO $tbl (c2) SELECT c2 FROM $tbl;
INSERT INTO $tbl (c2) SELECT c2 FROM $tbl;
INSERT INTO $tbl (c2) SELECT c2 FROM $tbl;
INSERT INTO $tbl (c2) SELECT c2 FROM $tbl;
EOF
  done

  # Graceful shutdown flushes all dirty pages with punch holes,
  # then restart gives us a clean, deterministic .ibd state.
  shutdown_server
  start_server

  if ! is_sparse_file "$mysql_datadir/test/t_zlib.ibd" ; then
    die "original t_zlib.ibd is NOT sparse after restart"
  fi

  orig_zlib_alloc=$(get_allocated_size "$mysql_datadir/test/t_zlib.ibd")
  orig_zlib_apparent=$(get_apparent_size "$mysql_datadir/test/t_zlib.ibd")

  vlog "Baseline t_zlib.ibd: apparent=$orig_zlib_apparent allocated=$orig_zlib_alloc"
}

#
# Start server and check punch hole support
#
start_server

if grep -q 'PUNCH HOLE support not available' $MYSQLD_ERRFILE ; then
  skip_test 'punch hole support is not available'
fi

########################################################################
# Path 1: Local backup (no streaming)
########################################################################

vlog "===== Path 1: Local backup ====="

setup_and_get_baseline

record_db_state test

xtrabackup --backup --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

check_sparse_exact "$topdir/backup/test/t_zlib.ibd" \
  "local-backup t_zlib (after prepare)" "$orig_zlib_alloc"

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

check_sparse_exact "$mysql_datadir/test/t_zlib.ibd" \
  "local-backup t_zlib (after copy-back)" "$orig_zlib_alloc"

if is_sparse_file "$mysql_datadir/test/t_plain.ibd" ; then
  die "t_plain.ibd should NOT be sparse"
fi

verify_db_state test

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
DROP TABLE IF EXISTS t_zlib;
DROP TABLE IF EXISTS t_plain;
EOF

stop_server
rm -rf $mysql_datadir
rm -rf $topdir/backup

########################################################################
# Path 2: xbstream extract (no xtrabackup compression)
########################################################################

vlog "===== Path 2: xbstream extract (no xtrabackup compression) ====="

start_server
setup_and_get_baseline

record_db_state test

xtrabackup --backup --stream=xbstream --target-dir=$topdir/tmp \
  > $topdir/backup.xbs

rm -rf $topdir/backup && mkdir $topdir/backup
xbstream -x -v -C $topdir/backup < $topdir/backup.xbs

xtrabackup --prepare --target-dir=$topdir/backup

check_sparse_exact "$topdir/backup/test/t_zlib.ibd" \
  "xbstream t_zlib (after prepare)" "$orig_zlib_alloc"

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

check_sparse_exact "$mysql_datadir/test/t_zlib.ibd" \
  "xbstream t_zlib (after copy-back)" "$orig_zlib_alloc"

if is_sparse_file "$mysql_datadir/test/t_plain.ibd" ; then
  die "t_plain.ibd should NOT be sparse (xbstream path)"
fi

verify_db_state test

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
DROP TABLE IF EXISTS t_zlib;
DROP TABLE IF EXISTS t_plain;
EOF

stop_server
rm -rf $mysql_datadir
rm -rf $topdir/backup $topdir/backup.xbs

########################################################################
# Path 3: xbstream extract --decompress (with lz4 compression)
########################################################################

vlog "===== Path 3: xbstream --decompress (lz4) ====="

start_server
setup_and_get_baseline

record_db_state test

xtrabackup --backup --compress=lz4 --stream=xbstream --target-dir=$topdir/tmp \
  > $topdir/backup.xbs

rm -rf $topdir/backup && mkdir $topdir/backup
xbstream -x -v -C $topdir/backup --decompress < $topdir/backup.xbs

xtrabackup --prepare --target-dir=$topdir/backup

check_sparse_exact "$topdir/backup/test/t_zlib.ibd" \
  "xbstream-decompress t_zlib (after prepare)" "$orig_zlib_alloc"

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

check_sparse_exact "$mysql_datadir/test/t_zlib.ibd" \
  "xbstream-decompress t_zlib (after copy-back)" "$orig_zlib_alloc"

if is_sparse_file "$mysql_datadir/test/t_plain.ibd" ; then
  die "t_plain.ibd should NOT be sparse (decompress path)"
fi

verify_db_state test

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
DROP TABLE IF EXISTS t_zlib;
DROP TABLE IF EXISTS t_plain;
EOF

stop_server
rm -rf $mysql_datadir
rm -rf $topdir/backup $topdir/backup.xbs

########################################################################
# Path 4: Local backup with --compress=zstd + xtrabackup --decompress
########################################################################

vlog "===== Path 4: Local backup + compress=zstd + xtrabackup --decompress ====="

start_server
setup_and_get_baseline

record_db_state test

xtrabackup --backup --compress=zstd --target-dir=$topdir/backup

xtrabackup --decompress --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

check_sparse_exact "$topdir/backup/test/t_zlib.ibd" \
  "local-zstd-decompress t_zlib (after prepare)" "$orig_zlib_alloc"

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

check_sparse_exact "$mysql_datadir/test/t_zlib.ibd" \
  "local-zstd-decompress t_zlib (after copy-back)" "$orig_zlib_alloc"

if is_sparse_file "$mysql_datadir/test/t_plain.ibd" ; then
  die "t_plain.ibd should NOT be sparse (local zstd decompress path)"
fi

verify_db_state test

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
DROP TABLE IF EXISTS t_zlib;
DROP TABLE IF EXISTS t_plain;
EOF

stop_server
rm -rf $mysql_datadir
rm -rf $topdir/backup

########################################################################
# Path 5: Stream + compress=zstd + xbstream extract + xtrabackup --decompress
########################################################################

vlog "===== Path 5: xbstream extract (no decompress) + xtrabackup --decompress (zstd) ====="

start_server
setup_and_get_baseline

record_db_state test

xtrabackup --backup --compress=zstd --stream=xbstream --target-dir=$topdir/tmp \
  > $topdir/backup.xbs

rm -rf $topdir/backup && mkdir $topdir/backup
xbstream -x -v -C $topdir/backup < $topdir/backup.xbs

xtrabackup --decompress --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

check_sparse_exact "$topdir/backup/test/t_zlib.ibd" \
  "stream-zstd-xb-decompress t_zlib (after prepare)" "$orig_zlib_alloc"

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

check_sparse_exact "$mysql_datadir/test/t_zlib.ibd" \
  "stream-zstd-xb-decompress t_zlib (after copy-back)" "$orig_zlib_alloc"

if is_sparse_file "$mysql_datadir/test/t_plain.ibd" ; then
  die "t_plain.ibd should NOT be sparse (stream zstd xb-decompress path)"
fi

verify_db_state test

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
DROP TABLE IF EXISTS t_zlib;
DROP TABLE IF EXISTS t_plain;
EOF
