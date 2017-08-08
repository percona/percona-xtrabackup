start_server

load_dbase_schema incremental_sample
multi_row_insert incremental_sample.test \({1..100},100\)

vlog "Creating a MyISAM-powered clone of the incremental_sample.test"
mysql -e "show create table incremental_sample.test;" \
    | tail -n +2 \
    | sed -r 's/test\s+CREATE TABLE `test`/CREATE TABLE `test_MyISAM`/' \
    | sed 's/ENGINE=InnoDB/ENGINE=MyISAM/' \
    > $topdir/is_test_myISAM.sql

mysql incremental_sample <<EOF
$(cat $topdir/is_test_myISAM.sql);
insert into test_MyISAM select * from test;
EOF

record_db_state incremental_sample

vlog "Default buffer size"
xtrabackup --backup --target-dir=$topdir/backup \
	--read_buffer_size=0 \
	2>&1 | tee $topdir/xb.log
rm -rf $topdir/backup

vlog "100Mb buffer"
xtrabackup --backup --target-dir=$topdir/backup \
	--read_buffer_size=100Mb \
	2>&1 | tee $topdir/xb.log

stop_server
rm -rf $mysql_datadir/*

vlog "Restoring..."
xtrabackup --prepare --target-dir=$topdir/backup
xtrabackup --copy-back --target-dir=$topdir/backup
start_server

verify_db_state incremental_sample
