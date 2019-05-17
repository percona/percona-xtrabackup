############################################################################
# Test external XA prepare transactions in backup result are not rolled back
# after --prepare
############################################################################

. inc/common.sh

vlog "start server with gtid enabled"
start_server --gtid-mode=ON --enforce-gtid-consistency=ON --log-bin --log-slave-updates

mysql test -e "create table t1(id int, primary key(id)) engine=innodb"
mysql test -e "xa start 'test1'; insert t1 values (1); xa end 'test1'; xa prepare 'test1'"
mysql test -e "xa start 'test2'; insert t1 values (2); update t1 set id = 3 where id = 2; xa end 'test2'; xa prepare 'test2'"

mysql test -e "xa recover" | grep "test1"
mysql test -e "xa recover" | grep "test2"

vlog "Make full backup"
mkdir $topdir/backup
xtrabackup --backup --target-dir=$topdir/backup

vlog "Prepare backup"
xtrabackup --prepare --target-dir=$topdir/backup

vlog "Shutdown server and cleanup data"
stop_server
rm -rf $mysql_datadir/*

vlog "Copying files to their original locations"
xtrabackup --copy-back --target-dir=$topdir/backup

vlog "Start up server"
start_server --gtid-mode=ON --enforce-gtid-consistency=ON --log-bin --log-slave-updates

vlog "Check external XA preapred transactions still exist"
mysql test -e "xa recover" | grep "test1"
mysql test -e "xa recover" | grep "test2"

if ! grep -q "will be kept in prepared state" $OUTFILE
then
    die "Not found keep prepared message!"
fi
