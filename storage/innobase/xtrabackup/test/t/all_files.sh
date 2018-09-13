#
# Compare files in the datadir and the backup dir
#

start_server

load_sakila

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

# files that present in the backup directory, but not present in the datadir
diff -u <( ( ( cd $topdir/backup; find . )
             ( cd $mysql_datadir; find . )
             ( cd $mysql_datadir; find . ) ) | sort | uniq -u ) - <<EOF
./backup-my.cnf
./test/db.opt
./xtrabackup_binlog_info
./xtrabackup_binlog_pos_innodb
./xtrabackup_checkpoints
./xtrabackup_info
./xtrabackup_logfile
./xtrabackup_master_key_id
./xtrabackup_tablespaces
EOF

# files that present in the datadir, but not present in the backup
diff -u <( ( ( cd $topdir/backup; find . )
             ( cd $topdir/backup; find . )
             ( cd $mysql_datadir; find . ) ) | sort | uniq -u ) - <<EOF
./auto.cnf
./ca-key.pem
./ca.pem
./client-cert.pem
./client-key.pem
./mysql-bin.000001
./mysql-bin.000002
./mysql-bin.index
./mysqld1.err
./private_key.pem
./public_key.pem
./server-cert.pem
./server-key.pem
EOF
