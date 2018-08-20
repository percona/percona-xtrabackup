#
# Bug 1475487: xtrabackup handling of space id conflicts
#

# MySQL 8 fails with multiple files having the same tablespace id found in
# the datadir
require_server_version_lower_than 8.0.1

start_server --innodb_file_per_table

${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t (a INT) engine=InnoDB" test

${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t VALUES (1), (2), (3)" test

datadir=$mysql_datadir

shutdown_server

cp $datadir/test/t.ibd $datadir/test/a.ibd

start_server --innodb_file_per_table

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --backup --datadir=$mysql_datadir \
					    --target-dir=$topdir/backup
