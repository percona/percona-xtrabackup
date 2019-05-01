#
# basic test of DDL and rocksdb
#

. inc/common.sh

require_rocksdb

start_server

init_rocksdb

mysql test <<EOF

CREATE TABLE t1(a INT) ENGINE=RocksDB;
INSERT INTO t1 VALUES (1), (2), (3);

CREATE TABLE t2(a INT) ENGINE=RocksDB;
INSERT INTO t2 VALUES (1), (2), (3);

CREATE TABLE t3(a INT) ENGINE=RocksDB;
INSERT INTO t3 VALUES (1), (2), (3);

CREATE TABLE t4_old(a INT) ENGINE=RocksDB;
INSERT INTO t4_old VALUES (1), (2), (3);

CREATE TABLE t5(a INT) ENGINE=RocksDB;
INSERT INTO t5 VALUES (1), (2), (3);

CREATE TABLE t6(c CHAR(1)) ENGINE=RocksDB;
INSERT INTO t6 VALUES ('a'), ('b'), ('c');

EOF

while true ; do

    echo "INSERT INTO t1 VALUES (4), (5), (6);"
    echo "DROP TABLE t1;"
    echo "CREATE TABLE t1(a CHAR(1)) ENGINE=RocksDB;"
    echo "INSERT INTO t1 VALUES ('1'), ('2'), ('3');"

    echo "INSERT INTO t2 VALUES (4), (5), (6);"
    echo "ALTER TABLE t2 MODIFY a BIGINT;"
    echo "INSERT INTO t2 VALUES (7), (8), (9);"

    echo "INSERT INTO t3 VALUES (4), (5), (6);"
    echo "TRUNCATE t3;"
    echo "INSERT INTO t3 VALUES (7), (8), (9);"

    echo "INSERT INTO t4_old VALUES (4), (5), (6);"
    echo "ALTER TABLE t4_old RENAME t4;"
    echo "INSERT INTO t4 VALUES (7), (8), (9);"

    echo "INSERT INTO t5 VALUES (4), (5), (6);"
    echo "INSERT INTO t6 VALUES ('d'), ('e'), ('f');"

    echo "RENAME TABLE t5 TO temp, t6 TO t5, temp TO t6;"

    echo "INSERT INTO t5 VALUES ('g'), ('h'), ('i');"
    echo "INSERT INTO t6 VALUES (7), (8), (9);"

done | $MYSQL $MYSQL_ARGS test &

mysql_pid=$!

# Backup
xtrabackup --parallel=2 --backup --target-dir=$topdir/backup \
           --debug-sync="log_status_get" &

backup_job_pid=$!

pid_file=$topdir/backup/xtrabackup_debug_sync

# Wait for xtrabackup to suspend
i=0
while [ ! -r "$pid_file" ]
do
    sleep 1
    i=$((i+1))
    echo "Waited $i seconds for $pid_file to be created"
done

xb_pid=`cat $pid_file`

kill $mysql_pid
wait $mysql_pid


record_db_state test

# Resume xtrabackup
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

run_cmd wait $backup_job_pid

# Prepare
xtrabackup --prepare --target-dir=$topdir/backup

stop_server

# Restore
rm -rf $mysql_datadir
xtrabackup --move-back --target-dir=$topdir/backup

start_server

# Verify backup
verify_db_state test
