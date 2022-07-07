. inc/fifo_common.sh


start_server
create_base_table

vlog "Test 1 - Create a backup and restore"

record_db_state test

mkdir ${topdir}/backup
take_backup_fifo_xbstream ${topdir}/stream '--parallel=4 --fifo-streams=3' "-x -C ${topdir}/backup --fifo-streams=3"

stop_server
xtrabackup --prepare --target-dir=${topdir}/backup
rm -rf ${mysql_datadir}
xtrabackup --copy-back --target-dir=${topdir}/backup
start_server
verify_db_state test
rm -rf ${topdir}/backup
vlog "Test 1 - Completed"

vlog "Test 2 - Create a incremental backup and restore"
vlog "Test 2 - Taking full backup"
mkdir ${topdir}/full ${topdir}/inc1 ${topdir}/inc2
run_insert &
insert_pid=$!
take_backup_fifo_xbstream ${topdir}/stream '--parallel=4 --fifo-streams=3' "-x -C ${topdir}/full --fifo-streams=3"
wait ${insert_pid}
run_insert &
insert_pid=$!
vlog "Test 2 - Taking inc1 backup"
take_backup_fifo_xbstream ${topdir}/stream "--parallel=4 --fifo-streams=3 --incremental-basedir=${topdir}/full" "-x -C ${topdir}/inc1 --fifo-streams=3"
wait ${insert_pid}
vlog "Test 2 - Taking inc2 backup"
take_backup_fifo_xbstream ${topdir}/stream "--parallel=4 --fifo-streams=3 --incremental-basedir=${topdir}/inc1" "-x -C ${topdir}/inc2 --fifo-streams=3"
record_db_state test

stop_server
vlog "Test 2 - Preparing full backup"
xtrabackup --prepare --apply-log-only --target-dir=${topdir}/full
vlog "Test 2 - Preparing inc1 backup"
xtrabackup --prepare --apply-log-only --target-dir=${topdir}/full --incremental-dir=${topdir}/inc1
vlog "Test 2 - Preparing inc2 backup"
xtrabackup --prepare --target-dir=${topdir}/full --incremental-dir=${topdir}/inc2
rm -rf ${mysql_datadir}
vlog "Test 2 - Runninc copy-back"
xtrabackup --copy-back --target-dir=${topdir}/full
start_server
verify_db_state test
rm -rf ${topdir}/full ${topdir}/inc1 ${topdir}/inc2
vlog "Test 2 - Completed"

vlog "Test 3 - different fifo streams on xtrabackup and xbcloud"
load_sakila
stream_dir=${topdir}/stream
mkdir ${topdir}/stream_backup
xtrabackup --backup --fifo-streams=2 --target-dir=${stream_dir} --throttle=1 &
pxb_pid=$!
wait_for_file_to_generated ${stream_dir}/thread_0
run_cmd_expect_failure xbstream --fifo-dir=${stream_dir} -x -C ${topdir}/stream_backup --fifo-streams=3 --fifo-timeout=5
run_cmd_expect_failure wait ${pxb_pid}
vlog "Test 3 - Completed"
