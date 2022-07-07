. inc/xbcloud_common.sh
. inc/fifo_common.sh
is_xbcloud_credentials_set

# call cleanup in case of error
trap 'xbcloud_cleanup' ERR

start_server
write_credentials
load_dbase_schema sakila
load_dbase_data sakila
create_base_table

mkdir ${topdir}/full ${topdir}/inc1 ${topdir}/inc2

vlog "Test 1 - Taking full backup"
run_insert &
insert_pid=$!
take_backup_fifo_xbcloud ${topdir}/stream "--parallel=4 --fifo-streams=3 --extra-lsndir=${full_backup_dir}" "--parallel=10 --fifo-streams=3 ${full_backup_name}"
wait ${insert_pid}
run_insert &
insert_pid=$!
vlog "Test 1 - Taking inc1 backup"
take_backup_fifo_xbcloud ${topdir}/stream "--parallel=4 --fifo-streams=3 --incremental-basedir=${full_backup_dir} --extra-lsndir=${inc_backup_dir}" "--parallel=10 --fifo-streams=3 ${inc_backup_name}"
wait ${insert_pid}
vlog "Test 1 - Taking inc2 backup"
take_backup_fifo_xbcloud ${topdir}/stream "--parallel=4 --fifo-streams=3 --incremental-basedir=${inc_backup_dir}" "--parallel=10 --fifo-streams=3 ${inc2_backup_name}"
record_db_state test

vlog "download & prepare"
download_fifo_xbcloud ${topdir}/stream "--parallel=10 --fifo-streams=3 ${full_backup_name}" "-x -C ${topdir}/full --fifo-streams=3"
download_fifo_xbcloud ${topdir}/stream "--parallel=10 --fifo-streams=3 ${inc_backup_name}" "-x -C ${topdir}/inc1 --fifo-streams=3"
download_fifo_xbcloud ${topdir}/stream "--parallel=10 --fifo-streams=3 ${inc2_backup_name}" "-x -C ${topdir}/inc2 --fifo-streams=3"
stop_server
vlog "Test 1 - Preparing full backup"
xtrabackup --prepare --apply-log-only --target-dir=${topdir}/full
vlog "Test 1 - Preparing inc1 backup"
xtrabackup --prepare --apply-log-only --target-dir=${topdir}/full --incremental-dir=${topdir}/inc1
vlog "Test 1 - Preparing inc2 backup"
xtrabackup --prepare --target-dir=${topdir}/full --incremental-dir=${topdir}/inc2
rm -rf ${mysql_datadir}
vlog "Test 1 - Running copy-back"
xtrabackup --copy-back --target-dir=${topdir}/full
start_server
verify_db_state test
rm -rf ${topdir}/full ${topdir}/inc1 ${topdir}/inc2
vlog "Test 1 - Completed"
vlog "Test 2 - Partial Download"

mkdir $topdir/partial
download_fifo_xbcloud ${topdir}/stream "--parallel=10 --fifo-streams=2 ${full_backup_name} ibdata1 sakila/payment.ibd test/t.ibd" "-x -C ${topdir}/partial --fifo-streams=2"
if [ ! -f "$topdir/partial/ibdata1" ] || \
[ ! -f "$topdir/partial/sakila/payment.ibd" ] || \
[ ! -f "$topdir/partial/test/t.ibd" ]
then
    vlog "Error: File was not downloaded"
    exit 1
fi
vlog "Test 2 - Completed"

xbcloud_cleanup
