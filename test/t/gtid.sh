########################################################################
# Test support for GTID
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

# Check that the value of gtid_executed is in xtrabackup_binlog_info
if ! egrep -q '^mysql-bin.[0-9]+[[:space:]]+[0-9]+[[:space:]]+[a-f0-9:-]+$' \
    $topdir/backup/xtrabackup_binlog_info
then
    die "Cannot find GTID coordinates in xtrabackup_binlog_info"
fi
