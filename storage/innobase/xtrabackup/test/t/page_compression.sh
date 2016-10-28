#
# Basic tests InnoDB page compression
#

. inc/common.sh

require_server_version_higher_than 5.7.9

start_server --innodb_file_per_table

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

CREATE TABLE t1 (c1 BLOB) COMPRESSION="zlib";

INSERT INTO t1 VALUES (REPEAT('x', 5000));

INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;

EOF

# wait for InnoDB to flush all dirty pages
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;

EOF

# wait for InnoDB to flush all dirty pages
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup1 --incremental-basedir=$topdir/backup

record_db_state test

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup --incremental-dir=$topdir/backup1

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

verify_db_state test
