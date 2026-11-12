########################################################################
# Back up and restore a table with an HNSW vector index, and check that
# the restored table and vector index are what the server had.
#
# Sourced by suites/vector/index.sh and index_debug.sh, after inc/common.sh.
# A test that defines dump_graph() has it called with the file to write
# the graph of the vector index to, and the graph is compared too.
#
# 10 rows are written after a clean restart, so that they are only in
# the redo the backup copies. Two INSERTs are rolled back after they took
# a label, and one transaction is still open during the backup (--prepare
# rolls it back). Each of the three leaves a node without a row.
#
# The rows, the rows read through the vector index and AUTO_INCREMENT
# are saved on the server before the backup and on the restored server,
# and must be the same. Then new rows must get labels the graph has not
# used.
########################################################################

. inc/vector_index_rows.sh

start_server

require_vector_index

mysql test -e "CREATE TABLE t (id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
                               v VECTOR(4) NOT NULL,
                               VECTOR KEY vk (v) TYPE hnsw) ENGINE=InnoDB"

# A clean restart checkpoints, and no idle flushing keeps the checkpoint
# there: the rows below are only in the redo the backup copies.
shutdown_server
start_server
mysql -e "SET GLOBAL innodb_idle_flush_pct = 0"

# INSERT a row and roll it back: its label is used.
function insert_rolled_back()
{
  mysql test -e "START TRANSACTION;
    INSERT INTO t (v) VALUES (STRING_TO_VECTOR('[9,9,9,9]')); ROLLBACK"
}

insert_rows 4
insert_rolled_back
insert_rows 3
insert_rolled_back
insert_rows 3

# A transaction that is still open when the backup ends.
$MYSQL $MYSQL_ARGS test >/dev/null 2>&1 <<EOF &
START TRANSACTION;
INSERT INTO t (v) VALUES (STRING_TO_VECTOR('[8,8,8,8]'));
SELECT SLEEP(10000);
EOF
open_trx_pid=$!
for i in $(seq 60)
do
  n=$($MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM information_schema.innodb_trx")
  [ "$n" -ge 1 ] && break
  sleep 1
done
[ "$n" -ge 1 ] || die "The open transaction did not start"

snapshot server
[ "$(wc -l < $topdir/server.rows)" -eq 10 ] || die "Expected 10 rows"

xtrabackup --backup --target-dir=$topdir/backup

kill -9 $open_trx_pid
wait $open_trx_pid 2>/dev/null || true
stop_server

# The server refuses to import a table with a vector index, but
# --prepare --export must still work.
cp -a $topdir/backup $topdir/backup_export
xtrabackup --prepare --export --target-dir=$topdir/backup_export
[ -f $topdir/backup_export/test/t.cfg ] || die "t.cfg was not written"
rm -rf $topdir/backup_export

xtrabackup --prepare --target-dir=$topdir/backup
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup
start_server

snapshot restored

for f in rows index autoinc graph
do
  [ -f $topdir/server.$f ] || continue
  diff $topdir/server.$f $topdir/restored.$f ||
    die "The restored $f is not what the server had"
done

# A label handed out again fails the INSERT on the graph.
insert_rows 5
insert_rolled_back
snapshot inserted
[ "$(wc -l < $topdir/inserted.rows)" -eq 15 ] || die "Expected 15 rows"
