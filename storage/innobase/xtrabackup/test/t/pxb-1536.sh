#
# PXB-1536: xtrabackup 2.4 does not remove missing tables
#

start_server --innodb_file_per_table

mysql -e 'CREATE DATABASE db1'
mysql -e 'CREATE DATABASE db2'
mysql -e 'CREATE DATABASE db3'
mysql -e 'CREATE DATABASE db4'

mysql -e 'CREATE TABLE t (a INT)' db1
mysql -e 'INSERT INTO t VALUES (1), (1), (3)' db1

mysql -e 'CREATE TABLE t (a INT)' db2
mysql -e 'INSERT INTO t VALUES (1), (1), (3)' db2

mysql -e 'CREATE TABLE t (a INT)' db3
mysql -e 'INSERT INTO t VALUES (1), (1), (3)' db3

if is_server_version_higher_than 5.7.0 ; then
  mysql -e 'CREATE TABLESPACE q ADD DATAFILE "q.ibd"'
  mysql -e 'CREATE TABLE qt (a INT) TABLESPACE q' db4
  mysql -e 'INSERT INTO qt VALUES (1), (1), (3)' db4

  mysql -e 'CREATE TABLESPACE p ADD DATAFILE "p.ibd"'
  mysql -e 'CREATE TABLE pt (a INT) TABLESPACE p' db4
  mysql -e 'INSERT INTO pt VALUES (1), (1), (3)' db4
fi

innodb_wait_for_flush_all

flushed_lsn=`innodb_flushed_lsn`

while [ `innodb_checkpoint_lsn` -lt "$flushed_lsn" ] ; do
  sleep 1
done

( while true ; do
    mysql -e 'INSERT INTO t VALUES (1), (1), (3)' db3
  done ) &

xtrabackup --backup --tables="mysql,sys,performance_schema,db1,p" --target-dir=$topdir/backup

ls -alh $mysql_datadir

stop_server

rm -rf $mysql_datadir

xtrabackup --prepare --target-dir=$topdir/backup

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

mysql -e 'SELECT * FROM t' db1

if is_server_version_higher_than 5.7.0 ; then
  diff -u <(mysql -Nse "SELECT NAME FROM INFORMATION_SCHEMA.INNODB_SYS_TABLES \
                        WHERE NAME NOT LIKE '%mysql%' AND NAME NOT LIKE '%sys%' \
                        ORDER BY NAME") - <<EOF
db1/t
db4/pt
EOF
elif is_server_version_higher_than 5.6.0 ; then
  diff -u <(mysql -Nse "SELECT NAME FROM INFORMATION_SCHEMA.INNODB_SYS_TABLES \
                        WHERE NAME NOT LIKE '%mysql%' AND NAME NOT LIKE '%sys%' \
                        ORDER BY NAME") - <<EOF
db1/t
EOF
fi

if is_server_version_higher_than 5.7.0 ; then
    diff -u <(mysql -Nse "SELECT PATH FROM INFORMATION_SCHEMA.INNODB_SYS_DATAFILES \
                          WHERE PATH NOT LIKE '%mysq%' AND PATH NOT LIKE '%sys%' \
                          ORDER BY PATH") - <<EOF
./db1/t.ibd
./p.ibd
EOF
  diff -u <(mysql -Nse "SELECT NAME FROM INFORMATION_SCHEMA.INNODB_SYS_TABLESPACES \
                        WHERE NAME NOT LIKE '%mysq%' AND NAME NOT LIKE '%sys%' \
                        ORDER BY NAME") - <<EOF
db1/t
p
EOF
elif is_server_version_higher_than 5.6.0 ; then
    diff -u <(mysql -Nse "SELECT PATH FROM INFORMATION_SCHEMA.INNODB_SYS_DATAFILES \
                          WHERE PATH NOT LIKE '%mysq%' AND PATH NOT LIKE '%sys%' \
                          ORDER BY PATH") - <<EOF
./db1/t.ibd
EOF
  diff -u <(mysql -Nse "SELECT NAME FROM INFORMATION_SCHEMA.INNODB_SYS_TABLESPACES \
                        WHERE NAME NOT LIKE '%mysq%' AND NAME NOT LIKE '%sys%' \
                        ORDER BY NAME") - <<EOF
db1/t
EOF
fi

mysql -e 'CREATE DATABASE db2'
mysql -e 'CREATE DATABASE db3'

mysql -e 'CREATE TABLE t (a INT)' db2
mysql -e 'INSERT INTO t VALUES (1), (1), (3)' db2

mysql -e 'CREATE TABLE t (a INT)' db3
mysql -e 'INSERT INTO t VALUES (1), (1), (3)' db3

if is_server_version_higher_than 5.7.0 ; then
  mysql -e 'CREATE TABLESPACE q ADD DATAFILE "q.ibd"'
  mysql -e 'CREATE TABLE qt (a INT) TABLESPACE q' db4
  mysql -e 'INSERT INTO qt VALUES (1), (1), (3)' db4
fi
