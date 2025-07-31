#
# PXB-3571 : --transition-key does not save keys with --lock-ddl=reduced
#


KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

mysql -e "CREATE TABLE t (a INT) ENCRYPTION='y'" test

shutdown_server
start_server

for i in {1..100} ; do
    mysql -e "INSERT INTO t VALUES ($i)" test
done

BACKUP_DIR=$topdir/backup
xtrabackup --backup --transition-key=123 --target-dir=$BACKUP_DIR --lock-ddl=reduced
record_db_state test
xtrabackup --prepare --transition-key=123 --target-dir=$BACKUP_DIR --lock-ddl=reduced --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$BACKUP_DIR --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir
start_server
verify_db_state test
