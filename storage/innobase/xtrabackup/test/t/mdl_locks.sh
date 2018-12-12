# Check lock-ddl, lock-ddl-timeout and lock-ddl-per-table

require_server_version_higher_than 5.7.0

start_server

load_sakila

mysql -e 'CREATE TABLE t_hello_你好(id INT, PRIMARY KEY(id)) ENGINE=InnoDB' test

function start_transaction() {
	run_cmd_expect_failure $MYSQL $MYSQL_ARGS <<EOF
BEGIN;
SELECT * FROM sakila.payment LIMIT 1;
SELECT SLEEP(10000);
EOF
}

function alter_table() {
	run_cmd_expect_failure $MYSQL $MYSQL_ARGS \
		-e "ALTER TABLE sakila.payment ADD COLUMN col1 INT"
}

xtrabackup --backup --lock-ddl \
	--lock-ddl-timeout=2 --target-dir=$topdir/backup1

start_transaction &
tr_job_id=$!

while ! mysql -e 'SHOW PROCESSLIST' | grep -q 'User sleep' ; do
	sleep 1
done

alter_table &
ddl_job_id=$!

while ! mysql -e 'SHOW PROCESSLIST' | grep -q 'Waiting for table metadata lock' ; do
	sleep 1
done

# SELECT blocks ALTER TABLE, ALTER TABLE blocks LOCK TABLES FOR BACKUP
run_cmd_expect_failure \
	$XB_BIN $XB_ARGS --backup --lock-ddl --lock-ddl-timeout=2 \
			 --target-dir=$topdir/backup2

mysql -Ne "SELECT CONCAT('KILL ', id, ';') FROM \
INFORMATION_SCHEMA.PROCESSLIST WHERE info LIKE 'SELECT SLEEP%' \
OR info LIKE 'ALTER TABLE%'" | mysql

wait $tr_job_id
wait $ddl_job_id

xtrabackup --backup --lock-ddl \
	--lock-ddl-timeout=2 --target-dir=$topdir/backup3

mysql -e "CREATE TABLE rcount (val INT)" test
mysql -e "INSERT INTO rcount (val) VALUES (0)" test

function heavy_index_rotation() {
	trap "finish=true" SIGUSR1
	finish="false"
	while [ $finish != "true" ] ; do
		mysql -e "CREATE INDEX idx_payment_date ON payment (payment_date)" sakila
		mysql -e "DROP INDEX idx_payment_date ON payment" sakila
		mysql -e "UPDATE rcount SET val = val + 1" test
	done
}

heavy_index_rotation &
job_id=$!

while [ `mysql -Ne "SELECT val FROM rcount"` -lt "3" ] ; do
	sleep 1
done

xtrabackup --backup --lock-ddl-per-table --target-dir=$topdir/backup5
xtrabackup --prepare --target-dir=$topdir/backup5

if has_backup_locks ;
then
	xtrabackup --backup --lock-ddl --target-dir=$topdir/backup6
	xtrabackup --prepare --target-dir=$topdir/backup6
fi

# PXB-1539: lock-ddl-per-table reports error for table name with special characters
run_cmd_expect_failure grep 'failed to execute query SELECT \* FROM test.t_hello' $OUTFILE

kill -USR1 $job_id
wait $job_id

stop_server
