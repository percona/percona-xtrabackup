. inc/common.sh

start_server --innodb_file_per_table

vlog "Creating test data"
run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE DATABASE db1;
USE db1;
CREATE TABLE t1(a INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1),(2),(3);
CREATE DATABASE db2;
USE db2;
CREATE TABLE t1(a INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1),(2),(3);
EOF

vlog "Creating backup directory"
mkdir -p $topdir/backup

vlog "Starting backup"
xtrabackup --datadir=$mysql_datadir --backup --databases=db1 --target-dir=$topdir/backup

vlog "Preparing backup"
xtrabackup --datadir=$mysql_datadir --prepare --databases=db1 --export --target-dir=$topdir/backup

if grep -q "InnoDB data dictionary has tablespace" $OUTFILE; then
  exit 1
fi

stop_server
