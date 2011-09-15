. inc/common.sh

init
run_mysqld --innodb_file_per_table
load_dbase_schema incremental_sample

# Adding initial rows
vlog "Adding initial rows to database..."
numrow=100
count=0
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count, $numrow);" incremental_sample
	let "count=count+1"
done
vlog "Initial rows added"

# Full backup
# backup root directory
mkdir -p $topdir/backup

vlog "Check that --slave-info with --no-lock and no --safe-slave-backup fails"
run_cmd_expect_failure $IB_BIN $IB_ARGS --slave-info --no-lock $topdir/backup

# TODO: add positive tests when the test suite is able to setup replication
