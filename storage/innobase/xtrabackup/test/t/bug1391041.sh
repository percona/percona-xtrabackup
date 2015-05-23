########################################################################
# Bug 1391041: innobackupex should skip GTID output is GTID_MODE is OFF
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

MYSQLD_EXTRA_MY_CNF_OPTS="
gtid_mode=on
log_slave_updates=on
enforce_gtid_consistency=on
"
start_server

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, c INT);
INSERT INTO t(c) VALUES (1),(2),(3),(4),(5),(6),(7),(8);
SET AUTOCOMMIT=0;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
COMMIT;
EOF

innobackupex --no-timestamp $topdir/backup

binlog_info=$topdir/backup/xtrabackup_binlog_info

if ! egrep -q "filename.*position" $OUTFILE ; then
	die "filename/position NOT FOUND with GTID_MODE=ON"
fi

if ! egrep -q "GTID of the last change" $OUTFILE ; then
	die "GTID of the last change not found with GTID_MODE=ON"
fi

if ! egrep -q "^mysql-bin.[0-9]+[[:space:]]+[0-9]+" $binlog_info ; then
	die "filename/position NOT FOUND with GTID_MODE=ON"
fi

if ! egrep -q "[a-f0-9:-]+$" $binlog_info ; then
	die "GTID not found in xtrabackup_binlog_info with GTID_MODE=ON"
fi

echo -n >$OUTFILE
rm -rf $topdir/backup
MYSQLD_EXTRA_MY_CNF_OPTS=

stop_server
start_server

innobackupex --no-timestamp $topdir/backup

if egrep -q "GTID of the last change" $OUTFILE ; then
	die "Found GTID of the last change with GTID_MODE=OFF"
fi

if ! grep -q "filename.*position" $OUTFILE ; then
	die "filename/position not found with GTID_MODE=OFF"
fi

if ! egrep -q "^mysql-bin.[0-9]+[[:space:]]+[0-9]+$" $binlog_info ; then
	die "Found filename/position in xtrabackup_binlog_info with GTID_MODE=OFF"
fi

if egrep -q "^[a-f0-9:-]+$" $binlog_info ; then
	die "Found GTID of the last change in xtrabackup_binlog_info with GTID_MODE=OFF"
fi
