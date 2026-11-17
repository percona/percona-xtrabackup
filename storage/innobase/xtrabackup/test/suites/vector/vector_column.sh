########################################################################
# --prepare rolls back a transaction on a table with a VECTOR
# column
#
# To roll back, --prepare builds the table from its SDI, and has to know
# how InnoDB stores every column type. Runs on any server with the VECTOR
# type; no vector index is needed.
########################################################################

. inc/common.sh

start_server

if ! $MYSQL $MYSQL_ARGS -e "CREATE TABLE test.vec_probe (v VECTOR(2))" \
      >/dev/null 2>&1
then
  skip_test "Requires a server with the VECTOR type"
fi
mysql -e "DROP TABLE test.vec_probe"

mysql test <<EOF
CREATE TABLE t (id INT PRIMARY KEY, v VECTOR(4), c INT, KEY k (c))
  ENGINE=InnoDB;
INSERT INTO t VALUES (1, STRING_TO_VECTOR('[1,0,0,0]'), 1),
                     (2, STRING_TO_VECTOR('[0,1,0,0]'), 2);
EOF

# Open when the backup ends: --prepare rolls it back.
$MYSQL $MYSQL_ARGS test >/dev/null 2>&1 <<EOF &
START TRANSACTION;
INSERT INTO t VALUES (3, STRING_TO_VECTOR('[0,0,1,0]'), 3);
UPDATE t SET v = STRING_TO_VECTOR('[9,9,9,9]'), c = 10 WHERE id = 1;
DELETE FROM t WHERE id = 2;
SELECT SLEEP(10000);
EOF
open_trx_pid=$!

for i in $(seq 60)
do
  n=$($MYSQL $MYSQL_ARGS -Ns -e "SELECT COALESCE(SUM(trx_rows_modified), 0)
        FROM information_schema.innodb_trx")
  [ "$n" -ge 3 ] && break
  sleep 1
done
[ "$n" -ge 3 ] || die "The open transaction did not change its rows"

xtrabackup --backup --target-dir=$topdir/backup

kill -9 $open_trx_pid
wait $open_trx_pid 2>/dev/null || true
stop_server

xtrabackup --prepare --target-dir=$topdir/backup
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup
start_server

result=$($MYSQL $MYSQL_ARGS -Ns test -e "SELECT id, VECTOR_TO_STRING(v), c
  FROM t ORDER BY id")
expected=$(printf '1\t[1.00000e+00,0.00000e+00,0.00000e+00,0.00000e+00]\t1
2\t[0.00000e+00,1.00000e+00,0.00000e+00,0.00000e+00]\t2')
[ "$result" = "$expected" ] || die "Unexpected rows after the restore: $result"

mysql test -e "CHECK TABLE t" | grep -w OK || die "CHECK TABLE t failed"
