#
# PXB-1937: xtrabackup --move-back is failing due to encrypted binlog file
#

MYSQLD_EXTRA_MY_CNF_OPTS="
binlog-encryption
log-bin=binlog
"

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component


xtrabackup --backup --target-dir=$topdir/backup --transition-key=123

xtrabackup --prepare --target-dir=$topdir/backup --transition-key=123

stop_server

rm -rf $mysql_datadir

xtrabackup --no-defaults --datadir=$mysql_datadir \
           --move-back --target-dir=$topdir/backup \
           --generate-new-master-key \
           --transition-key=123 \
           --xtrabackup-plugin-dir=${plugin_dir} \
           ${keyring_args}

cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir

start_server
