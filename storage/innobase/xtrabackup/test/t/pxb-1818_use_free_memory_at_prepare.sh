#
# PXB-1818 - use free memory at prepare
#
require_debug_sync
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-log-file-size=250M
innodb-flush-log-at-trx-commit=0
"
start_server

mkdir $topdir/backup

$MYSQL $MYSQL_ARGS -Ns -e "CREATE TABLE test.pxb_1818 (ID INT PRIMARY KEY AUTO_INCREMENT, name char(255));"

function insert_data()
{
  while true;
  do
    $MYSQL $MYSQL_ARGS -Ns -e "INSERT INTO test.pxb_1818 VALUES (NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),
    (NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a'),(NULL, 'a');"
  done
}

insert_data &
insert_pid=$!

insert_data &
insert_pid2=$!


xtrabackup --backup --target-dir=$topdir/backup \
  --debug-sync="data_copy_thread_func" &
job_pid=$!

pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
start_lsn=`innodb_lsn`
current_lsn=`innodb_lsn`

#wait to have more than 130Mb of records
vlog "waiting to have 130Mb of redo log"
i=0
while [ $(($current_lsn - $start_lsn)) -lt "136314880" ] ; do
  if ((i % 10 == 0)); then
    vlog "Current Redo log size: $(($current_lsn - $start_lsn)) bytes"
  fi
  sleep 1
  current_lsn=`innodb_lsn`
  ((i=i+1))
done

kill -SIGKILL ${insert_pid} ${insert_pid2}
record_db_state test

vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid

run_cmd wait $job_pid

cp -R ${topdir}/backup ${topdir}/backup2


vlog "Test #1 not setting use-free-memory-pct"
xtrabackup --prepare --target-dir=$topdir/backup

if grep -q "Increasing the buffer pool to" $OUTFILE ;
then
    vlog "xtrabackup did resized BP at --prepare."
    exit -1
fi

if grep -q "Not increasing Buffer Pool as it will exceed" $OUTFILE ;
then
    vlog "xtrabackup sould not have attempted to increase buffer pool."
    exit -1
fi

stop_server
rm -rf $mysql_datadir
xtrabackup --move-back --target-dir=$topdir/backup
start_server

verify_db_state test

rm -rf $topdir/backup
cp -R ${topdir}/backup2 ${topdir}/backup

vlog "Test #2 resize buffer pool"
xtrabackup --prepare --target-dir=$topdir/backup --use-free-memory-pct=100

if ! grep -q "Increasing the buffer pool to" $OUTFILE ;
then
    vlog "xtrabackup did not resized BP at --prepare."
    exit -1
fi
stop_server
rm -rf $mysql_datadir
xtrabackup --move-back --target-dir=$topdir/backup
start_server

verify_db_state test


vlog "Test #3 do not resize buffer pool"
xtrabackup --prepare --target-dir=$topdir/backup2 --use-free-memory-pct=0

if ! grep -q "Not increasing Buffer Pool as it will exceed 0% of free memory" $OUTFILE ;
then
    vlog "xtrabackup sould have failed to increase buffer pool."
    exit -1
fi

stop_server
rm -rf $mysql_datadir
xtrabackup --move-back --target-dir=$topdir/backup2
start_server

verify_db_state test