#!/bin/bash
. inc/common.sh

start_server
function run_inserts()
{
    for i in `seq 1 100`
    do
        mysql -e 'insert into `$some()ta;ble{}><&|` values (1)' test
    done
}

mysql -e 'create table `$some()ta;ble{}><&|` (i int)' test
mysql -e 'create table t1 (i int)' test

run_inserts &
insert_pid=$!

xtrabackup --backup --target-dir=$topdir/backup --compress=zstd --encrypt=AES256 --encrypt-key=percona_xtrabackup_is_awesome___


vlog "case#1 backup with compress&encryption and modified file name"


cp -R $topdir/backup $topdir/backup2

echo "echo secret_text_not_to_be_printed" > $topdir/backup2/runme.sh
cp $topdir/backup2/test/t1.ibd.zst.xbcrypt $topdir/backup2/'./m'\''; bash runme.sh;#.zst.xbcrypt'

run_cmd_expect_failure xtrabackup --target-dir=$topdir/backup2 --encrypt-key=percona_xtrabackup_is_awesome___ --decrypt=AES256 --decompress --parallel=4 2>&1 | tee $topdir/pxb.log

if grep -q 'secret_text_not_to_be_printed' $topdir/pxb.log
then
	die "xtrabackup should not print secret_text_not_to_be_printed"
fi

if ! grep -q 'has one or more invalid characters' $topdir/pxb.log
then
	die "xtrabackup did not show error about not allowed characters"
fi

vlog "case#2 backup with compress&encryption and filename containing some not allowed characters"
rm -rf $topdir/backup2
cp -R $topdir/backup $topdir/backup2
cp $topdir/backup2/test/t1.ibd.zst.xbcrypt $topdir/backup2/'some$\{};()><&|char.zst.xbcrypt'
run_cmd_expect_failure xtrabackup --target-dir=$topdir/backup2 --encrypt-key=percona_xtrabackup_is_awesome___ --decrypt=AES256 --decompress --parallel=4 2>&1 | tee $topdir/pxb2.log

if ! grep -q 'has one or more invalid characters' $topdir/pxb2.log
then
	die "xtrabackup did not show error about not allowed characters"
fi

vlog "case#3 backup with compress&encryption and without modified file name"

wait $insert_pid
record_db_state test

run_cmd xtrabackup --backup --target-dir=$topdir/inc1 --incremental-basedir=$topdir/backup --compress=zstd --encrypt=AES256 --encrypt-key=percona_xtrabackup_is_awesome___

run_cmd xtrabackup --target-dir=$topdir/backup --encrypt-key=percona_xtrabackup_is_awesome___ --decrypt=AES256 --decompress --parallel=4
run_cmd xtrabackup --target-dir=$topdir/inc1 --encrypt-key=percona_xtrabackup_is_awesome___ --decrypt=AES256 --decompress --parallel=4
run_cmd xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
run_cmd xtrabackup --prepare --target-dir=$topdir/backup --incremental-dir=$topdir/inc1

# Restore
stop_server
rm -rf $mysql_datadir
run_cmd xtrabackup --copy-back --target-dir=$topdir/backup
start_server
# Verify backup
verify_db_state test
