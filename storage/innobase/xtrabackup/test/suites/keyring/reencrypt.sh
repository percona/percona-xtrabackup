#
# Test for reencrypt feature
#

require_server_version_higher_than 5.7.10

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component


run_cmd $MYSQL $MYSQL_ARGS test <<EOF

SELECT @@server_id;
SELECT @@server_uuid;

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

cat $mysql_datadir/auto.cnf

xtrabackup --backup --target-dir=$topdir/backup ${keyring_args}

cat $topdir/backup/backup-my.cnf

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

INSERT INTO t1 SELECT * FROM t1;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

EOF

xtrabackup --backup --incremental-basedir=$topdir/backup \
           --target-dir=$topdir/inc1 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

INSERT INTO t1 SELECT * FROM t1;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

EOF

xtrabackup --backup --incremental-basedir=$topdir/inc1 \
	   --target-dir=$topdir/inc2 \
	   --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

${XB_BIN} --prepare --apply-log-only --target-dir=$topdir/backup \
	  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

${XB_BIN} --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
	  --target-dir=$topdir/backup \
	  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

${XB_BIN} --prepare --apply-log-only --incremental-dir=$topdir/inc2 \
	  --target-dir=$topdir/backup \
	  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

${XB_BIN} --prepare --target-dir=$topdir/backup \
	  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir

cat > $mysql_datadir/auto.cnf <<EOF
[auto]
server_uuid=8a94f357-aab4-11df-86ab-c80aa9429562
EOF

start_server --server_id=200

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
SELECT @@server_id;
SELECT @@server_uuid;
SELECT SUM(c1) FROM t1;
EOF
