#
# Compare files in the datadir and the backup dir
#

start_server

load_sakila

mysql -e "CREATE TABLE t1 (a INT) ENGINE=MyISAM" test
mysql -e "CREATE TABLE t2 (a INT) ENGINE=InnoDB" test

function compare_files() {

dir1=$1
dir2=$2

# files that present in the backup directory, but not present in the datadir
diff -u <( ( ( cd $dir1; find . | grep -v innodb_temp )
             ( cd $dir2; find . | grep -v innodb_temp )
             ( cd $dir2; find . | grep -v innodb_temp ) ) | sort | uniq -u ) - <<EOF
./backup-my.cnf
./xtrabackup_binlog_info
./xtrabackup_binlog_pos_innodb
./xtrabackup_checkpoints
./xtrabackup_info
./xtrabackup_logfile
./xtrabackup_master_key_id
./xtrabackup_tablespaces
EOF

# files that present in the datadir, but not present in the backup
diff -u <( ( ( cd $dir1; find . | grep -v innodb_temp )
             ( cd $dir1; find . | grep -v innodb_temp )
             ( cd $dir2; find . | grep -v innodb_temp ) ) | sort | uniq -u ) - <<EOF
./auto.cnf
./ca-key.pem
./ca.pem
./client-cert.pem
./client-key.pem
./mysql-bin.000001
./mysql-bin.000002
./mysqld1.err
./private_key.pem
./public_key.pem
./server-cert.pem
./server-key.pem
EOF

}

function compare_files_inc() {

dir1=$1
dir2=$2

# files that present in the backup directory, but not present in the datadir
diff -u <( ( ( cd $dir1; find . | grep -v innodb_temp )
             ( cd $dir2; find . | grep -v innodb_temp )
             ( cd $dir2; find . | grep -v innodb_temp ) ) | sort | uniq -u ) - <<EOF
./backup-my.cnf
./xtrabackup_binlog_info
./xtrabackup_binlog_pos_innodb
./xtrabackup_checkpoints
./xtrabackup_info
./xtrabackup_logfile
./xtrabackup_master_key_id
./xtrabackup_tablespaces
EOF

# files that present in the datadir, but not present in the backup
diff -u <( ( ( cd $dir1; find . | grep -v innodb_temp )
             ( cd $dir1; find . | grep -v innodb_temp )
             ( cd $dir2; find . | grep -v innodb_temp ) ) | sort | uniq -u ) - <<EOF
./auto.cnf
./ca-key.pem
./ca.pem
./client-cert.pem
./client-key.pem
./mysql-bin.000001
./mysql-bin.000002
./mysql-bin.000003
./mysqld1.err
./private_key.pem
./public_key.pem
./server-cert.pem
./server-key.pem
EOF

}

xtrabackup --backup --target-dir=$topdir/backup
cp -a $topdir/backup $topdir/full
xtrabackup --prepare --target-dir=$topdir/backup
compare_files $topdir/backup $mysql_datadir

# PXB-1696: Incremental prepare removes .sdi files from the data dir
mysql -e "CREATE TABLE t3 (a INT) ENGINE=MyISAM" test
mysql -e "CREATE TABLE t4 (a INT) ENGINE=InnoDB" test

xtrabackup --backup --target-dir=$topdir/inc --incremental-basedir=$topdir/full
xtrabackup --prepare --target-dir=$topdir/full --apply-log-only
xtrabackup --prepare --target-dir=$topdir/full --incremental-dir=$topdir/inc
compare_files_inc $topdir/full $mysql_datadir
