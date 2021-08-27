# PXB should not try to get group_replication_applier channel

start_group_replication_cluster 2
run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1 (c1 VARCHAR(100) PRIMARY KEY);
INSERT INTO t1 (c1) VALUES ('ONE'), ('TWO'), ('THREE');
INSERT INTO t1 (c1) VALUES ('10'), ('20'), ('30');
CREATE TABLE t2 (c1 VARCHAR(100) PRIMARY KEY);
INSERT INTO t2 SELECT * FROM t1;
EOF

GTID_EXECUTED=$($MYSQL ${MYSQL_ARGS} -NBe "SELECT @@global.GTID_EXECUTED")
switch_server 2
$MYSQL ${MYSQL_ARGS} -e "SELECT WAIT_FOR_EXECUTED_GTID_SET('${GTID_EXECUTED}')"
# This is required as after the backup we will have gtid_purged
MYSQLDUMP_ARGS="--set-gtid-purged=OFF"
record_db_state test
xtrabackup --backup --slave-info --target-dir=${topdir}/../backup
${MYSQLADMIN} ${MYSQL_ARGS} shutdown
rm -rf ${mysql_datadir}
switch_server 1
${MYSQLADMIN} ${MYSQL_ARGS} shutdown
rm -rf ${mysql_datadir}

# Restore the backup on and validate
xtrabackup --prepare --target-dir=${topdir}/../backup
xtrabackup --copy-back --target-dir=${topdir}/../backup --datadir=${mysql_datadir}
switch_server 2
xtrabackup --copy-back --target-dir=${topdir}/../backup --datadir=${mysql_datadir}
shutdown_server_with_id 1
shutdown_server_with_id 2
start_group_replication_cluster 2
switch_server 2
MYSQLDUMP_ARGS="--set-gtid-purged=OFF"
run_cmd verify_db_state test
