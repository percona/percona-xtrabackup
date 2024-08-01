#
# Basic test of InnoDB encryption support
#

require_server_version_higher_than 8.0.10
require_lz4
require_zstd

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_encrypt=ON
innodb_undo_log_encrypt=ON
"

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

load_sakila

mysql -e 'CREATE TABLE t (a INT) engine=InnoDB' test

function start_workload()
{
	mysql -e 'INSERT INTO t (a) VALUES (1), (2), (3), (4)' test
	( while true ; do
		echo 'INSERT INTO t (a) SELECT a FROM t LIMIT 200;'
		sleep 1
	done ) | mysql test
}

function test_do() {
	xtra_backup_args=$1
	xtra_prepare_args=$2
	xtra_restore_args=$3

	start_workload &
	job=$!

	xtrabackup --backup --target-dir=$topdir/backup ${xtra_backup_args}

	# for compressed backup xtrabackup_logfile should not be compressed since redo log is encrypted
	if [[ $1 == *"compress"* ]]; then
		test -f $topdir/backup/xtrabackup_logfile || die "xtrabackup_logfile file not found"
		test -f $topdir/backup/undo_001 || die "undo_001 not found"
		xtrabackup --decompress --target-dir=$topdir/backup
	fi

	xtrabackup --prepare --target-dir=$topdir/backup ${xtra_prepare_args}

	record_db_state sakila

	kill -SIGKILL $job

	stop_server

	rm -rf $mysql_datadir

	xtrabackup --copy-back --target-dir=$topdir/backup ${xtra_restore_args}

	cp ${instance_local_manifest}  $mysql_datadir
    cp ${keyring_component_cnf} $mysql_datadir

	start_server

	verify_db_state sakila
	mysql -e 'SELECT SUM(a) FROM t' test

	rm -rf $topdir/backup
}


test_do "" "--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}" ""
test_do "--transition-key=123_" "--transition-key=123_" "--transition-key=123_ --generate-new-key"
test_do "--compress=lz4" "--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}" ""
test_do "--compress=zstd" "--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}" ""
