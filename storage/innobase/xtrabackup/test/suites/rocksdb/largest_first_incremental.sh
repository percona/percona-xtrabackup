#
# Test that RocksDB incremental prepare copies files in largest-first order.
# With --parallel=1 the processing order is deterministic and must be
# strictly descending by file size.
#

require_rocksdb

start_server

init_rocksdb

# Create RocksDB tables with different sizes.
mysql -e "CREATE TABLE t_large (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(255), c VARCHAR(255)) ENGINE=ROCKSDB" test
mysql -e "CREATE TABLE t_medium (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(200)) ENGINE=ROCKSDB" test
mysql -e "CREATE TABLE t_small (a INT PRIMARY KEY) ENGINE=ROCKSDB" test

mysql test <<EOF
SET SESSION cte_max_recursion_depth = 10000;
INSERT INTO t_large (b, c)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 1000)
SELECT REPEAT('x', 200), REPEAT('y', 200) FROM seq;
INSERT INTO t_medium (b)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 200)
SELECT REPEAT('m', 150) FROM seq;
INSERT INTO t_small VALUES (1);
EOF

# Flush to SST
mysql -e "SET GLOBAL rocksdb_force_flush_memtable_now = ON"
sleep 2

# Show SST file sizes for debugging
ls -lS $mysql_datadir/.rocksdb/*.sst 2>/dev/null >&2 || true

###############################################################################
# Take full backup
###############################################################################

xtrabackup --backup --target-dir=$topdir/full --parallel=1

###############################################################################
# Insert more data to create new SST files for the incremental
###############################################################################

mysql test <<EOF
SET SESSION cte_max_recursion_depth = 10000;
INSERT INTO t_large (b, c)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 2000)
SELECT REPEAT('X', 200), REPEAT('Y', 200) FROM seq;
INSERT INTO t_medium (b)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 100)
SELECT REPEAT('M', 150) FROM seq;
INSERT INTO t_small VALUES (2);
EOF

# Flush to create new SST files
mysql -e "SET GLOBAL rocksdb_force_flush_memtable_now = ON"
sleep 2

###############################################################################
# Take incremental backup
###############################################################################

xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/full --parallel=1

# Show RocksDB files in incremental directory
vlog "RocksDB files in incremental backup:"
ls -lS $topdir/inc/.rocksdb/*.sst 2>/dev/null >&2 || true

# Record .sst file sizes before prepare (move_file doesn't log sizes)
sst_sizes=$topdir/sst_sizes.txt
rm -f $sst_sizes
for f in $topdir/inc/.rocksdb/*.sst; do
  [ -f "$f" ] || continue
  fname=$(basename "$f")
  fsize=$(stat -c%s "$f" 2>/dev/null || stat -f%z "$f" 2>/dev/null)
  echo "$fname $fsize" >> $sst_sizes
done

vlog "SST sizes in incremental dir:"
cat $sst_sizes >&2

###############################################################################
# Test: Prepare with --parallel=1 copies RocksDB files largest-first
###############################################################################

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc \
  --target-dir=$topdir/full --parallel=1

# During incremental prepare, copy_incremental_over_full() moves RocksDB files
# using our priority queue. The log has "Moving *.sst to *" lines.
# Extract only the .sst "Moving" lines from the prepare phase.
# Lines after the last "Starting InnoDB instance" belong to incremental prepare.
rocksdb_log=$topdir/rocksdb_inc_move.txt
last_start=$(grep -n "Starting InnoDB instance" $OUTFILE | tail -1 | cut -d: -f1)
if [ -n "$last_start" ]; then
  tail -n +${last_start} $OUTFILE | grep "Moving.*\.sst" | grep -v "Done:" > $rocksdb_log || true
fi

vlog "RocksDB .sst files moved during incremental prepare:"
cat $rocksdb_log >&2

prev_size=999999999999
ordered=true
count=0

while IFS= read -r line; do
  # Extract filename: "Moving .../inc/.rocksdb/000020.sst to ..."
  sst_file=$(echo "$line" | sed -n 's|.*/\([0-9]*\.sst\) to .*|\1|p')
  if [ -z "$sst_file" ]; then
    continue
  fi

  # Look up the size from our recorded sizes
  file_size=$(grep "^${sst_file} " $sst_sizes | awk '{print $2}')
  if [ -z "$file_size" ]; then
    vlog "  WARNING: could not find size for $sst_file, skipping"
    continue
  fi

  vlog "  $sst_file: $file_size bytes"
  if [ "$file_size" -gt "$prev_size" ]; then
    ordered=false
    vlog "  ERROR: size $file_size > previous $prev_size (not largest-first)"
  fi
  prev_size=$file_size
  count=$((count + 1))
done < $rocksdb_log

if [ "$count" -lt 2 ]; then
  die "Only $count RocksDB .sst files found in prepare log, need at least 2 to test ordering"
fi

if [ "$ordered" = "false" ]; then
  die "FAIL: RocksDB .sst files were not moved in largest-first order during incremental prepare"
fi

vlog "PASS: Incremental prepare moves $count RocksDB .sst files in largest-first order"

###############################################################################
# Verify the backup is usable (final prepare + restore)
###############################################################################

xtrabackup --prepare --target-dir=$topdir/full

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/full --parallel=1

start_server

count=$(mysql -N -e "SELECT COUNT(*) FROM t_large" test)
if [ "$count" -lt 3000 ]; then
  die "FAIL: t_large has only $count rows after restore, expected >= 3000"
fi

vlog "PASS: Data verified after RocksDB incremental restore"
