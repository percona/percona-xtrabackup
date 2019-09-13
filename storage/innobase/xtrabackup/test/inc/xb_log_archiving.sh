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
