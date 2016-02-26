########################################################################
# Test correct handling of "broken pipe" error for streaming backups
# Is the test case for
#     bug #1452387: xtrabackup never verifies return code of ds_close
########################################################################

. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_data_file_path=ibdata1:100M:autoextend
innodb_file_per_table
"

start_server

$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.t1(a INT)"
$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.t3(a INT)"
$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.t4(a INT)"
$MYSQL $MYSQL_ARGS -e "CREATE TABLE test.t5(a INT)"

xtrabackup --backup --stream=xbstream \
	--target-dir=$topdir/backup | dd bs=1024 count=103000 of=/dev/null

if [ ${PIPESTATUS[0]} -eq 0 ] ; then
	die "xtrabackup completed OK when it was expected to fail"
fi
