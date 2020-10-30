#
# Basic test of InnoDB encryption support
#
. inc/keyring_common.sh
. inc/keyring_file.sh

function cleanup_keyring() {
	rm -rf $keyring_file
}

test_do "ENCRYPTION='y'" "top-secret"
test_do "ENCRYPTION='y' ROW_FORMAT=COMPRESSED" "none"
test_do "ENCRYPTION='y' COMPRESSION='lz4'" "none"

#
# PXB-2216 - Backup changing table encryption version
#
MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS:-""}
innodb-buffer-pool-size=536870912
innodb-log-file-size=536870912
"

XB_EXTRA_MY_CNF_OPTS="${XB_EXTRA_MY_CNF_OPTS:-""}
xtrabackup-plugin-dir=${plugin_dir}
"
prepare_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
start_server
run_cmd $MYSQL $MYSQL_ARGS test -e "SELECT @@server_uuid"
run_cmd $MYSQL $MYSQL_ARGS test -e "CREATE TABLE tb1 (ID INT PRIMARY KEY) ENCRYPTION='Y'"
for i in $(seq 1 10);
do
  run_cmd $MYSQL $MYSQL_ARGS test -e "INSERT INTO tb1 VALUES (${i})"
done
run_cmd $MYSQL $MYSQL_ARGS test -e "CREATE TABLE tb2 (ID INT PRIMARY KEY) ENCRYPTION='Y'"
xtrabackup --backup --target-dir=$topdir/backup1
record_db_state test
stop_server
rm -rf $mysql_datadir
cp -R $topdir/backup1 $topdir/backup2

# Write some garbage to magic section and rewrite checksum
# Prepare must catch and fail.
printf '\xdc\x67\xf3' | dd of=$topdir/backup2/test/tb2.ibd bs=1 seek=10390 count=3 conv=notrunc
$MYSQL_BASEDIR/bin/innochecksum -w crc32 --no-check $topdir/backup2/test/tb2.ibd
run_cmd_expect_failure $XB_BIN $XB_ARGS \
--prepare --target-dir=$topdir/backup2 ${prepare_options}
if ! grep -q "Failed to decrypt encryption information, found unexpected version of it!" $OUTFILE
then
    die "Cannot find 'read unknown ENCRYPTION_KEY_MAGIC' message on xtrabackup log"
fi


xtrabackup --prepare --target-dir=$topdir/backup1 ${prepare_options}
xtrabackup --copy-back --target-dir=$topdir/backup1
start_server
run_cmd verify_db_state test
stop_server
rm -rf $mysql_datadir
rm -rf $topdir/{backup1,backup2}

# cleanup environment variables
MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_encrypt
innodb_undo_log_encrypt
binlog-encryption
log-bin
"
XB_EXTRA_MY_CNF_OPTS=
