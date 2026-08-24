#
# Test that parallel delta application processes largest .delta files first.
# With --parallel=1 the processing order is deterministic and must be
# strictly descending by .delta file size.
#

start_server --innodb_file_per_table

# Create tables with dramatically different sizes.
# Use recursive CTE for compatibility with both Oracle MySQL and Percona Server.
mysql -e "CREATE TABLE t_large (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(255), c VARCHAR(255)) ENGINE=InnoDB" test
mysql -e "CREATE TABLE t_medium (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(255)) ENGINE=InnoDB" test
mysql -e "CREATE TABLE t_small (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(50)) ENGINE=InnoDB" test

mysql test <<EOF
SET SESSION cte_max_recursion_depth = 10000;
INSERT INTO t_large (b, c)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 5000)
SELECT REPEAT('x', 250), REPEAT('y', 250) FROM seq;
INSERT INTO t_medium (b)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 500)
SELECT REPEAT('m', 200) FROM seq;
INSERT INTO t_small (b)
WITH RECURSIVE seq AS (SELECT 1 AS n UNION ALL SELECT n+1 FROM seq WHERE n < 20)
SELECT REPEAT('s', 40) FROM seq;
EOF
mysql -e "FLUSH TABLES" test

###############################################################################
# Take full backup
###############################################################################

xtrabackup --backup --target-dir=$topdir/full --parallel=1

###############################################################################
# Modify all tables to create .delta files of different sizes.
# UPDATE all rows to dirty all pages, making .delta size proportional to table.
###############################################################################

mysql test <<EOF
SET SESSION cte_max_recursion_depth = 10000;
UPDATE t_large SET b = REPEAT('X', 250), c = REPEAT('Y', 250);
UPDATE t_medium SET b = REPEAT('M', 200);
UPDATE t_small SET b = REPEAT('S', 40);
EOF
mysql -e "FLUSH TABLES" test

###############################################################################
# Take incremental backup
###############################################################################

xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/full --parallel=1

# Show .delta file sizes for debugging
vlog "Delta file sizes in incremental backup:"
ls -lS $topdir/inc/test/*.delta 2>/dev/null >&2 || true

# Record .delta file sizes into an associative structure for later verification.
# Build a mapping: filename -> size
delta_sizes=$topdir/delta_sizes.txt
for f in $topdir/inc/test/*.delta; do
  fname=$(basename "$f")
  fsize=$(stat -c%s "$f" 2>/dev/null || stat -f%z "$f" 2>/dev/null)
  echo "$fname $fsize" >> $delta_sizes
done

vlog "Delta sizes:"
cat $delta_sizes >&2

###############################################################################
# Test: Prepare with --parallel=1 applies largest .delta first
###############################################################################

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc \
  --target-dir=$topdir/full --parallel=1

# Extract the "Applying" lines from the log (only those referencing test/ deltas)
apply_log=$topdir/apply_order.txt
grep "Applying.*test/.*\.delta" $OUTFILE > $apply_log || true

vlog "Delta apply order from log:"
cat $apply_log >&2

# Verify that files were applied in descending .delta file size order
prev_size=999999999999
ordered=true
count=0

while IFS= read -r line; do
  # Extract filename from "Applying /path/to/inc/test/t_xxx.ibd.delta to ..."
  delta_file=$(echo "$line" | sed -n 's|.*Applying .*/test/\([^ ]*\.delta\) to .*|\1|p')
  if [ -z "$delta_file" ]; then
    continue
  fi

  # Look up the size from our recorded sizes
  file_size=$(grep "^${delta_file} " $delta_sizes | awk '{print $2}')
  if [ -z "$file_size" ]; then
    vlog "  WARNING: could not find size for $delta_file, skipping"
    continue
  fi

  vlog "  $delta_file: $file_size bytes"
  if [ "$file_size" -gt "$prev_size" ]; then
    ordered=false
    vlog "  ERROR: size $file_size > previous $prev_size (not largest-first)"
  fi
  prev_size=$file_size
  count=$((count + 1))
done < $apply_log

if [ "$count" -lt 3 ]; then
  die "Only $count .delta files found in apply log, need at least 3"
fi

if [ "$ordered" = "false" ]; then
  die "FAIL: .delta files were not applied in largest-first order"
fi

vlog "PASS: Delta apply processes $count .delta files in largest-first order"

###############################################################################
# Verify the backup is usable (final prepare + restore)
###############################################################################

xtrabackup --prepare --target-dir=$topdir/full

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/full --parallel=1

start_server

count=$(mysql -N -e "SELECT COUNT(*) FROM t_large" test)
if [ "$count" -lt 5000 ]; then
  die "FAIL: t_large has only $count rows after restore, expected >= 5000"
fi

vlog "PASS: Data verified after incremental restore"
