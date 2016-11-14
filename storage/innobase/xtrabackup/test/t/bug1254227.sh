#########################################################################
# Bug #1254227: xtrabackup_56 does not roll back prepared XA transactions
#########################################################################

is_galera && skip_test "Requires a server without Galera support"

start_server

mkfifo $topdir/fifo

$MYSQL $MYSQL_ARGS <$topdir/fifo &

client_pid=$!

# Open the pipe for writing. This is required to prevent cat from closing the
# pipe when stdout is redirected to it

exec 3>$topdir/fifo

cat >&3 <<EOF
CREATE TABLE test.t(a INT) ENGINE=InnoDB;
XA START 'xatrx';
INSERT INTO test.t VALUES(1);
XA END 'xatrx';
XA PREPARE 'xatrx';
EOF

innobackupex --no-timestamp $topdir/full

# Terminate the background client
echo "exit" >&3
exec 3>&-
wait $client_pid

innobackupex --apply-log $topdir/full

stop_server

rm -rf $MYSQLD_DATADIR/*

innobackupex --copy-back $topdir/full

# The server will fail to start if it has MySQL bug #47134 fixed.
start_server
