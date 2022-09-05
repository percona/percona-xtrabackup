########################################################################
# register pxb as redo log consummer. Backup should complete
########################################################################
. inc/common.sh
require_server_version_higher_than 8.0.29
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_capacity=8M
"

start_server

function run_inserts()
{
  for i in $(seq 1000);
  do
    $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.redo_log_consumer VALUES (NULL, REPEAT('a', 63 * 1024))";
  done
}

mysql -e "CREATE TABLE redo_log_consumer (
  id INT PRIMARY KEY AUTO_INCREMENT,
  str BLOB
);" test

vlog "Run without redo log consumer"

run_inserts &
PID=$!

run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --stream=xbstream > /dev/null


xtrabackup --backup --target-dir=$topdir/backup --register-redo-log-consumer

# ER_IB_MSG_LOG_WRITER_WAIT_ON_CONSUMER
run_cmd grep -q "Redo log writer is waiting for" ${MYSQLD_ERRFILE}

kill -SIGKILL ${PID}
wait ${PID}

xtrabackup --prepare --target-dir=$topdir/backup
