
. inc/common.sh

require_server_version_higher_than 5.7.9


ext_dir=${TEST_VAR_ROOT}/ext_dir
mkdir $ext_dir

start_server --innodb-directories="$ext_dir"

run_cmd $MYSQL $MYSQL_ARGS <<EOF

CREATE TABLESPACE ts1 ADD DATAFILE '$ext_dir/data1.ibd' ENGINE=InnoDB;
CREATE TABLESPACE ts2 ADD DATAFILE 'data2.ibd' ENGINE=InnoDB;

CREATE TABLE test.t1_1 (c1 INT PRIMARY KEY) TABLESPACE ts1 ROW_FORMAT=REDUNDANT;
CREATE TABLE test.t1_2 (c1 INT PRIMARY KEY) TABLESPACE ts1 ROW_FORMAT=COMPACT;
CREATE TABLE test.t1_3 (c1 INT PRIMARY KEY) TABLESPACE ts1 ROW_FORMAT=DYNAMIC;

INSERT INTO test.t1_1 VALUES (1), (2), (3);
INSERT INTO test.t1_2 VALUES (10), (20), (30);
INSERT INTO test.t1_3 VALUES (100), (200), (300);

CREATE TABLE test.t2_1 (c1 INT PRIMARY KEY) TABLESPACE ts2 ROW_FORMAT=REDUNDANT;
CREATE TABLE test.t2_2 (c1 INT PRIMARY KEY) TABLESPACE ts2 ROW_FORMAT=COMPACT;
CREATE TABLE test.t2_3 (c1 INT PRIMARY KEY) TABLESPACE ts2 ROW_FORMAT=DYNAMIC;

INSERT INTO test.t2_1 VALUES (1), (2), (3);
INSERT INTO test.t2_2 VALUES (10), (20), (30);
INSERT INTO test.t2_3 VALUES (100), (200), (300);
EOF

xtrabackup --backup --target-dir=$topdir/full

run_cmd $MYSQL $MYSQL_ARGS <<EOF

INSERT INTO test.t1_1 VALUES (11), (12), (13);
INSERT INTO test.t1_2 VALUES (110), (120), (130);
INSERT INTO test.t1_3 VALUES (1100), (1200), (1300);

INSERT INTO test.t2_1 VALUES (11), (12), (13);
INSERT INTO test.t2_2 VALUES (110), (120), (130);
INSERT INTO test.t2_3 VALUES (1100), (1200), (1300);

EOF

xtrabackup --backup --target-dir=$topdir/inc \
	   --incremental-basedir=$topdir/full --lock-ddl=OFF \
	   --debug-sync="data_copy_thread_func" &

job_pid=$!

pid_file=$topdir/inc/xtrabackup_debug_sync

# Wait for xtrabackup to suspend
i=0
while [ ! -r "$pid_file" ]
do
    sleep 1
    i=$((i+1))
    echo "Waited $i seconds for $pid_file to be created"
done

xb_pid=`cat $pid_file`


run_cmd $MYSQL $MYSQL_ARGS <<EOF

CREATE TABLESPACE ts3 ADD DATAFILE 'data3.ibd' ENGINE=InnoDB;

CREATE TABLE test.t3_1 (c1 INT PRIMARY KEY) ROW_FORMAT=DYNAMIC;
CREATE TABLE test.t3_2 (c1 INT PRIMARY KEY) ROW_FORMAT=DYNAMIC;
CREATE TABLE test.t3_3 (c1 INT PRIMARY KEY) ROW_FORMAT=DYNAMIC;

INSERT INTO test.t3_1 VALUES (1), (2), (3);
INSERT INTO test.t3_2 VALUES (10), (20), (30);
INSERT INTO test.t3_3 VALUES (100), (200), (300);

# TODO: uncomment once crash recovery for ALTER TABLE ... TABLESPACE is fixed

# ALTER TABLE test.t3_1 TABLESPACE ts3;
# ALTER TABLE test.t3_2 TABLESPACE ts3;
# ALTER TABLE test.t3_3 TABLESPACE ts3;

INSERT INTO test.t3_1 VALUES (11), (12), (13);
INSERT INTO test.t3_2 VALUES (110), (120), (130);
INSERT INTO test.t3_3 VALUES (1100), (1200), (1300);

EOF

# Resume xtrabackup
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

run_cmd wait $job_pid

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full

xtrabackup --prepare --target-dir=$topdir/full --incremental-dir=$topdir/inc

record_db_state test

stop_server

rm -rf $mysql_datadir
rm -rf $ext_dir

xtrabackup --copy-back --target-dir=$topdir/full

start_server --innodb-directories="$ext_dir/"

$MYSQL $MYSQL_ARGS -e "SELECT * FROM test.t1_1"
$MYSQL $MYSQL_ARGS -e "SELECT * FROM test.t1_2"
$MYSQL $MYSQL_ARGS -e "SELECT * FROM test.t1_3"

$MYSQL $MYSQL_ARGS -e "SELECT * FROM test.t2_1"
$MYSQL $MYSQL_ARGS -e "SELECT * FROM test.t2_2"
$MYSQL $MYSQL_ARGS -e "SELECT * FROM test.t2_3"

$MYSQL $MYSQL_ARGS -e "SELECT * FROM test.t3_1"
$MYSQL $MYSQL_ARGS -e "SELECT * FROM test.t3_2"
$MYSQL $MYSQL_ARGS -e "SELECT * FROM test.t3_3"

verify_db_state test
