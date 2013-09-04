########################################################################
# Bug #1112224: Missing space_id from *.ibd.meta Leads to Assertion
########################################################################

. inc/common.sh

start_server --innodb_file_per_table

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1(a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (4), (5), (6);
EOF

# Full backup
# backup root directory
vlog "Starting backup"
innobackupex  --no-timestamp $topdir/full

vlog "Rotating the table"

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
RENAME TABLE t1 TO t12;
EOF

vlog "Creating incremental backup"

innobackupex --incremental --no-timestamp \
    --incremental-basedir=$topdir/full $topdir/inc

# remove space_id = something line from .meta file
sed -ie '/space_id/ d' $topdir/inc/test/t12.ibd.meta
vlog "Preparing backup"

innobackupex --apply-log --redo-only $topdir/full
vlog "Log applied to full backup"

# Command should fail and print error message
run_cmd_expect_failure $IB_BIN $IB_ARGS --apply-log --redo-only --incremental-dir=$topdir/inc \
    $topdir/full
if ! grep -q "Cannot handle DDL operation" $OUTFILE
then
	die "Error message not found!"
fi
