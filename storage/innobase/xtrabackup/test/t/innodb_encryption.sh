#
# Basic test of InnoDB encryption support
#

require_server_version_higher_than 5.7.10

function test_do()
{
	encryption_clause=$1
	compression_clause=$2
	transition_key=$3

	if [[ "$transition_key" = "generate" ]] ; then
		backup_options="--generate-transition-key"
		prepare_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
		copyback_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
	elif [[ "$transition_key" = "none" ]] ; then
		backup_options=
		prepare_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
		copyback_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
	else
		backup_options="--transition-key=$transition_key"
		prepare_options="--transition-key=$transition_key"
		copyback_options="--transition-key=$transition_key"
	fi

	start_server

	run_cmd $MYSQL $MYSQL_ARGS test -e "SELECT @@server_uuid"

	# PXB-1540: XB removes and recreate keyring file of 0 size
	xtrabackup --backup --target-dir=$topdir/backup0

	rm -rf $topdir/backup0

	run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1 (c1 VARCHAR(100)) ${encryption_clause} ${compression_clause};
INSERT INTO t1 (c1) VALUES ('ONE'), ('TWO'), ('THREE');
INSERT INTO t1 (c1) VALUES ('10'), ('20'), ('30');
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
CREATE TABLE t2 (c1 VARCHAR(100)) ${encryption_clause} ${compression_clause};
INSERT INTO t2 SELECT * FROM t1;
EOF

	# wait for InnoDB to flush all dirty pages
	innodb_wait_for_flush_all

	xtrabackup --backup --target-dir=$topdir/backup $backup_options

	cat $topdir/backup/backup-my.cnf

	run_cmd $MYSQL $MYSQL_ARGS test <<EOF
INSERT INTO t1 SELECT * FROM t1;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
INSERT INTO t1 SELECT * FROM t1;
CREATE TABLE t3 (c1 VARCHAR(100)) ${encryption_clause} ${compression_clause};
INSERT INTO t3 SELECT * FROM t1;
DROP TABLE t2;
EOF

	# wait for InnoDB to flush all dirty pages
	innodb_wait_for_flush_all

	xtrabackup --backup --incremental-basedir=$topdir/backup \
		   --target-dir=$topdir/inc1 $backup_options

	run_cmd $MYSQL $MYSQL_ARGS test <<EOF
INSERT INTO t1 SELECT * FROM t1;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
INSERT INTO t1 SELECT * FROM t1;
EOF

	xtrabackup --backup --incremental-basedir=$topdir/inc1 \
		   --target-dir=$topdir/inc2 $backup_options


	xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup \
		   $prepare_options
	xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
		   --target-dir=$topdir/backup $prepare_options

	xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc2 \
		   --target-dir=$topdir/backup $prepare_options

	xtrabackup --prepare --export --target-dir=$topdir/backup \
		   $prepare_options

	# check that stats works
	xtrabackup --stats --datadir=$topdir/backup --plugin-load=${plugin_load}

	# make sure t1.ibd is still encrypted
	strings $topdir/backup/test/t1.ibd | ( grep -vq TWO || die "t1 is not encrypted" )

	record_db_state test

	# copy-back with existing master key
	stop_server

	rm -rf $mysql_datadir

	xtrabackup --copy-back --target-dir=$topdir/backup

	start_server

	verify_db_state test

	if [[ "$transition_key" != "none" ]]; then

		# copy-back with new master key
		stop_server

		rm -rf $mysql_datadir

		test "$transition_key" = "generate" || cleanup_keyring

		xtrabackup --copy-back --generate-new-master-key \
			   $copyback_options \
			   --target-dir=$topdir/backup

		start_server

		verify_db_state test
	fi

	stop_server

	rm -rf $mysql_datadir
	rm -rf $topdir/{backup,inc1,inc2}

}

# run with keyring_file plugin first

. inc/keyring_file.sh

function cleanup_keyring() {
	rm -rf $keyring_file
}

test_do "ENCRYPTION='y'" "" "top-secret"
test_do "ENCRYPTION='y'" "ROW_FORMAT=COMPRESSED" "none"
test_do "ENCRYPTION='y'" "ROW_FORMAT=COMPRESSED" "top-secret"
test_do "ENCRYPTION='y'" "COMPRESSION='zlib'" "top-secret"

# cleanup environment variables
MYSQLD_EXTRA_MY_CNF_OPTS=
XB_EXTRA_MY_CNF_OPTS=

# and rerun with keyring_vault

. inc/keyring_vault.sh

keyring_vault_ping || skip_test "Keyring vault server is not avaliable"

keyring_vault_mount

function cleanup_keyring() {
	keyring_vault_remove_all_keys
}

trap "keyring_vault_unmount" EXIT

test_do "ENCRYPTION='y'" "" "top-secret"
test_do "ENCRYPTION='y'" "ROW_FORMAT=COMPRESSED" "none"
test_do "ENCRYPTION='y'" "ROW_FORMAT=COMPRESSED" "top-secret"
test_do "ENCRYPTION='y'" "COMPRESSION='zlib'" "generate"

keyring_vault_unmount
trap "" EXIT

# cleanup environment variables
MYSQLD_EXTRA_MY_CNF_OPTS=
XB_EXTRA_MY_CNF_OPTS=
