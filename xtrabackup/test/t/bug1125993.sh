########################################################################
# Bug #1125993: To specify sleep interval in log copying thread
########################################################################

. inc/common.sh

start_server --innodb_file_per_table

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1(a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (4), (5), (6);
EOF

vlog "Starting backup"
innobackupex  --no-timestamp $topdir/backup --log-copy-interval=590
