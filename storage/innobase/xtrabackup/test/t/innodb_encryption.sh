#
# Basic test of InnoDB encryption support
#

require_server_version_higher_than 5.7.10

keyring_file=${TEST_VAR_ROOT}/keyring_file

start_server --early-plugin-load=keyring_file.so --keyring-file-data=$keyring_file --server_id=10

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

SELECT @@server_id;

CREATE TABLE t1 (c1 VARCHAR(100)) ENCRYPTION='Y';

INSERT INTO t1 (c1) VALUES ('ONE'), ('TWO'), ('THREE');
INSERT INTO t1 (c1) VALUES ('10'), ('20'), ('30');

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
	   --keyring-file-data=$keyring_file --server_id=10

cat $topdir/backup/backup-my.cnf

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

INSERT INTO t1 SELECT * FROM t1;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

EOF

xtrabackup --backup --incremental-basedir=$topdir/backup \
           --target-dir=$topdir/inc1 \
           --keyring-file-data=$keyring_file --server_id=10

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

INSERT INTO t1 SELECT * FROM t1;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

EOF

xtrabackup --backup --incremental-basedir=$topdir/inc1 \
	   --target-dir=$topdir/inc2 \
	   --keyring-file-data=$keyring_file --server_id=10

${XB_BIN} --prepare --apply-log-only --target-dir=$topdir/backup \
	  --keyring-file-data=$keyring_file
${XB_BIN} --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
	  --target-dir=$topdir/backup \
	  --keyring-file-data=$keyring_file
${XB_BIN} --prepare --apply-log-only --incremental-dir=$topdir/inc2 \
	  --target-dir=$topdir/backup \
	  --keyring-file-data=$keyring_file
${XB_BIN} --prepare --target-dir=$topdir/backup \
	  --keyring-file-data=$keyring_file

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server --early-plugin-load=keyring_file.so --keyring_file_data=$keyring_file --server_id=10

run_cmd $MYSQL $MYSQL_ARGS -e "SELECT @@server_id" test
run_cmd $MYSQL $MYSQL_ARGS -e "SELECT @@keyring_file_data" test
run_cmd $MYSQL $MYSQL_ARGS -e "SELECT SUM(c1) FROM t1" test

