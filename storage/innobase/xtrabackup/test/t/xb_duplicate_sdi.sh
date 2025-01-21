
############################################################################
# PXB-2101 --prepare crash if exchange partition happened
############################################################################
require_server_version_higher_than 8.0.22

MYSQLD_EXTRA_MY_CNF_OPTS="
lower_case_table_names
"

. inc/common.sh
keyring_args=""
start_server

vlog "case#1 check exchange partition"

mysql test <<EOF
SET NAMES utf8;
CREATE TABLE t1(a  INT NOT NULL, PRIMARY KEY(a)) engine = InnoDB;
CREATE TABLE pt1(a INT NOT NULL, PRIMARY KEY(a)) engine = InnoDB
PARTITION BY RANGE (a) PARTITIONS 3 (
PARTITION p1 VALUES LESS THAN (1000),
PARTITION p2 VALUES LESS THAN (2000),
PARTITION p3 VALUES LESS THAN (3000));
INSERT INTO t1 VALUES(1),(2),(3),(4);
INSERT INTO pt1 VALUES(100),(101),(102),(103),(104);
ALTER TABLE pt1 EXCHANGE PARTITION p1 WITH TABLE t1;

CREATE TABLE t1你好(a  INT NOT NULL, PRIMARY KEY(a)) engine = InnoDB;
CREATE TABLE pt1你好(a INT NOT NULL, PRIMARY KEY(a)) engine = InnoDB
PARTITION BY RANGE (a) PARTITIONS 3 (
PARTITION p1 VALUES LESS THAN (1000),
PARTITION p2 VALUES LESS THAN (2000),
PARTITION p3 VALUES LESS THAN (3000));
INSERT INTO t1你好 VALUES(1),(2),(3),(4);
INSERT INTO pt1你好 VALUES(100),(101),(102),(103),(104);
ALTER TABLE pt1你好 EXCHANGE PARTITION p1 WITH TABLE t1你好;
EOF
innodb_wait_for_flush_all
run_cmd backup_and_restore test

vlog "case#2 check import tablespace"

mysql test <<EOF
DROP TABLE t1;
CREATE TABLE t1(a  INT NOT NULL, PRIMARY KEY(a)) engine = InnoDB;
ALTER TABLE t1 DISCARD TABLESPACE;
CREATE DATABASE test2;
use test2;
CREATE TABLE t1(a  INT NOT NULL, PRIMARY KEY(a)) engine = InnoDB;
FLUSH table t1 for EXPORT;
UNLOCK TABLES;
EOF

run_cmd cp $MYSQLD_DATADIR/test2/* $MYSQLD_DATADIR/test/

run_cmd ls -tlr $MYSQLD_DATADIR/test
mysql -e "ALTER TABLE t1 IMPORT TABLESPACE" test

innodb_wait_for_flush_all
run_cmd backup_and_restore test

vlog "case#3 check exchange partition on imported tablespace"
mysql -e "ALTER TABLE pt1 EXCHANGE PARTITION p1 WITH TABLE t1" test
innodb_wait_for_flush_all
run_cmd backup_and_restore test

# Use datadir upgraded from 8.4.0 with duplicate SDI
# to print SDI:
# for f in $(ls datadir/test); do echo $f; ibd2sdi $f |  jq '.[1].id'; done
vlog "case#4 check prepare works with backup having duplicate SDI"
stop_server
#cleanup backup and place backup with duplicate SDI
rm -rf $mysql_datadir $topdir/backup && mkdir -p $mysql_datadir

run_cmd tar -xf inc/duplicate_sdi_datadir.tar.gz -C $mysql_datadir

MYSQLD_START_TIMEOUT=1200
#start server
start_server

#backup
xtrabackup --backup --target-dir=$topdir/backup
#prepare
xtrabackup --prepare --target-dir=$topdir/backup
#restore
stop_server
rm -rf $mysql_datadir && mkdir -p $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup

start_server
run_cmd check_count test t1 5
run_cmd check_count test pt1 4
