#########################################################################
# Bug #1239670: xtrabackup_slave_info does not contain GTID purge lists
#########################################################################

require_server_version_higher_than 5.6.0

MYSQLD_EXTRA_MY_CNF_OPTS="
gtid_mode=on
log_slave_updates=on
enforce_gtid_consistency=on
"
master_id=1
slave_id=2

start_server_with_id $master_id
start_server_with_id $slave_id

setup_slave GTID $slave_id $master_id

switch_server $master_id

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, c INT);
INSERT INTO t(c) VALUES (1),(2),(3),(4),(5),(6),(7),(8);
SET AUTOCOMMIT=0;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
COMMIT;
EOF

sync_slave_with_master $slave_id $master_id

switch_server $slave_id

$MYSQL $MYSQL_ARGS -e "SHOW REPLICA STATUS\G"

xtrabackup --backup --slave-info --target-dir=$topdir/backup

vlog "Verifying format of xtrabackup_slave_info:"
f=$topdir/backup/xtrabackup_slave_info
echo "---"
cat $f
echo "---"
run_cmd egrep -q '^SET GLOBAL gtid_purged='\''[0-9a-f:, -]+'\' $f
run_cmd egrep -q '^CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=1' $f
