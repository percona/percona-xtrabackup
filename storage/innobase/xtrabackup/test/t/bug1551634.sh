########################################################################
# Bug #1551634: xtrabackup --slave-info does not handle multi-master replication
########################################################################
# Plan:
#  Set up 2 masters and 1 slave.
#  Insert some data on both masters
#  Ensure that slave1 has the data
#  Backup the slave1
#  Verify that xtrabackup_slave_info contains info about 2 channels.

. inc/common.sh

require_server_version_higher_than 5.7.6

function init_db()
{
  local db_name=$1
  local table_name=$2

mysql <<EOF
CREATE DATABASE IF NOT EXISTS $db_name;
USE $db_name;
CREATE TABLE IF NOT EXISTS $table_name (
  c int(11) NOT NULL PRIMARY KEY
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
EOF
}

readonly master1_id=1
readonly master2_id=2
readonly slave_id=3

function run_test()
{
    local slave_mode=$1

    vlog "############################################################"
    vlog "$slave_mode mode"

    if [ $slave_mode = "GTID" ]
    then
        local extra_arguments="--gtid_mode=ON --enforce-gtid-consistency=ON"
    else
        local extra_arguments=
        slave_mode=
    fi

    vlog "############################################################"
    vlog "Setting up masters and slave"
    start_server_with_id $master1_id $extra_arguments
    init_db test m1
    multi_row_insert test.m1 \({1..7}\)

    start_server_with_id $master2_id $extra_arguments
    init_db test m2
    # number of rows differ from one for master1 to get different MASTER_LOG_POS in xtrabackup_slave_info
    multi_row_insert test.m2 \({1..13}\)

    start_server_with_id $slave_id \
        --master-info-repository=TABLE \
        --relay-log-info-repository=TABLE \
        "$extra_arguments"

    setup_slave $slave_mode USE_CHANNELS $slave_id $master1_id $master2_id

    test `mysql -N -se "SHOW SLAVE STATUS" | wc -l` -eq 2

    vlog "############################################################"
    vlog "Backing up slave"
    switch_server $slave_id

    xtrabackup --backup \
        --no-timestamp \
        --slave-info \
        --safe-slave-backup \
        --target_dir $topdir/backup

    cat $topdir/backup/xtrabackup_slave_info
    vlog "Verifying that there are 2 different channels in xtrabackup_slave_info"
    test `egrep -c '^CHANGE MASTER TO.*FOR CHANNEL '\''master-[0-9]+'\' \
        $topdir/backup/xtrabackup_slave_info` -eq 2 || die

    for i in ${SRV_MYSQLD_IDS[@]}
    do
        stop_server_with_id "$i"
    done

    remove_var_dirs
}

run_test BINLOG
run_test GTID
