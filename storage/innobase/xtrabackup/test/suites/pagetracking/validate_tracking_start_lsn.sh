############################################################################
#PXB-2658 PXB waits for checkpoint LSN to be greater than page tracking LSN in full and incremental backup
############################################################################

. inc/common.sh
. inc/keyring_file.sh

require_debug_server
require_debug_pxb_version

start_server

vlog "case#1 backup should wait till pagetracking lsn is more than checkpoint lsn"
mysql test <<EOF
CREATE TABLE t1(a INT) ENGINE=INNODB;
SET GLOBAL innodb_log_checkpoint_now = 1;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
INSTALL COMPONENT "file://component_mysqlbackup";
INSERT INTO t1 VALUES(100);
EOF

xtrabackup --backup --target-dir=$topdir/backup --page-tracking --debug_sync="xtrabackup_after_wait_page_tracking" 2>&1 | tee $topdir/pxb.log &
job_pid=$!

pid_file=$topdir/backup/xtrabackup_debug_sync

wait_for_xb_to_suspend $pid_file

xb_pid=`cat $pid_file`

mysql test << EOF
SET GLOBAL innodb_page_cleaner_disabled_debug = 0;
SET GLOBAL innodb_log_checkpoint_now = 1;
EOF

vlog "Resuming xtrabackup full backup"
kill -SIGCONT $xb_pid

run_cmd wait $job_pid

if ! grep -q ' pagetracking: Sleeping for 1 second' $topdir/pxb.log
then
	die "xtrabackup should print wait time for pagetracking."
fi

# run it multiple time so page tracking lsn can move
for i in {1..10}
do
 load_sakila
 drop_dbase sakila
done

mysql test << EOF
SET GLOBAL innodb_log_checkpoint_now = 1;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
INSERT INTO t1 VALUES(100);
EOF

vlog "case#2 checkpoint is stopped and still incremental should complete "

xtrabackup --backup --target-dir=$topdir/inc  --incremental-basedir=$topdir/backup  --page-tracking --debug_sync="xtrabackup_after_wait_page_tracking" 2>&1 | tee $topdir/pxb_inc.log &
job_pid=$!

pid_file=$topdir/inc/xtrabackup_debug_sync

wait_for_xb_to_suspend $pid_file

xb_pid=`cat $pid_file`

mysql test << EOF
SET GLOBAL innodb_page_cleaner_disabled_debug = 0;
SET GLOBAL innodb_log_checkpoint_now = 1;
EOF

vlog "Resuming xtrabackup full backup"
kill -SIGCONT $xb_pid

run_cmd wait $job_pid

if  grep -q ' pagetracking: Sleeping for 1 second' $topdir/pxb_inc.log
then
	die "xtrabackup should not print wait time for pagetracking."
fi
