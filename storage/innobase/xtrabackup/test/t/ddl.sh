############################################################################
# Bug #722638: xtrabackup: taking backup while tables are droped and
#              created breaks backup
#
# Bug #1079700: Issues with renaming/rotating tables during the backup stage
############################################################################

. inc/common.sh

require_debug_sync

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table
"

start_server

mysql test <<EOF

CREATE TABLE t1(a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1), (2), (3);

CREATE TABLE t2(a INT) ENGINE=InnoDB;
INSERT INTO t2 VALUES (1), (2), (3);

CREATE TABLE t3(a INT) ENGINE=InnoDB;
INSERT INTO t3 VALUES (1), (2), (3);

CREATE TABLE t4_old(a INT) ENGINE=InnoDB;
INSERT INTO t4_old VALUES (1), (2), (3);

CREATE TABLE t5(a INT) ENGINE=InnoDB;
INSERT INTO t5 VALUES (1), (2), (3);

CREATE TABLE t6(c CHAR(1)) ENGINE=InnoDB;
INSERT INTO t6 VALUES ('a'), ('b'), ('c');

EOF

# Make a checkpoint so that original tablespace creation events are not in the
# xtrabackup log
shutdown_server
start_server

mkdir -p $topdir/backup

# Backup
xtrabackup --datadir=$mysql_datadir --lock-ddl=OFF --backup \
    --target-dir=$topdir/backup \
    --debug-sync="data_copy_thread_func" &

job_pid=$!

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

# Modify the original tables, then change spaces ids by running DDL

mysql test <<EOF

INSERT INTO t1 VALUES (4), (5), (6);
DROP TABLE t1;
CREATE TABLE t1(a CHAR(1)) ENGINE=InnoDB;
INSERT INTO t1 VALUES ("1"), ("2"), ("3");

INSERT INTO t2 VALUES (4), (5), (6);
ALTER TABLE t2 MODIFY a BIGINT;
INSERT INTO t2 VALUES (7), (8), (9);

INSERT INTO t3 VALUES (4), (5), (6);
TRUNCATE t3;
INSERT INTO t3 VALUES (7), (8), (9);

INSERT INTO t4_old VALUES (4), (5), (6);
ALTER TABLE t4_old RENAME t4;
INSERT INTO t4 VALUES (7), (8), (9);

INSERT INTO t5 VALUES (4), (5), (6);
INSERT INTO t6 VALUES ('d'), ('e'), ('f');

# Rotate tables t5 and t6
RENAME TABLE t5 TO temp, t6 TO t5, temp TO t6;

INSERT INTO t5 VALUES ('g'), ('h'), ('i');
INSERT INTO t6 VALUES (7), (8), (9);

EOF

record_db_state test

# Resume xtrabackup
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

run_cmd wait $job_pid

# Prepare
xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/backup

stop_server

# Restore
rm -rf $mysql_datadir
xtrabackup --move-back --target-dir=$topdir/backup

start_server

# Verify backup
verify_db_state test
