################################################################################
# Bug #1174314
# Test xtrabackup --stats with server dir
################################################################################

. inc/common.sh

logdir=${TEST_VAR_ROOT}/logs
mkdir $logdir

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_log_group_home_dir=$logdir
"

start_server

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1(a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1), (2), (3);
EOF

shutdown_server

# there is inconsistency between shutdown_server and stop_server
# stop_server sets XB_ARGS="--no-defaults", while shutdown_server
# doesn't.
# we pass all necessary options as an arguments, so if someday this
# will be changed, test still will work
xtrabackup --stats --datadir=${MYSQLD_DATADIR} \
        --innodb_log_group_home_dir=$logdir

vlog "stats did not fail"
