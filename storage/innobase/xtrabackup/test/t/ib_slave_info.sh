. inc/common.sh

master_id=1
slave_id=2
slave2_id=3
slave3_id=4
binlog_slave_info_pattern='^CHANGE MASTER TO MASTER_LOG_FILE='\''mysql-bin.[0-9]+'\'', MASTER_LOG_POS=[0-9]+;$'
pxb_log_binlog_info_pattern='filename '\''mysql-bin.[0-9]+'\'', position '\''[0-9]+'\'''
pxb_log_slave_info_pattern='master host '\''[a-zA-Z0-9\.-]+'\'', filename '\''mysql-bin.[0-9]+'\'', position '\''[0-9]+'\'', channel name: '\''[a-zA-Z0-9\.-]*'\'''

start_server_with_id $master_id
start_server_with_id $slave_id

setup_slave $slave_id $master_id

switch_server $master_id
load_dbase_schema incremental_sample

if has_backup_locks ; then
    backup_locks="yes"
else
    backup_locks="no"
fi

# Adding initial rows
multi_row_insert incremental_sample.test \({1..100},100\)

switch_server $slave_id

vlog "Check that --slave-info with --no-lock and no --safe-slave-backup fails"
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --slave-info --no-lock \
  --target-dir=$topdir/backup

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_lock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_unlock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status1

if [ $backup_locks = "yes" ] ; then
	diff -u - $topdir/status1 <<EOF
Com_lock_instance	0
Com_lock_tables	0
Com_lock_tables_for_backup	0
Com_unlock_instance	0
Com_unlock_tables	0
Com_flush	1
EOF
else
diff -u $topdir/status1 - <<EOF
Com_lock_instance	0
Com_lock_tables	0
Com_unlock_instance	0
Com_unlock_tables	0
Com_flush	1
EOF
fi

vlog "Full backup of the slave server"
xtrabackup --backup --target-dir=$topdir/backup \
    --no-timestamp --slave-info \
    2>&1 | tee $topdir/pxb.log

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_lock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_unlock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status2

if [ $backup_locks = "yes" ] ; then
	diff -u - $topdir/status2 <<EOF
Com_lock_instance	0
Com_lock_tables	0
Com_lock_tables_for_backup	0
Com_unlock_instance	0
Com_unlock_tables	0
Com_flush	2
EOF
else
diff -u $topdir/status2 - <<EOF
Com_lock_instance	0
Com_lock_tables	0
Com_unlock_instance	0
Com_unlock_tables	1
Com_flush	4
EOF
fi

run_cmd egrep -q '^mysql-bin.[0-9]+[[:space:]]+[0-9]+$' \
    $topdir/backup/xtrabackup_binlog_info

run_cmd egrep "MySQL binlog position: $pxb_log_binlog_info_pattern" $topdir/pxb.log

run_cmd egrep "MySQL slave binlog position: $pxb_log_slave_info_pattern" $topdir/pxb.log

run_cmd egrep -q "$binlog_slave_info_pattern" \
    $topdir/backup/xtrabackup_slave_info

mkdir $topdir/xbstream_backup

vlog "Full backup of the slave server to a xbstream stream"
xtrabackup --backup --no-timestamp --slave-info --stream=xbstream \
    | xbstream -xv -C $topdir/xbstream_backup

vlog "Verifying that xtrabackup_slave_info is not corrupted"
run_cmd egrep -q "$binlog_slave_info_pattern" \
    $topdir/xbstream_backup/xtrabackup_slave_info

# GTID + auto position

rm -rf $topdir/backup

MYSQLD_EXTRA_MY_CNF_OPTS="
gtid_mode=on
enforce_gtid_consistency=on
"

start_server_with_id $slave2_id
setup_slave GTID $slave2_id $master_id

vlog "Full backup of the GTID with AUTO_POSITION slave server"
xtrabackup --backup --no-timestamp --slave-info --target-dir=$topdir/backup

$MYSQL $MYSQL_ARGS -Ns -e \
       "SHOW GLOBAL STATUS LIKE 'Com_lock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_unlock%'; \
       SHOW GLOBAL STATUS LIKE 'Com_flush%'" \
       > $topdir/status3

if [ $backup_locks = "yes" ] ; then
	diff -u - $topdir/status3 <<EOF
Com_lock_instance	0
Com_lock_tables	0
Com_lock_tables_for_backup	0
Com_unlock_instance	0
Com_unlock_tables	0
Com_flush	2
EOF
else
	diff -u - $topdir/status3 <<EOF
Com_lock_instance	0
Com_lock_tables	0
Com_unlock_instance	0
Com_unlock_tables	0
Com_flush	2
EOF
fi

run_cmd egrep -q '^SET GLOBAL gtid_purged=.*;$' \
    $topdir/backup/xtrabackup_slave_info
run_cmd egrep -q '^CHANGE MASTER TO MASTER_AUTO_POSITION=1;$' \
    $topdir/backup/xtrabackup_slave_info

rm -rf $topdir/backup

# Restarting master in GTID mode, but not setting AUTO_POSITION on slave
# so it uses binlog.
stop_server_with_id $master_id
start_server_with_id $master_id
start_server_with_id $slave3_id
setup_slave $slave3_id $master_id

vlog "Full backup of the GTID slave server"
xtrabackup --backup --no-timestamp --slave-info --target-dir=$topdir/backup

run_cmd egrep -q "$binlog_slave_info_pattern" \
    $topdir/backup/xtrabackup_slave_info
