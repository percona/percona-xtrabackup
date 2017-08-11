require_server_version_higher_than 5.6.3

master_id=1
slave_id=3
restored_slave_id=4
backup_dir=$TEST_VAR_ROOT

function init_database()
{
    local db_name="$1"

    switch_server $master_id
    mysql <<EOF
create database ${db_name};
use ${db_name};
create table test (a INT PRIMARY KEY);

delimiter //

create procedure purge_test (rounds int)
begin
    while rounds > 0 do
        start transaction;
        delete from test limit 1;
        commit;
        set rounds = rounds - 1;
    end while;
end//

delimiter ;
EOF
  multi_row_insert $db_name.test \({1..10000}\)
}

function delete_rows()
{
    local db_name="$1"
    local rounds=$2

    switch_server $master_id
    mysql $db_name -e  "call purge_test($rounds)";
}

function start_slave_with_id()
{
    local slave_id_arg=$1
    shift 1
    start_server_with_id $slave_id_arg \
        --binlog-format=ROW \
        --general-log=1 \
        --general-log-file=slave.log \
        --slave-checkpoint-group=100 \
        --slave-checkpoint-period=100 \
        --relay_log_info_repository=TABLE \
        $*
}

start_server_with_id $master_id \
    --binlog-format=ROW \
    --relay_log_info_repository=table

start_slave_with_id $slave_id \
    --slave-parallel-workers=2

setup_slave $slave_id $master_id

init_database m1
init_database m2

run_cmd delete_rows m1 5000 &
run_cmd delete_rows m2 1000 &


switch_server $slave_id
mysql -e "LOCK TABLES m2.test WRITE; SELECT SLEEP(10);" &
mysql -e "select * from mysql.slave_worker_info\G" > $backup_dir/slave_worker_info1.log
mysql -e "SHOW SLAVE STATUS\G" 2>&1 > $backup_dir/slave_info1.log

xtrabackup --backup --target-dir=$backup_dir/safe_backup --slave_info \
    --safe-slave-backup --safe-slave-backup-timeout=200

mysql -e "SHOW SLAVE STATUS\G" 2>&1 > $backup_dir/slave_info2.log
mysql -e "select * from mysql.slave_worker_info\G" > $backup_dir/slave_worker_info2.log
slave_binlog_pos=$(cat $backup_dir/safe_backup/xtrabackup_slave_info | sed 's/CHANGE MASTER TO//')
vlog "Slave binglog pos: $slave_binlog_pos"

xtrabackup --prepare --target-dir=$backup_dir/safe_backup

start_slave_with_id $restored_slave_id \
    --slave-parallel-workers=2

stop_server_with_id $restored_slave_id
rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$backup_dir/safe_backup

start_slave_with_id $restored_slave_id \
        --slave-parallel-workers=2
mysql -e "select * from mysql.slave_worker_info\G" > $backup_dir/slave_worker_info3a.log
mysql -e "SHOW SLAVE STATUS\G" 2>&1 > $backup_dir/slave_info3a.log

setup_slave $restored_slave_id $master_id
mysql -e "select * from mysql.slave_worker_info\G" > $backup_dir/slave_worker_info4.log
mysql -e "SHOW SLAVE STATUS\G" 2>&1 > $backup_dir/slave_info4.log

switch_server $slave_id
mysql -e "select * from mysql.slave_worker_info\G" > $backup_dir/slave_worker_info5.log
mysql -e "SHOW SLAVE STATUS\G" 2>&1 > $backup_dir/slave_info5.log

