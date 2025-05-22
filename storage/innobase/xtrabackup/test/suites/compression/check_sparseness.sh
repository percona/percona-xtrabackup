. inc/common.sh
. inc/keyring_file.sh

require_lz4
require_zstd
require_debug_pxb_version

cleanup() {
run_cmd $MYSQL $MYSQL_ARGS test <<EOF
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
EOF
}

grep_sparse_msg() {
  local filename=$1
  local errfile=$2

  grep -q "File .*$filename is not sparse, skipping restore_sparseness" "$errfile"
}

check_sparse_msgs() {
  local errfile="$1"

  # These should have the "not sparse" message
  for file in sys/sys_config.ibd mysql.ibd "test/t5.ibd"; do
    if ! grep_sparse_msg "$file" "$errfile"; then
      die "$file is not sparse, but the 'skipping restore_sparseness' message is missing"
    fi
  done

  # These should NOT have the message (i.e., they are sparse)
  for file in "test/t1.ibd" "test/t2.ibd" "test/t3.ibd" "test/t4.ibd"; do
    if grep_sparse_msg "$file" "$errfile"; then
      die "$file is sparse, but the 'skipping restore_sparseness' message is present"
    fi
  done
}

check_sparseness() {
  local mysql_datadir="$1"

  # Expected sparse files
  for file in "t1.ibd" "t2.ibd" "t3.ibd" "t4.ibd"; do
    full_path="$mysql_datadir/test/$file"
    if ! is_sparse_file "$full_path"; then
      cleanup
      die "$file is expected to be sparse, but it is NOT"
    fi
  done

  # Expected non-sparse files
  for file in "sys/sys_config.ibd" "mysql.ibd" "test/t5.ibd"; do
    full_path="$mysql_datadir/$file"
    if is_sparse_file "$full_path"; then
      cleanup
      die "$file is NOT expected to be sparse, but it IS"
    fi
  done

  echo "All sparseness checks passed"
}

check_and_start_server_sparse() {
start_server

if grep -q 'PUNCH HOLE support not available' $MYSQLD_ERRFILE ; then
    skip 'punch hole support is not available'
fi

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

CREATE TABLE t1 (c1 BLOB) COMPRESSION='zlib';
CREATE TABLE t2 (c1 BLOB) COMPRESSION='lz4';
CREATE TABLE t3 (c1 BLOB) COMPRESSION='zlib' ENCRYPTION='y';
CREATE TABLE t4 (c1 BLOB) COMPRESSION='lz4' ENCRYPTION='y';
CREATE TABLE t5 (c1 BLOB);
EOF

for i in {1..5} ; do
  run_cmd $MYSQL $MYSQL_ARGS test <<EOF
INSERT INTO t$i VALUES (REPEAT('x', 5000));

INSERT INTO t$i SELECT * FROM t$i;
INSERT INTO t$i SELECT * FROM t$i;
INSERT INTO t$i SELECT * FROM t$i;
INSERT INTO t$i SELECT * FROM t$i;

EOF
done

# wait for InnoDB to flush all dirty pages
innodb_wait_for_flush_all

if ! is_sparse_file $mysql_datadir/test/t1.ibd ; then
     cleanup
     die "t1.ibd is expected to be sparse, but it is NOT"
fi
}

take_backup_local_and_stream() {
  extra_args=$1
  xtrabackup --backup --target-dir=$topdir/backup --transition-key=123 ${extra_args}

  mkdir $topdir/backuplsn

  xtrabackup --backup --target-dir=$topdir/tmp --transition-key=123 \
           --extra-lsndir=$topdir/backuplsn \
           --stream=xbstream ${extra_args} > $topdir/backup.xbs

  record_db_state test
}

decompress_from_dir() {
  xtrabackup --decompress --remove-original --target-dir=$topdir/backup 2>&1 |tee  $topdir/decompress_dir.log

  check_sparse_msgs $topdir/decompress_dir.log
  check_sparseness $topdir/backup
  rm -rf $topdir/decompress_dir.log
  restore_and_verify $topdir/backup
  rm -rf $topdir/backup
}

decompress_from_stream() {
  rm -rf $topdir/backup_stream && mkdir $topdir/backup_stream
  xbstream -x -v -C $topdir/backup_stream --decompress < $topdir/backup.xbs 2>&1 |tee $topdir/decompress_stream.log

  check_sparse_msgs $topdir/decompress_stream.log
  check_sparseness $topdir/backup_stream
  rm -rf $topdir/decompress_stream.log
  restore_and_verify $topdir/backup_stream
  rm -rf $topdir/backup_stream
}

restore_and_verify() {
    local TARGETDIR=$1
    xtrabackup --prepare --target-dir=$TARGETDIR --transition-key=123 \
               --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

    stop_server

    rm -rf $mysql_datadir

    xtrabackup --copy-back --target-dir=$TARGETDIR --transition-key=123 \
               --generate-new-master-key \
               --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
    start_server
    verify_db_state test
}

echo "Test with lz4"
check_and_start_server_sparse
vlog "Taking backup with LZ4 compression"
take_backup_local_and_stream "--compress=lz4 --read-buffer-size=1M"
decompress_from_dir
decompress_from_stream
stop_server
rm -rf $mysql_datadir

echo "Test with zstd"
check_and_start_server_sparse
vlog "Taking backup with LZ4 compression"
take_backup_local_and_stream "--compress=zstd --read-buffer-size=1M"
decompress_from_dir
decompress_from_stream
