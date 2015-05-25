############################################################################
# Bug 1399471: XtraBackup 2.2.x crash on prepare when backing up
#              MySQL/Percona Server 5.5.x (temp tables involved)
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

function create_tmp_table()
{
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
START TRANSACTION;
CREATE TEMPORARY TABLE tmp_crash(id INT) ENGINE=InnoDB;
SELECT SLEEP(10000);
EOF
}

create_tmp_table &
job_id=$!

vlog "Starting backup"

innobackupex --no-timestamp $topdir/backup

kill -SIGKILL $job_id

vlog "Preparing backup"
innobackupex --apply-log $topdir/backup

stop_server

rm -rf ${mysql_datadir}/*

innobackupex --copy-back $topdir/backup

start_server
