#
# common test for log archiving
#

mysql -e "set global innodb_redo_log_archive_dirs='b:$TEST_VAR_ROOT/dir_b;a:$TEST_VAR_ROOT/dir_a'"

load_sakila

mysql -e "create table t (a int)" test

mysql -e "insert into t (a) values (1), (2), (3), (4), (5), (6), (7), (8), (9), (10)" test
mysql -e "insert into t select * from t" test
mysql -e "insert into t select * from t" test
mysql -e "insert into t select * from t" test
mysql -e "insert into t select * from t" test
mysql -e "insert into t select * from t" test
mysql -e "insert into t select * from t" test
mysql -e "insert into t select * from t" test
mysql -e "insert into t select * from t" test
mysql -e "insert into t select * from t" test

while true ; do
    mysql -e "insert into t select * from t" test >/dev/null 2>/dev/null
done &

xtrabackup --backup --target-dir=$topdir/backup

stop_server

xtrabackup --prepare --target-dir=$topdir/backup \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

start_server

mysql -e "CHECKSUM TABLE t" test

# PXB-1928: PXB does not use the redo log archiving feature if label is not specified

mysql -e "set global innodb_redo_log_archive_dirs=':$TEST_VAR_ROOT/dir_b'"

xtrabackup --backup --target-dir=$topdir/bakup_pxb_1928 2> >( tee $topdir/pxb-1928 )

if ! grep -q "Waiting for archive file" $topdir/pxb-1928 ; then
    die "xtrabackup did not use redo log archiving"
fi

# PXB-1938: PXB leaves empty directories in redo log archive directory after backup

ls $TEST_VAR_ROOT/dir_a
ls $TEST_VAR_ROOT/dir_b

[ "$( ls -A $TEST_VAR_ROOT/dir_a )" ] && die "directory $TEST_VAR_ROOT/dir_a is not empty" || true
[ "$( ls -A $TEST_VAR_ROOT/dir_b )" ] && die "directory $TEST_VAR_ROOT/dir_b is not empty" || true

# PXB-3059 add a innodb_redo_log archive option to xtrabackup

#PXB-3059-1 mysql  and  xtrabackup all set and not null
mysql -e "set global innodb_redo_log_archive_dirs='temp:$TEST_VAR_ROOT/dir_set_1'"
xtrabackup  --backup  --target-dir=$topdir/backup1  --redo_log_arch_dir=temp:$topdir/dir_1
redo_dir_1=$(mysql  -e "show variables like '%innodb_redo_log_archive_dirs%'"  2> /dev/null  | grep innodb_redo_log_archive_dirs| awk -F ' '  '{print $2}')
if [ "$redo_dir_1" != "temp:$TEST_VAR_ROOT/dir_set_1" ]
then
    die "innodb_redo_log_archive_dirs has been changed"
    exit -1 
fi
[ ! -d $TEST_VAR_ROOT/dir_set_1 ] && die "innodb_redo_log_archive_dirs not task effect"


#PXB-3059-2 mysql not set  ,  xtrabackup  set
mysql -e "set global innodb_redo_log_archive_dirs=NULL"
xtrabackup  --backup  --target-dir=$topdir/backup2   --redo_log_arch_dir=temp:$topdir/dir_2
redo_dir_2=$(mysql  -e "show variables like '%innodb_redo_log_archive_dirs%'"  2> /dev/null  | grep innodb_redo_log_archive_dirs| awk -F ' '  '{print $2}')
[ $redo_dir_2  ] && die  "innodb_redo_log_archive_dirs has been changed"
[  -d $topdir/dir_2 ] && die "innodb_redo_log_archive_dirs not task effect"

#PXB-3059-3 mysql set  ,  xtrabackup  not set
mysql -e "set global innodb_redo_log_archive_dirs='temp:$TEST_VAR_ROOT/dir_set_3'"
xtrabackup  --backup  --target-dir=$topdir/backup3  
redo_dir_3=$(mysql  -e "show variables like '%innodb_redo_log_archive_dirs%'"  2> /dev/null  | grep innodb_redo_log_archive_dirs| awk -F ' '  '{print $2}')
if [ "$redo_dir_3" != "temp:$TEST_VAR_ROOT/dir_set_3" ]
then
    die "innodb_redo_log_archive_dirs has been changed"
    exit -1
fi
[  -d $TEST_VAR_ROOT/dir_set_3 ] && die "innodb_redo_log_archive_dirs not task effect"



#PXB-3059-4   mysql  and  xtrabackup all not  set 
mysql -e "set global innodb_redo_log_archive_dirs=NULL"
xtrabackup  --backup  --target-dir=$topdir/backup4
redo_dir_4=$(mysql  -e "show variables like '%innodb_redo_log_archive_dirs%'"  2> /dev/null  | grep innodb_redo_log_archive_dirs| awk -F ' '  '{print $2}')
[  ! $redo_dir_4  ] && die  "innodb_redo_log_archive_dirs has been changed"






