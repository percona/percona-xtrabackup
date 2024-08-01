#
# common test for log archiving
#

mysql -e "SET GLOBAL innodb_redo_log_archive_dirs='b:$TEST_VAR_ROOT/dir_b;a:$TEST_VAR_ROOT/dir_a'"

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

if [[ "${KEYRING_TYPE}" = "component" ]]; then
  cp ${instance_local_manifest}  $mysql_datadir
  cp ${keyring_component_cnf} $mysql_datadir
fi           

start_server

mysql -e "CHECKSUM TABLE t" test

# PXB-1928: PXB does not use the redo log archiving feature if label is not specified

mysql -e "SET GLOBAL innodb_redo_log_archive_dirs=':$TEST_VAR_ROOT/dir_b'"

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

#PXB-3059-1 mysql and xtrabackup all set and not null
ARCH_DIR="temp:$TEST_VAR_ROOT/dir_set_1"
mysql -e "SET GLOBAL innodb_redo_log_archive_dirs='${ARCH_DIR}'"
xtrabackup  --backup  --target-dir=$topdir/backup1  --redo_log_arch_dir=temp:$topdir/dir_1 2> >( tee $topdir/pxb-3059-1 )
redo_dir_1=$(mysql  -e "SHOW VARIABLES LIKE '%innodb_redo_log_archive_dirs%'"  2> /dev/null  | grep innodb_redo_log_archive_dirs| awk -F ' '  '{print $2}')
if [ "$redo_dir_1" != "${ARCH_DIR}" ]
then
    die "innodb_redo_log_archive_dirs has been changed on test 1"
fi
if ! grep -q "xtrabackup redo_log_arch_dir is set to ${ARCH_DIR}" $topdir/pxb-3059-1 ; then
    die "xtrabackup redo_log_arch_dir not set to ${ARCH_DIR} on test 1"
fi

#PXB-3059-2 mysql not set  ,  xtrabackup  set
mysql -e "SET GLOBAL innodb_redo_log_archive_dirs=NULL"
ARCH_DIR="temp:$topdir/dir_2"
xtrabackup  --backup  --target-dir=$topdir/backup2   --redo_log_arch_dir=${ARCH_DIR} 2> >( tee $topdir/pxb-3059-2 )
redo_dir_after_backup=$(mysql  -e "SHOW VARIABLES LIKE '%innodb_redo_log_archive_dirs%'"  2> /dev/null  | grep innodb_redo_log_archive_dirs| awk -F ' '  '{print $2}')
[ ${redo_dir_after_backup}  ] && die  "innodb_redo_log_archive_dirs has not been reset on test 2"
if ! grep -q "xtrabackup redo_log_arch_dir is set to ${ARCH_DIR}" $topdir/pxb-3059-2 ; then
    die "xtrabackup redo_log_arch_dir not set to ${ARCH_DIR} on test 2"
fi

#PXB-3059-3 mysql set  ,  xtrabackup  not set
ARCH_DIR="temp:$TEST_VAR_ROOT/dir_set_3"
mysql -e "SET GLOBAL innodb_redo_log_archive_dirs='${ARCH_DIR}'"
xtrabackup  --backup  --target-dir=$topdir/backup3 2> >( tee $topdir/pxb-3059-3 )
redo_dir_3=$(mysql  -e "SHOW VARIABLES LIKE '%innodb_redo_log_archive_dirs%'"  2> /dev/null  | grep innodb_redo_log_archive_dirs| awk -F ' '  '{print $2}')
if [ "$redo_dir_3" != "${ARCH_DIR}" ]
then
    die "innodb_redo_log_archive_dirs has been changed on test 3"
fi
if ! grep -q "xtrabackup redo_log_arch_dir is set to ${ARCH_DIR}" $topdir/pxb-3059-3 ; then
    die "xtrabackup redo_log_arch_dir not set to ${ARCH_DIR} on test 3"
fi


#PXB-3059-4   mysql  and  xtrabackup all not  set
mysql -e "SET GLOBAL innodb_redo_log_archive_dirs=NULL"
xtrabackup  --backup  --target-dir=$topdir/backup4
redo_dir_4=$(mysql  -e "SHOW VARIABLES LIKE '%innodb_redo_log_archive_dirs%'"  2> /dev/null  | grep innodb_redo_log_archive_dirs| awk -F ' '  '{print $2}')
[  $redo_dir_4  ] && die  "innodb_redo_log_archive_dirs has been changed on test 4" || true






