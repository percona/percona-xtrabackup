. inc/common.sh

vlog "#"
vlog "# PXB-2955 : Implement dictionary cache"
vlog "#"

function wait_for_bg_trx() {
# we should NOT exit if grep fails or mysql client fails. We have to retry
# hence exit on err should be avoided.
set +e
 while true; do
   $MYSQL $MYSQL_ARGS -e "SHOW PROCESSLIST" |  grep -i 'SELECT SLEEP'
   if [ $? -eq 0 ]; then
	   break;
   fi
   vlog "waiting for background trx to finish inserting data and reach to sleep"
   sleep 1
 done

set -e

 vlog "Wait for trx data to be flushed. This guarantees rollback during prepare"
 innodb_wait_for_flush_all
}

function kill_bg_trx() {
 # Following commands in function can fail. Either success or failure is OK
 # Also used by 'trap'. Hence we unset the 'e' flag which Exit immediately if a
 # command exits with a non-zero status.
 set +e
 TRX_PID=$1
 vlog "Kill the Long running uncommited transaction after the backup"
 kill_query_pattern "INFO like '%SLEEP%'"

 kill -SIGKILL $TRX_PID

 vlog "Wait for killed process to exit"
 wait $TRX_PID
 set -e
}

function check_rollback_msg() {
FILE=$1
egrep -q "Rolling back trx with id [0-9]+, [0-9]+ rows to undo" $FILE || die "Expected rollback didn't happen during prepare"
}

function create_tablespace() {
 mysql test << EOF
 CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
 CREATE TABLE t11(a INT) TABLESPACE=ts1;
 CREATE TABLE t12(a INT, b INT) TABLESPACE=ts1;
EOF
}

