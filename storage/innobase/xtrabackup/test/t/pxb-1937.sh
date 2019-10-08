#
# PXB-1937: xtrabackup --move-back is failing due to encrypted binlog file
#

MYSQLD_EXTRA_MY_CNF_OPTS="
binlog-encryption
log-bin=binlog
"

. inc/keyring_file.sh

start_server

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

start_server
