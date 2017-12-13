start_server

vlog "Creating encrypted general tablespace"

mysql -e "CREATE TABLESPACE ts3 ADD DATAFILE 'ts3.ibd' ENCRYPTION='Y'"

vlog "Full backup"

xtrabackup --backup \
    --target-dir=$topdir/data/full

