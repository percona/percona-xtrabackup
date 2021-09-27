########################################################################
# binlog-info tests
########################################################################

require_server_version_higher_than 8.0.16

function test_binlog_info() {

    start_server $@

    is_gtid_mode && gtid=1 || gtid=0

    # Generate some InnoDB entries in the binary log
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
    CREATE TABLE p (a INT) ENGINE=InnoDB;
    INSERT INTO p VALUES (1), (2), (3);
EOF

    # Generate some non-InnoDB entries in the binary log
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
    CREATE TABLE t (a INT) ENGINE=MyISAM;
    INSERT INTO t VALUES (1), (2), (3);
EOF

    if [ $gtid = 1 ] ; then
        gtid_executed=`get_gtid_executed`
    fi

    xb_binlog_info=$topdir/backup/xtrabackup_binlog_info

    xtrabackup --backup --target-dir=$topdir/backup

    binlog_file=`get_binlog_file`
    binlog_pos=`get_binlog_pos`

    verify_binlog_info_on

    rm -rf $topdir/backup

    stop_server

    rm -rf $mysql_datadir

}

function normalize_path()
{
    sed -i -e 's|^\./||' $1
}

function verify_binlog_info_on()
{
    normalize_path $xb_binlog_info

    if [ $gtid = 1 ]
    then
		run_cmd diff -u $xb_binlog_info - <<EOF
$binlog_file	$binlog_pos	$gtid_executed
EOF
	else
		run_cmd diff -u $xb_binlog_info - <<EOF
$binlog_file	$binlog_pos
EOF
	fi

}

test_binlog_info

test_binlog_info --gtid-mode=ON --enforce-gtid-consistency=ON \
                 --log-bin --log-slave-updates
