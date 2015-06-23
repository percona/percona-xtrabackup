###############################################################################
# Bug #1410339: Set session lock_wait_timeout value to its default value before
#               executing FTWRL
###############################################################################

MYSQLD_EXTRA_MY_CNF_OPTS="
loose-lock_wait_timeout=1
"

start_server

function blocking_statement()
{
    $MYSQL $MYSQL_ARGS test -e "INSERT INTO t VALUES (SLEEP(10000))"
}

function start_innobackupex()
{
    innobackupex --no-timestamp $topdir/backup
}

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t (a INT) ENGINE=MyISAM;
EOF

blocking_statement &
job1=$!

while ! $MYSQL $MYSQL_ARGS -e "SHOW PROCESSLIST" | grep "INSERT"
do
    sleep 1
done

conn_id=`$MYSQL $MYSQL_ARGS -e "SHOW PROCESSLIST" | grep "INSERT" | awk '{print $1;}'`

start_innobackupex &
job2=$!

# xtrabackup does FLUSH TABLES before FLUSH TABLES WITH READ LOCK
# or LOCK TABLES FOR BACKUP
# both are affected by lock_wait_timeout setting
while ! $MYSQL $MYSQL_ARGS -e "SHOW PROCESSLIST" | grep "TABLES"
do
    sleep 1
done

# Make sure we wait longer than lock_wait_timeout
sleep 1

echo "Letting innobackupex to go!"
$MYSQL $MYSQL_ARGS -e "KILL $conn_id"

# Test that FTWRL / LOCK TABLES FOR BACKUP succeeds

wait $job2
