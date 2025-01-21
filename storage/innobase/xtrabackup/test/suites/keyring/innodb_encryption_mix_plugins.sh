#
# Backup server using keyring_file and restore using keyring_vault
#

require_server_version_higher_than 5.7.10

is_xtradb || skip_test "Keyring vault requires Percona Server"

vlog setup keyring_file
KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1 (c1 VARCHAR(100)) ENCRYPTION='y';
INSERT INTO t1 (c1) VALUES ('ONE'), ('TWO'), ('THREE');
INSERT INTO t1 (c1) VALUES ('10'), ('20'), ('30');
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
EOF

record_db_state test

# wait for InnoDB to flush all dirty pages
innodb_wait_for_flush_all

vlog backup
xtrabackup --backup --target-dir=$topdir/backup1 --transition-key=key1

vlog cleanup environment variables
MYSQLD_EXTRA_MY_CNF_OPTS=
XB_EXTRA_MY_CNF_OPTS=

vlog setup keyring_vault
. inc/keyring_vault.sh

cat > ${topdir}/my-copy-back.cnf <<EOF
[mysqld]
datadir=${MYSQLD_DATADIR}
${MYSQLD_EXTRA_MY_CNF_OPTS:-}

[xtrabackup]
${XB_EXTRA_MY_CNF_OPTS:-}
EOF

vlog remove datadir
stop_server
VAULT_MOUNT_VERSION=2
VAULT_CONFIG_VERSION=2
configure_keyring_file_component
rm -rf $mysql_datadir

vlog prepare
xtrabackup --prepare --target-dir=$topdir/backup1 --transition-key=key1

keyring_vault_ping || skip_test "Keyring vault server is not avaliable"


trap "keyring_vault_unmount" EXIT

vlog copy-back
run_cmd ${XB_BIN} --defaults-file=${topdir}/my-copy-back.cnf --copy-back \
	--target-dir=$topdir/backup1 --generate-new-master-key \
	 --transition-key=key1 \
	--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir
start_server

verify_db_state test
