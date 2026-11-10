########################################################################
# Prepare rolls back a large uncommitted transaction when the backup copied
# only a small redo log.
#
# After the redo apply the prepare writes the redo of the rollback to new
# redo log files. The redo log capacity of the prepare must leave room for
# them independently of the size of the copied redo log (xtrabackup_logfile),
# which can be much smaller than the redo of the rollback.
########################################################################

. inc/common.sh

start_server

mysql test <<EOF
CREATE TABLE t (id INT PRIMARY KEY AUTO_INCREMENT, c CHAR(200)) ENGINE=InnoDB;
SET SESSION cte_max_recursion_depth = 1000000;
INSERT INTO t (c)
  WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM seq
                            WHERE n < 200000)
  SELECT REPEAT('a', 200) FROM seq;
EOF

# The rollback of this transaction writes far more redo than the backup copies.
$MYSQL $MYSQL_ARGS test <<EOF &
START TRANSACTION;
UPDATE t SET c = REPEAT('b', 200);
SELECT SLEEP(10000);
EOF
client_pid=$!

while [ "$($MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM \
information_schema.innodb_trx WHERE trx_rows_modified >= 200000")" != 1 ]
do
  sleep 1
done

# Advance the checkpoint, so that the backup copies only a little redo.
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup

kill -SIGKILL $client_pid
wait $client_pid || true

vlog "Copied redo log: $(stat -c %s $topdir/backup/xtrabackup_logfile) bytes"

xtrabackup --prepare --target-dir=$topdir/backup

if ! grep -q "Rolling back trx with id .*, 200000 rows to undo" $OUTFILE; then
  die "The prepare did not roll back the uncommitted transaction"
fi

# With too little redo log capacity the rollback stalls waiting for
# checkpoints, or asserts in the log_files_governor.
if grep -q "MY-014084" $OUTFILE; then
  die "The prepare ran out of redo log capacity during the rollback"
fi

stop_server
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup
start_server

count=$($MYSQL $MYSQL_ARGS -Ns -e "SELECT COUNT(*) FROM t WHERE c = REPEAT('a', 200)" test)
if [ "$count" != 200000 ]; then
  die "Expected 200000 rows with their original values, found $count"
fi
