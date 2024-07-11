. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="
relay_log_recovery=on
skip-slave-start
"

master_id=1
slave_id=2
slave2_id=3
slave3_id=4
binlog_slave_info_pattern='^CHANGE REPLICATION SOURCE TO SOURCE_LOG_FILE='\''mysql-bin.[0-9]+'\'', SOURCE_LOG_POS=[0-9]+;$'
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

function test_slave_info() {
  backup_dir=$1
  master_id=$2
  slave_id=$3

  switch_server $slave_id
  if ! mysql -e "SHOW REPLICA STATUS\G" | grep -q 'Last_IO_Errno: 0' ; then
    die 'Slave IO error'
  fi
  if ! mysql -e "SHOW REPLICA STATUS\G" | grep -q 'Last_SQL_Errno: 0' ; then
    die 'Slave SQL error'
  fi
  stop_server_with_id $slave_id
  rm -rf $mysql_datadir
  xtrabackup --prepare --target-dir=$backup_dir
  xtrabackup --copy-back --target-dir=$backup_dir
  start_server_with_id $slave_id
  mysql -e "STOP REPLICA"
  cat $backup_dir/xtrabackup_slave_info | sed "s/CHANGE REPLICATION SOURCE TO/CHANGE REPLICATION SOURCE TO SOURCE_HOST='localhost', SOURCE_USER='root', SOURCE_PORT=${SRV_MYSQLD_PORT[$master_id]},/" | mysql
  mysql -e "START REPLICA"
  sync_slave_with_master $master_id $slave_id
  switch_server $slave_id
  if ! mysql -e "SHOW REPLICA STATUS\G" | grep -q 'Last_IO_Errno: 0' ; then
    die 'Slave IO error'
  fi
  if ! mysql -e "SHOW REPLICA STATUS\G" | grep -q 'Last_SQL_Errno: 0' ; then
    die 'Slave SQL error'
  fi
}

# Adding initial rows
multi_row_insert incremental_sample.test \({1..100},100\)

function add_rows_in_background()
{
  MY="${MYSQL} ${MYSQL_ARGS}"
    for i in {101..10000} ; do
      $MY -e "INSERT INTO incremental_sample.test (a, number) VALUES ($i, $i)"
      sleep 1
    done
}
add_rows_in_background &
pid=$!

switch_server $slave_id

mysql -e "SET GLOBAL general_log=1; SET GLOBAL log_output='TABLE';"

vlog "Check that --slave-info with --no-lock and no --safe-slave-backup fails"
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --slave-info --no-lock \
  --target-dir=$topdir/backup

vlog "Full backup of the slave server"
xtrabackup --backup --lock-ddl=false --target-dir=$topdir/backup --slave-info --safe-slave-backup 2>&1 | tee $topdir/pxb.log

grep_general_log > $topdir/log1

if [ $backup_locks = "yes" ] ; then
  diff -u - $topdir/log1 <<EOF
FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS
EOF
else
diff -u $topdir/log1 - <<EOF
FLUSH NO_WRITE_TO_BINLOG TABLES
FLUSH TABLES WITH READ LOCK
FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS
UNLOCK TABLES
EOF
fi

run_cmd egrep -q '^mysql-bin.[0-9]+[[:space:]]+[0-9]+$' \
    $topdir/backup/xtrabackup_binlog_info

run_cmd egrep "MySQL binlog position: $pxb_log_binlog_info_pattern" $topdir/pxb.log

run_cmd egrep "MySQL slave binlog position: $pxb_log_slave_info_pattern" $topdir/pxb.log

# PXB-3033 - Execute STOP SLAVE before copying non-InnoDB tables
grep -A 5 'Slave is safe to backup.' $topdir/pxb.log | grep -q 'Starting to backup non-InnoDB tables and files' || die 'STOP REPLICA in wrong place'

run_cmd egrep -q "$binlog_slave_info_pattern" \
    $topdir/backup/xtrabackup_slave_info

test_slave_info $topdir/backup $master_id $slave_id
mysql -e "SET GLOBAL general_log=1; SET GLOBAL log_output='TABLE';"
mysql -e "TRUNCATE TABLE mysql.general_log;"

mkdir $topdir/xbstream_backup

vlog "Full backup of the slave server to a xbstream stream"
xtrabackup --backup --lock-ddl=false --slave-info --safe-slave-backup \
--stream=xbstream | xbstream -xv -C $topdir/xbstream_backup

cat $topdir/xbstream_backup/xtrabackup_slave_info
vlog "Verifying that xtrabackup_slave_info is not corrupted"
run_cmd egrep -q "$binlog_slave_info_pattern" \
    $topdir/xbstream_backup/xtrabackup_slave_info

grep_general_log > $topdir/log2

if [ $backup_locks = "yes" ] ; then
  diff -u - $topdir/log2 <<EOF
FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS
EOF
else
diff -u $topdir/log2 - <<EOF
FLUSH NO_WRITE_TO_BINLOG TABLES
FLUSH TABLES WITH READ LOCK
FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS
UNLOCK TABLES
EOF
fi

# GTID + auto position

rm -rf $topdir/backup

# Setup a new slave and let it catchup with master data prior to GTID
start_server_with_id $slave2_id
setup_slave $slave2_id $master_id
sync_slave_with_master $slave2_id $master_id
stop_server_with_id $master_id
stop_server_with_id $slave2_id

MYSQLD_EXTRA_MY_CNF_OPTS="
gtid_mode=on
enforce_gtid_consistency=on
relay_log_recovery=on
skip-slave-start
"
start_server_with_id $master_id
start_server_with_id $slave2_id
vlog "Setup Slave GTID"
setup_slave GTID $slave2_id $master_id

mysql -e "SET GLOBAL general_log=1; SET GLOBAL log_output='TABLE';"

vlog "Full backup of the GTID with AUTO_POSITION slave server"
xtrabackup --backup --lock-ddl=false --slave-info --target-dir=$topdir/backup

grep_general_log > $topdir/log3

if [ $backup_locks = "yes" ] ; then
  diff -u - $topdir/log3 <<EOF
FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS
EOF
else
diff -u $topdir/log3 - <<EOF
FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS
EOF
fi

run_cmd egrep -q '^SET GLOBAL gtid_purged=.*;$' \
    $topdir/backup/xtrabackup_slave_info
run_cmd egrep -q '^CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=1;$' \
    $topdir/backup/xtrabackup_slave_info

test_slave_info $topdir/backup $master_id $slave2_id

rm -rf $topdir/backup

# Restarting master in GTID mode, but not setting AUTO_POSITION on slave
# so it uses binlog.
stop_server_with_id $master_id
start_server_with_id $master_id
start_server_with_id $slave3_id
setup_slave $slave3_id $master_id

vlog "Full backup of the GTID slave server"
xtrabackup --backup --lock-ddl=false --slave-info --target-dir=$topdir/backup

run_cmd egrep -q "$binlog_slave_info_pattern" \
    $topdir/backup/xtrabackup_slave_info

if kill -0 ${pid} >/dev/null 2>&1
then
  kill -SIGKILL ${pid} >/dev/null 2>&1 || true
fi
