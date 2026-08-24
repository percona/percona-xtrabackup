#
# Test that decompression processes largest files first.
# With --parallel=1 the processing order is deterministic and must be
# strictly descending by file size.
#

require_lz4

start_server --innodb_file_per_table

# Create tables with dramatically different sizes.
mysql -e "CREATE TABLE t_large (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(255), c VARCHAR(255)) ENGINE=InnoDB" test
mysql -e "CREATE TABLE t_medium (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(255)) ENGINE=InnoDB" test
mysql -e "CREATE TABLE t_small (a INT PRIMARY KEY) ENGINE=InnoDB" test

# Fill tables
# Use recursive CTE for compatibility with both Oracle MySQL and Percona Server.
mysql test <<EOF
INSERT INTO t_large (b, c)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 500)
SELECT REPEAT('x', 200), REPEAT('y', 200) FROM seq;
INSERT INTO t_medium (b)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 100)
SELECT REPEAT('m', 200) FROM seq;
INSERT INTO t_small VALUES (1);
EOF
mysql -e "FLUSH TABLES" test

###############################################################################
# Test: Decompress with --parallel=1 processes largest files first
###############################################################################

# Take a compressed backup
xtrabackup --backup --target-dir=$topdir/backup --compress=lz4 --parallel=1

# Show compressed file sizes for debugging
ls -lS $topdir/backup/test/*.lz4 2>/dev/null || true

# Decompress with --parallel=1
xtrabackup --decompress --target-dir=$topdir/backup --parallel=1

# The decompress log lines have format: "decompressing ./test/t_xxx.ibd.lz4"
# Check that .ibd.lz4 files for our test tables appear in largest-first order.
grep "decompressing.*test/t_" $OUTFILE | head -10

large_line=$(grep -n "decompressing.*test/t_large" $OUTFILE | head -1 | cut -d: -f1)
medium_line=$(grep -n "decompressing.*test/t_medium" $OUTFILE | head -1 | cut -d: -f1)
small_line=$(grep -n "decompressing.*test/t_small" $OUTFILE | head -1 | cut -d: -f1)

vlog "Decompress order: t_large at line $large_line, t_medium at line $medium_line, t_small at line $small_line"

if [ -z "$large_line" ] || [ -z "$medium_line" ] || [ -z "$small_line" ]; then
  die "Could not find all tables in decompress log"
fi

if [ "$large_line" -ge "$medium_line" ]; then
  die "FAIL: t_large (line $large_line) was not decompressed before t_medium (line $medium_line)"
fi

if [ "$medium_line" -ge "$small_line" ]; then
  die "FAIL: t_medium (line $medium_line) was not decompressed before t_small (line $small_line)"
fi

vlog "PASS: Decompress processes files in largest-first order"

###############################################################################
# Verify the backup is usable (prepare + restore)
###############################################################################

xtrabackup --prepare --target-dir=$topdir/backup

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup --parallel=1

start_server

# Verify data
count=$(mysql -N -e "SELECT COUNT(*) FROM t_large" test)
if [ "$count" -lt 500 ]; then
  die "FAIL: t_large has only $count rows after restore, expected >= 500"
fi

vlog "PASS: Data verified after restore"
