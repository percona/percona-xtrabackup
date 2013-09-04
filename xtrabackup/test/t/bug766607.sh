. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="innodb-file-per-table"

start_server
load_dbase_schema incremental_sample

# Backup dir
mkdir -p $topdir/backup

innobackupex --no-timestamp $topdir/backup/full

vlog "Creating and filling a new table"
run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t(a int) ENGINE=InnoDB;
INSERT INTO t VALUES (1), (2), (3);
FLUSH LOGS;
EOF

force_checkpoint

vlog "Making incremental backup"
innobackupex --incremental --no-timestamp --incremental-basedir=$topdir/backup/full $topdir/backup/delta

vlog "Preparing full backup"
innobackupex --apply-log --redo-only $topdir/backup/full

# The following would fail before the bugfix
vlog "Applying incremental delta"
innobackupex --apply-log --redo-only --incremental-dir=$topdir/backup/delta $topdir/backup/full

vlog "Preparing full backup"
innobackupex --apply-log $topdir/backup/full
vlog "Data prepared for restore"

stop_server
rm -r $mysql_datadir/*

vlog "Copying files"

cd $topdir/backup/full/
cp -r * $mysql_datadir
cd $topdir

vlog "Data restored"
start_server

run_cmd $MYSQL $MYSQL_ARGS -e "SELECT * FROM t" test
