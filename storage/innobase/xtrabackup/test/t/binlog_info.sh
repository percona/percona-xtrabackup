########################################################################
# --binlog-info tests
########################################################################

xtrabackup --help 2>&1 | grep 'binlog-info *auto'

function test_binlog_info() {

    start_server $@

    has_backup_locks && bl_avail=1 || bl_avail=0
    has_backup_safe_binlog_info && bsbi_avail=1 || bsbi_avail=0
    is_gtid_mode && gtid=1 || gtid=0

    # Generate some InnoDB entries in the binary log
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
    CREATE TABLE p (a INT) ENGINE=InnoDB;
    INSERT INTO p VALUES (1), (2), (3);
EOF

    binlog_file_innodb=`get_binlog_file`
    binlog_pos_innodb=`get_binlog_pos`
    binlog_info_innodb="$binlog_file_innodb	$binlog_pos_innodb"

    # Generate some non-InnoDB entries in the binary log
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
    CREATE TABLE t (a INT) ENGINE=MyISAM;
    INSERT INTO t VALUES (1), (2), (3);
EOF

    binlog_file=`get_binlog_file`
    binlog_pos=`get_binlog_pos`

    if [ $gtid = 1 ] ; then
        gtid_executed=`get_gtid_executed`
    fi

    xb_binlog_info=$topdir/backup/xtrabackup_binlog_info
    xb_binlog_info_innodb=$topdir/backup/xtrabackup_binlog_pos_innodb

    # --binlog-info=auto
    xtrabackup --backup --binlog-info=auto --target-dir=$topdir/backup

    if [ $bsbi_avail = 1 ] && [ $gtid = 0 ]
    then
        verify_binlog_info_lockless
    else
        verify_binlog_info_on
    fi

    rm -rf $topdir/backup

    # --binlog-info=off
    xtrabackup --backup --binlog-info=off --target-dir=$topdir/backup

    verify_binlog_info_off

    rm -rf $topdir/backup

    # --binlog-info=on
    xtrabackup --backup --binlog-info=on --target-dir=$topdir/backup

    verify_binlog_info_on

    rm -rf $topdir/backup

    # --binlog-info=lockless

    if [ $bsbi_avail = 0 ]
    then
       run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --binlog-info=lockless \
                              --target-dir=$topdir/backup
    else
        xtrabackup --backup --binlog-info=lockless --target-dir=$topdir/backup
        verify_binlog_info_lockless
    fi

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
    grep "recover_binlog_info = 0" $topdir/backup/xtrabackup_checkpoints

    normalize_path $xb_binlog_info

    if [ $gtid = 1 ]
    then
		diff $xb_binlog_info - <<EOF
$binlog_file	$binlog_pos	$gtid_executed
EOF
	else
		diff $xb_binlog_info - <<EOF
$binlog_file	$binlog_pos
EOF
	fi
    xtrabackup --prepare --target-dir=$topdir/backup

    normalize_path $xb_binlog_info_innodb

    if [ $bsbi_avail = 1 ]
    then
        # Real coordinates in xtrabackup_binlog_pos_innodb
        diff $xb_binlog_info_innodb - <<EOF
$binlog_file	$binlog_pos
EOF
    else
        # Stale coordinates in xtrabackup_binlog_pos_innodb
        diff $xb_binlog_info_innodb - <<EOF
$binlog_file_innodb	$binlog_pos_innodb
EOF
    fi
}

function verify_binlog_info_lockless()
{
    test -f $xb_binlog_info &&
        die "$xb_binlog_info was created on the backup stage with --binlog-info=auto and BSBI available"
    grep "recover_binlog_info = 1" $topdir/backup/xtrabackup_checkpoints

    xtrabackup --prepare --target-dir=$topdir/backup

    normalize_path $xb_binlog_info
    normalize_path $xb_binlog_info_innodb

    diff $xb_binlog_info - <<EOF
$binlog_file	$binlog_pos
EOF
    # Real coordinates in xtrabackup_binlog_pos_innodb
    diff $xb_binlog_info_innodb - <<EOF
$binlog_file	$binlog_pos
EOF
}

function verify_binlog_info_off()
{
    grep "recover_binlog_info = 0" $topdir/backup/xtrabackup_checkpoints

    test -f $xb_binlog_info &&
        die "$xb_binlog_info was created on the backup stage with --binlog-info=off"

    xtrabackup --prepare --target-dir=$topdir/backup

    test -f $xb_binlog_info &&
        die "$xb_binlog_info was created on the prepare stage with --binlog-info=off"

    true
}

test_binlog_info

if is_server_version_higher_than 5.6.0 ; then
    test_binlog_info --gtid-mode=ON --enforce-gtid-consistency=ON \
                     --log-bin --log-slave-updates
fi
