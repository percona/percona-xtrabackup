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

# PXB-2486: PXB hangs in case of broken pipe, --parallel
# and --encryption or --compress

timeout 20 $XB_BIN $XB_ARGS --backup --stream=xbstream \
	--target-dir=$topdir/backup1 --parallel=10 --encrypt=AES256 \
	--encrypt-key=2222233333232323 | dd bs=1024 count=100 of=/dev/null

if [ ${PIPESTATUS[0]} -eq 124 ] ; then
	die "xtrabackup timeout after 20 seconds when it was expected to fail"
fi

if [[ is_debug_pxb_version ]]; then
	# let the command fail and capture the output
	set +e
	timeout 20 $XB_BIN $XB_ARGS --backup --stream=xbstream \
	--target-dir=$topdir/backup2 --parallel=10  --compress \
	--debug='+d,simulate_compress_error' > /dev/null

	if [ $? -eq 124 ] ; then
		die "xtrabackup timeout after 20 seconds when it was expected to fail"
	fi
	set -e
fi
