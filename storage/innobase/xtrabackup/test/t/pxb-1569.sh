#
# PXB-1569: --history does not work when autocommit is disabled
#

start_server

mysql -e "CREATE USER 'bkpuser'@'localhost' IDENTIFIED BY '111'"
mysql -e "GRANT RELOAD, LOCK TABLES, PROCESS, REPLICATION CLIENT ON *.* TO 'bkpuser'@'localhost';"
mysql -e "GRANT ALL ON PERCONA_SCHEMA.* TO 'bkpuser'@'localhost';"
mysql -e "SET GLOBAL init_connect='set autocommit=0'"

xtrabackup -ubkpuser -p111 --backup --history=abcx --target-dir=$topdir/backup

N=`mysql -N -e "SELECT COUNT(*) FROM PERCONA_SCHEMA.xtrabackup_history"`

test $N -eq 1 || die "Wrong rows number in xtrabackup_history: $N"
