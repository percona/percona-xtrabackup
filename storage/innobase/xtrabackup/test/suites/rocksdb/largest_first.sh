#
# Test that parallel processing handles largest RocksDB files first.
# With --parallel=1 the processing order is deterministic and must be
# strictly descending by file size.
#

require_rocksdb

start_server

init_rocksdb

# Create a RocksDB table with enough data to produce multiple .sst files
# of different sizes after compaction.
mysql -e "CREATE TABLE t_large (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(255), c VARCHAR(255)) ENGINE=ROCKSDB" test
mysql -e "CREATE TABLE t_small (a INT PRIMARY KEY) ENGINE=ROCKSDB" test

# Fill t_large to produce large .sst files (bulk insert for speed)
# Use recursive CTE for compatibility with both Oracle MySQL and Percona Server.
mysql test <<EOF
SET SESSION cte_max_recursion_depth = 10000;
INSERT INTO t_large (b, c)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 1000)
SELECT REPEAT('x', 200), REPEAT('y', 200) FROM seq;
INSERT INTO t_small VALUES (1);
EOF

# Force RocksDB to flush memtable to SST files
mysql -e "SET GLOBAL rocksdb_force_flush_memtable_now = ON"

# Give compaction time to settle
sleep 2

# Show .sst file sizes for debugging
ls -lS $mysql_datadir/.rocksdb/*.sst 2>/dev/null >&2 || true

###############################################################################
# Test 1: Backup with --parallel=1 processes largest .sst first
###############################################################################

xtrabackup --backup --target-dir=$topdir/backup --parallel=1

# The log now prints ", size <bytes> (<human>)" for each file.
# Extract sizes from .sst copy lines and verify non-increasing order.
backup_sst_log=$topdir/backup_sst_lines.txt
grep "Copying.*\.sst.*size [0-9]" $OUTFILE | grep -v "Done:" > $backup_sst_log || true

vlog "SST files copied during backup:"

prev_size=999999999999
ordered=true
count=0

while IFS= read -r line; do
  file_size=$(echo "$line" | sed -n 's/.*, size \([0-9]*\) .*/\1/p')
  file_name=$(echo "$line" | sed -n 's/.*Copying \([^ ]*\.sst\).*/\1/p')
  if [ -z "$file_size" ] || [ -z "$file_name" ]; then
    continue
  fi
  vlog "  $file_name: $file_size bytes"
  if [ "$file_size" -gt "$prev_size" ]; then
    ordered=false
    vlog "  ERROR: size $file_size > previous $prev_size (not largest-first)"
  fi
  prev_size=$file_size
  count=$((count + 1))
done < $backup_sst_log

if [ "$count" -lt 2 ]; then
  die "Only $count .sst files found in backup log, need at least 2 to test ordering"
fi

if [ "$ordered" = "false" ]; then
  die "FAIL: RocksDB .sst files were not backed up in largest-first order"
fi

vlog "PASS: Backup processes $count RocksDB .sst files in largest-first order"

###############################################################################
# Test 2: Copy-back with --parallel=1 processes largest .sst first
###############################################################################

xtrabackup --prepare --target-dir=$topdir/backup

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup --parallel=1

# Exclude checkpoint lines (those are from the backup phase).
copyback_sst_log=$topdir/copyback_sst_lines.txt
grep "Copying.*\.sst.*size [0-9]" $OUTFILE | grep -v "Done:" | grep -v "checkpoint" > $copyback_sst_log || true

vlog "SST files copied during copy-back:"

prev_size=999999999999
ordered=true
count=0

while IFS= read -r line; do
  file_size=$(echo "$line" | sed -n 's/.*, size \([0-9]*\) .*/\1/p')
  file_name=$(echo "$line" | sed -n 's/.*Copying \([^ ]*\.sst\).*/\1/p')
  if [ -z "$file_size" ] || [ -z "$file_name" ]; then
    continue
  fi
  vlog "  $file_name: $file_size bytes"
  if [ "$file_size" -gt "$prev_size" ]; then
    ordered=false
    vlog "  ERROR: size $file_size > previous $prev_size (not largest-first)"
  fi
  prev_size=$file_size
  count=$((count + 1))
done < $copyback_sst_log

if [ "$count" -lt 2 ]; then
  die "Only $count .sst files found in copy-back log, need at least 2 to test ordering"
fi

if [ "$ordered" = "false" ]; then
  die "FAIL: RocksDB .sst files were not copied back in largest-first order"
fi

vlog "PASS: Copy-back processes $count RocksDB .sst files in largest-first order"
