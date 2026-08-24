#
# Test that parallel processing handles largest InnoDB files first.
# With --parallel=1 the processing order is deterministic and must be
# strictly descending by file size.
#

start_server --innodb_file_per_table

# Create tables with dramatically different sizes.
mysql -e "CREATE TABLE t_large (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(255), c VARCHAR(255)) ENGINE=InnoDB" test
mysql -e "CREATE TABLE t_medium (a INT AUTO_INCREMENT PRIMARY KEY, b VARCHAR(255)) ENGINE=InnoDB" test
mysql -e "CREATE TABLE t_small (a INT PRIMARY KEY) ENGINE=InnoDB" test

# Fill tables to ensure significant size differences.
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

# Record actual file sizes for debugging
ls -lS $mysql_datadir/test/*.ibd

###############################################################################
# Test 1: Backup with --parallel=1 processes largest .ibd first
###############################################################################

xtrabackup --backup --target-dir=$topdir/backup --parallel=1

# The log now prints "size <bytes>" for each file copied. Extract sizes from
# lines matching our test tables and verify they are in non-increasing order.
backup_log=$topdir/backup_sizes.txt
grep "space_id.*test/t_" $OUTFILE | grep "size [0-9]" | grep -v "Done:" > $backup_log

vlog "InnoDB files copied during backup:"

prev_size=999999999999
ordered=true
count=0

while IFS= read -r line; do
  file_size=$(echo "$line" | sed -n 's/.*, size \([0-9]*\) .*/\1/p')
  file_name=$(echo "$line" | sed -n 's|.*\(\.*/test/t_[^ ]*\).*|\1|p')
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
done < $backup_log

if [ "$count" -lt 3 ]; then
  die "Only $count test tables found in backup log, need at least 3"
fi

if [ "$ordered" = "false" ]; then
  die "FAIL: InnoDB files were not backed up in largest-first order"
fi

vlog "PASS: Backup processes $count InnoDB files in largest-first order"

###############################################################################
# Test 2: Copy-back with --parallel=1 processes largest .ibd first
###############################################################################

xtrabackup --prepare --target-dir=$topdir/backup

stop_server
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup --parallel=1

# Copy-back log uses "Copying <path> to <dest>, size <bytes> (<human>)".
# Exclude backup-phase lines by filtering out "space_id" (present in backup logs).
copyback_log=$topdir/copyback_sizes.txt
grep "Copying.*test/t_.*size [0-9]" $OUTFILE | grep -v "Done:" | grep -v "space_id" > $copyback_log

vlog "InnoDB files copied during copy-back:"

prev_size=999999999999
ordered=true
count=0

while IFS= read -r line; do
  file_size=$(echo "$line" | sed -n 's/.*, size \([0-9]*\) .*/\1/p')
  file_name=$(echo "$line" | sed -n 's|.*\(\.*/test/t_[^ ]*\).*|\1|p')
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
done < $copyback_log

if [ "$count" -lt 3 ]; then
  die "Only $count test tables found in copy-back log, need at least 3"
fi

if [ "$ordered" = "false" ]; then
  die "FAIL: InnoDB files were not copied back in largest-first order"
fi

vlog "PASS: Copy-back processes $count InnoDB files in largest-first order"
