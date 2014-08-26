########################################################################
# Bug #569387: innobackupex ignores --databases
#              Testcase covers using --databases option with InnoDB
#              database (list is specified in option itself)
########################################################################

. inc/common.sh

start_server --innodb-file-per-table

cat <<EOF | run_cmd $MYSQL $MYSQL_ARGS

	CREATE DATABASE test1;

	CREATE TABLE test1.a (a INT PRIMARY KEY);
	CREATE TABLE test1.b (b INT PRIMARY KEY);
	CREATE TABLE test1.c (c INT PRIMARY KEY);

	CREATE TABLE test.a (a INT PRIMARY KEY);
	CREATE TABLE test.b (b INT PRIMARY KEY);
	CREATE TABLE test.c (c INT PRIMARY KEY);

EOF

innobackupex --no-timestamp --databases=test\ test1.b\ test1.c\ mysql\ performance_schema $topdir/backup
innobackupex --apply-log $topdir/backup
vlog "Backup taken"

stop_server

# Restore partial backup
# Remove database
rm -rf $mysql_datadir/*
vlog "Original database removed"

# Restore database from backup
innobackupex --copy-back $topdir/backup
vlog "database restored from backup"

start_server

OUT=`run_cmd $MYSQL $MYSQL_ARGS -e "USE test; SHOW TABLES; USE test1; SHOW TABLES;" | tr -d '\n'`

if [ $OUT != "Tables_in_testabcTables_in_test1bc" ] ; then
	die "Backed up tables set doesn't match filter"
fi
