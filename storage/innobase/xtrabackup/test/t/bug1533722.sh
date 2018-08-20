if ! is_xtradb || ! is_server_version_higher_than 5.5.23 || ! is_server_version_lower_than 5.6.0
then
    skip_test "Requires PS 5.5.23 or higher, but less than 5.6"
fi

function long_transaction()
{
    mysql foo <<EOF
start transaction;
insert into sbtest values (999999999, 'longrunning transaction');
select sleep(1000);
EOF
}

start_server

mysql <<EOF
create database foo;
CREATE TABLE foo.sbtest (
  i int PRIMARY KEY,
  c char(50) CHARACTER SET utf8 COLLATE utf8_general50_ci NOT NULL DEFAULT ''
) ENGINE=InnoDB;
EOF

multi_row_insert foo.sbtest \({1..100},\'"text"\'\)
record_db_state foo
long_transaction &
sleep 1

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

stop_server
rm -rf $mysql_datadir
xtrabackup --move-back --target-dir=$topdir/backup
start_server

verify_db_state foo