function create_tables() {
  TABLE=$1
  # optional parameter 2 for TABLESPACE
  TABLESPACE=${2:-}
  if [ ! -z ${TABLESPACE} ];
  then
    TABLESPACE="TABLESPACE=$TABLESPACE"
  fi
  mysql test << EOF
    CREATE TABLE $TABLE (
    id INT PRIMARY KEY,
    val INT NOT NULL DEFAULT 0
    ) $TABLESPACE ENGINE=InnoDB
EOF

  mysql -e "INSERT INTO $TABLE (id) VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9),(10)" test
  vlog "Long running transaction that modifies table data but is not committed at the time of backup"
  $MYSQL $MYSQL_ARGS -e "\
   BEGIN;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   INSERT INTO $TABLE (id) SELECT id + (SELECT MAX(id) FROM $TABLE) FROM $TABLE;\
   SELECT SLEEP (10000);
 " test &
 uncommitted_trx=$!

 trap "kill_bg_trx $uncommitted_trx" EXIT

 wait_for_bg_trx

 vlog "Backup"
 rm -rf $topdir/backup
 xtrabackup --backup --target-dir=$topdir/backup 2>&1 | tee $topdir/pxb.log

 kill_bg_trx $uncommitted_trx

 vlog "Record db state"
 record_db_state test

 vlog "Prepare the backup"
 xtrabackup --prepare --target-dir=$topdir/backup 2>&1 | tee $topdir/pxb_prepare.log
 check_rollback_msg $topdir/pxb_prepare.log

 vlog "Restore"
 stop_server
 rm -rf $MYSQLD_DATADIR/*
 xtrabackup --move-back --target-dir=${topdir}/backup
 start_server
 verify_db_state test
}

function create_part_table() {
  vlog "Create partition table"
  mysql  test << EOF
  DROP TABLE IF EXISTS t1;
  CREATE TABLE t1 (
  a INT,
  pad1 CHAR(250) NOT NULL DEFAULT "a",
  pad2 CHAR(250) NOT NULL DEFAULT "a",
  pad3 CHAR(250) NOT NULL DEFAULT "a",
  pad4 CHAR(250) NOT NULL DEFAULT "a"
  ) ENGINE=InnoDB DEFAULT CHARSET=latin1
  PARTITION BY RANGE (a)
  (PARTITION p0 VALUES LESS THAN (2000) ENGINE = InnoDB,
   PARTITION P1 VALUES LESS THAN (4000) ENGINE = InnoDB,
   PARTITION p2 VALUES LESS THAN (6000) ENGINE = InnoDB,
   PARTITION p3 VALUES LESS THAN (8000) ENGINE = InnoDB,
   PARTITION p4 VALUES LESS THAN MAXVALUE ENGINE = InnoDB);
EOF
}

function load_data_part_table() {
  vlog "Load data"
  mysql test << EOF
  SET cte_max_recursion_depth=10000;
  INSERT INTO t1 WITH RECURSIVE cte (n) AS (   SELECT 1   UNION ALL   SELECT n + 1 FROM cte WHERE n < 10000 ) SELECT  *, REPEAT('a', 250) , REPEAT('b',250) , REPEAT('c', 250), REPEAT('d',250) FROM cte;
EOF
}

function create_part_sub_table() {
  vlog "Create partition table with sub partitions"
  mysql  test << EOF
  DROP TABLE IF EXISTS t1;
  CREATE TABLE t1 (
   a INT,
   pad1 CHAR(250) NOT NULL DEFAULT "a",
   pad2 CHAR(250) NOT NULL DEFAULT "a",
   pad3 CHAR(250) NOT NULL DEFAULT "a",
   pad4 CHAR(250) NOT NULL DEFAULT "a"
   ) ENGINE=InnoDB DEFAULT CHARSET=latin1
   PARTITION BY RANGE (a)
   SUBPARTITION BY HASH(a) SUBPARTITIONS 5
   (PARTITION p0 VALUES LESS THAN (2000),
    PARTITION P1 VALUES LESS THAN (4000),
    PARTITION p2 VALUES LESS THAN (6000),
    PARTITION p3 VALUES LESS THAN (8000),
    PARTITION p4 VALUES LESS THAN MAXVALUE);
EOF
}

function test_partition() {
  START=$1
  END=$2

  vlog "Long running transaction that modifies table data but is not committed at the time of backup"
 $MYSQL $MYSQL_ARGS -e "\
   BEGIN;\
   DELETE FROM t1 WHERE a>$START AND a < $END; \
   SELECT SLEEP (10000);
 " test &
 uncommitted_trx=$!

 trap "kill_bg_trx $uncommitted_trx" EXIT

 wait_for_bg_trx

 vlog "Backup"
 rm -rf $topdir/backup
 xtrabackup --backup --target-dir=$topdir/backup 2>&1 | tee $topdir/pxb.log

 kill_bg_trx $uncommitted_trx

 vlog "Record db state"
 record_db_state test

 vlog "Prepare the backup"
 xtrabackup --prepare --target-dir=$topdir/backup 2>&1 | tee $topdir/pxb_prepare.log
 check_rollback_msg $topdir/pxb_prepare.log

 vlog "Restore"
 stop_server
 rm -rf $MYSQLD_DATADIR/*
 xtrabackup --move-back --target-dir=${topdir}/backup
 start_server
 verify_db_state test
}
# This function uses a datadir that contains table with duplicate SDI
# Such tables are possible with an older version of server. Duplicate SDI
# can continue to remain even after an upgrade.
# Test rollback on such tables

function duplicate_sdi() {

  stop_server
  rm -rf $topdir/backup && mkdir -p $topdir/backup
  run_cmd tar -xf inc/duplicate_sdi_backup.tar.gz -C $topdir/backup
  vlog "prepare the backup dir with dup SDI"
  xtrabackup --prepare --target-dir=$topdir/backup
  #restore
  [[ -d $mysql_datadir ]] && rm -rf $mysql_datadir && mkdir -p $mysql_datadir
  vlog "Restoring the dup SDI prepared backup"
  xtrabackup --copy-back --target-dir=$topdir/backup

  MYSQLD_START_TIMEOUT=1200
  MYSQLD_EXTRA_MY_CNF_OPTS="
  lower_case_table_names
  "
  start_server

  vlog "Load more data to tables with duplicate SDI"
  mysql test <<EOF
  SET cte_max_recursion_depth=5000;
  INSERT INTO t1 WITH RECURSIVE cte (n) AS (   SELECT 105   UNION ALL   SELECT n + 1 FROM cte WHERE n < 4999 ) SELECT * FROM cte;
  INSERT INTO pt1 WITH RECURSIVE cte (n) AS (   SELECT 5   UNION ALL   SELECT n + 1 FROM cte WHERE n < 2999 ) SELECT * FROM cte;
EOF

  vlog "Long running transaction that modifies table data but is not committed at the time of backup"

$MYSQL $MYSQL_ARGS -e "\
   BEGIN;\
   DELETE FROM t1 WHERE a < 4000;
   DELETE FROM pt1 WHERE a < 2500;
   SELECT SLEEP (10000);
 " test &
 uncommitted_trx=$!

 trap "kill_bg_trx $uncommitted_trx" EXIT

 wait_for_bg_trx

 vlog "Backup"
 rm -rf $topdir/backup
 xtrabackup --backup --target-dir=$topdir/backup 2>&1 | tee $topdir/pxb.log

 kill_bg_trx $uncommitted_trx

 vlog "Record db state"
 record_db_state test

 vlog "Prepare the backup"
 xtrabackup --prepare --target-dir=$topdir/backup 2>&1 | tee $topdir/pxb_prepare.log
 check_rollback_msg $topdir/pxb_prepare.log

 vlog "Restore"
 stop_server
 rm -rf $MYSQLD_DATADIR/*
 xtrabackup --move-back --target-dir=${topdir}/backup
 start_server
 verify_db_state test
}
