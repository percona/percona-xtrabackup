#
# Basic test of InnoDB encryption support
#

require_server_version_higher_than 8.0.10

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_encrypt=ON
innodb_undo_log_encrypt=ON
"

. inc/keyring_file.sh

start_server

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
	xtrabackup --prepare --target-dir=$topdir/backup ${xtra_prepare_args}

	record_db_state sakila

	kill -SIGKILL $job

	stop_server

	rm -rf $mysql_datadir

	xtrabackup --copy-back --target-dir=$topdir/backup ${xtra_restore_args}

	start_server

	verify_db_state sakila
	mysql -e 'SELECT SUM(a) FROM t' test

	rm -rf $topdir/backup
}


test_do "" "--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}" ""
test_do "--transition-key=123_" "--transition-key=123_" "--transition-key=123_ --generate-new-key"
