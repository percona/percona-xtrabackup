start_server

load_dbase_schema sakila
load_dbase_data sakila

xtrabackup --backup --target-dir=$topdir/backup
record_db_state sakila

stop_server

rm -rf $mysql_datadir

xtrabackup --prepare --target-dir=$topdir/backup
xtrabackup --copy-back --parallel=10 --target-dir=$topdir/backup \
	2>&1 | tee $topdir/pxb.log

run_cmd grep -q "xtrabackup: Starting 10 threads for parallel data files transfer" $topdir/pxb.log
threads_count=$(grep -Eo '\[[0-9]+\] Copying' $topdir/pxb.log | sort | uniq | wc -l)
vlog "Involved threads count: $threads_count"
if [ $threads_count -eq 1 ]
then
  die "Copying was expected to be done in parallel"
fi

start_server
verify_db_state sakila
