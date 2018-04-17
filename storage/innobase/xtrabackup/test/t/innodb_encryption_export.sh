#
# Basic test of InnoDB encryption support
# Exporting and importing encrypted tables
#

require_server_version_higher_than 5.7.10

. inc/keyring_file.sh

keyring_file=${TEST_VAR_ROOT}/keyring_file

start_server --early-plugin-load=keyring_file.so --keyring-file-data=$keyring_file --server-id=10

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

CREATE TABLE t1 (c1 INT) ENCRYPTION='Y';

INSERT INTO t1 (c1) VALUES (1), (2), (3);
INSERT INTO t1 (c1) VALUES (10), (20), (30);

INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

EOF

# wait for InnoDB to flush all dirty pages
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup \
	   --keyring-file-data=$keyring_file --server-id=10

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

INSERT INTO t1 SELECT * FROM t1;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

EOF

xtrabackup --backup --incremental-basedir=$topdir/backup \
	   --target-dir=$topdir/inc1 \
	   --keyring-file-data=$keyring_file --server-id=10

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

INSERT INTO t1 SELECT * FROM t1;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

EOF

xtrabackup --backup --incremental-basedir=$topdir/inc1 \
	   --target-dir=$topdir/inc2 \
	   --keyring-file-data=$keyring_file --server-id=10

${XB_BIN} --prepare --apply-log-only --target-dir=$topdir/backup \
	  --keyring-file-data=$keyring_file \
	  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

${XB_BIN} --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
	  --target-dir=$topdir/backup \
	  --keyring-file-data=$keyring_file \
	  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

${XB_BIN} --prepare --apply-log-only --incremental-dir=$topdir/inc2 \
	  --target-dir=$topdir/backup \
	  --keyring-file-data=$keyring_file \
	  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

${XB_BIN} --prepare --export --target-dir=$topdir/backup \
	  --keyring-file-data=$keyring_file \
	  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

DROP TABLE t1;

EOF

stop_server

start_server --early-plugin-load=keyring_file.so --keyring-file-data=$keyring_file --server-id=20

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

CREATE TABLE t1 (c1 INT) ENCRYPTION='Y';

EOF

function discard_import() {

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

ALTER TABLE t1 DISCARD TABLESPACE;

SELECT SLEEP(5);

ALTER INSTANCE ROTATE INNODB MASTER KEY;

ALTER TABLE t1 IMPORT TABLESPACE;

EOF

}

discard_import &
mysql_pid=$!

while [ -f $mysql_datadir/test/t1.ibd ]
do
	sleep 1
done

cp -av $topdir/backup/test/t1.{cfg,cfp,ibd} $mysql_datadir/test

wait $mysql_pid

run_cmd $MYSQL $MYSQL_ARGS -e "SELECT SUM(c1) FROM t1" test

