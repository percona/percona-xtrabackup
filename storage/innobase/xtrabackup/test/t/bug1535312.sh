#
#   Bug 1535312: Failed to prepare incremental backup if general 
#                tablespace has been recreated between backups
#

require_server_version_higher_than 5.7.0

start_server

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';

CREATE TABLE t (a INT) TABLESPACE ts1;

INSERT INTO t VALUES (1), (2), (3);

EOF

xtrabackup --backup --target-dir=$topdir/full

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

DROP TABLE t;

DROP TABLESPACE ts1;

CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' FILE_BLOCK_SIZE = 8192;

CREATE TABLE p (a INT) TABLESPACE ts1 ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8;

INSERT INTO p VALUES (10), (20), (30);

EOF

xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/full

record_db_state test

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full

xtrabackup --prepare --target-dir=$topdir/full --incremental-dir=$topdir/inc

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/full

start_server

verify_db_state test
