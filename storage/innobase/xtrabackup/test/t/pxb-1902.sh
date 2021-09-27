#
# PXB-1902: PXB is unable to connect to the database with transition-key when password is specified
#

start_server

mysql -e "CREATE USER 'bkpuser'@'localhost' IDENTIFIED BY '111'"
mysql -e "GRANT BACKUP_ADMIN, RELOAD, LOCK TABLES, PROCESS, REPLICATION CLIENT ON *.* TO 'bkpuser'@'localhost';"
mysql -e "GRANT SELECT ON performance_schema.log_status TO 'bkpuser'@'localhost'"
if is_server_version_higher_than 8.0.23
then
  mysql -e "GRANT SELECT ON performance_schema.keyring_component_status TO bkpuser@'localhost'"
fi
mysql -e "GRANT ALL ON PERCONA_SCHEMA.* TO 'bkpuser'@'localhost';"

xtrabackup -ubkpuser -p111 --backup --target-dir=$topdir/backup \
	   --transition-key=abcd
