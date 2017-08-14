########################################################################
# Test for consistent mysql.gtid_executed on restore
########################################################################

. inc/common.sh

require_server_version_higher_than 5.7.4

MYSQLD_EXTRA_MY_CNF_OPTS="
gtid_mode=on
log_slave_updates=on
enforce_gtid_consistency=on
"
start_server

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, c INT);
INSERT INTO t(c) VALUES (1),(2),(3),(4),(5),(6),(7),(8);
EOF

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

master_gtid_executed=$(run_cmd $MYSQL $MYSQL_ARGS -Nse 'SELECT @@global.gtid_executed' mysql)
stop_server

rm -fr $mysql_datadir/*

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

slave_gtid_executed=$(run_cmd $MYSQL $MYSQL_ARGS -Nse 'SELECT @@global.gtid_executed' mysql)

if [ $master_gtid_executed != $slave_gtid_executed ]
then
	vlog "GTIDs do not match! ($master_gtid_executed != $slave_gtid_executed)"
	exit -1
fi

vlog "GTIDs are OK ($master_gtid_executed == $slave_gtid_executed)"
