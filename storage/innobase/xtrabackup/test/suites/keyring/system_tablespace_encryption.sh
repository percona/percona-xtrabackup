#
# Basic test for InnoDB system tablespace encryption (mysql.ibd)
#

require_server_version_higher_than 8.0.15

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

mysql -e "ALTER TABLESPACE mysql ENCRYPTION='y'"

# mysql.ibd contains the data dictionary among others.
# Lets create some tables to alter the data.

for i in {1..10} ; do
    mysql -e "CREATE TABLE t$i (a INT PRIMARY KEY, b TEXT)" test
done

# use transition key

xtrabackup --backup --transition-key=1234 --target-dir=$topdir/backup
xtrabackup --prepare --transition-key=1234 --target-dir=$topdir/backup

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --transition-key=1234 --generate-new-master-key --target-dir=$topdir/backup \
               --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir

start_server

for i in {1..10} ; do
    mysql -e "SELECT * FROM t$i" test
done

rm -rf $topdir/backup

# don't use transition-key

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

stop_server

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir

start_server

for i in {1..10} ; do
    mysql -e "SELECT * FROM t$i" test
done
