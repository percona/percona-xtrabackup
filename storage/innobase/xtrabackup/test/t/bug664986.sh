########################################################################
# Bug #664986: directory permission and backup failure
########################################################################

. inc/common.sh

[[ $EUID -eq 0 ]] && skip_test "Can not be run by root"

start_server --innodb_file_per_table

run_cmd $MYSQL $MYSQL_ARGS <<EOF
CREATE DATABASE test_bug664986_innodb;
CREATE TABLE test_bug664986_innodb.t (a INT) ENGINE=InnoDB;

CREATE DATABASE test_bug664986_myisam;
CREATE TABLE test_bug664986_myisam.t (a INT) ENGINE=MyISAM;

EOF

# Test that wrong directory permissions result in a backup failure
# for both InnoDB and non-InnoDB files
chmod 000 $MYSQLD_DATADIR/test_bug664986_innodb
run_cmd_expect_failure $IB_BIN $IB_ARGS --no-timestamp $topdir/backup
chmod 777 $MYSQLD_DATADIR/test_bug664986_innodb
rm -rf $topdir/backup

chmod 000 $MYSQLD_DATADIR/test_bug664986_myisam
run_cmd_expect_failure $IB_BIN $IB_ARGS --no-timestamp $topdir/backup
chmod 777 $MYSQLD_DATADIR/test_bug664986_myisam
rm -rf $topdir/backup

# Test that wrong file permissions result in a backup failure
# for both InnoDB and non-InnoDB files
chmod 000 $MYSQLD_DATADIR/test_bug664986_innodb/t.ibd
run_cmd_expect_failure $IB_BIN $IB_ARGS --no-timestamp $topdir/backup
chmod 644 $MYSQLD_DATADIR/test_bug664986_innodb/t.ibd
rm -rf $topdir/backup

chmod 000 $MYSQLD_DATADIR/test_bug664986_myisam/t.MYD
run_cmd_expect_failure $IB_BIN $IB_ARGS --no-timestamp $topdir/backup
chmod 644 $MYSQLD_DATADIR/test_bug664986_myisam/t.MYD
rm -rf $topdir/backup
