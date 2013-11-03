########################################################################
# Test for XtraDB innodb_fast_checksum option
########################################################################

require_xtradb
require_server_version_lower_than 5.6.0

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-file-per-table
innodb-fast-checksum
"

start_server

test "`$MYSQL $MYSQL_ARGS -Ns -e "SELECT @@innodb_fast_checksum"`" = "1"

load_sakila

backup_dir=${topdir}/backup
innobackupex --no-timestamp $backup_dir

record_db_state sakila

stop_server
rm -rf $MYSQLD_DATADIR/*

innobackupex --apply-log $backup_dir

innobackupex --copy-back $backup_dir

start_server

verify_db_state sakila
