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
xtrabackup --backup --target-dir=$topdir/full

vlog "Rotating the table"

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
RENAME TABLE t1 TO t12;
EOF

vlog "Creating incremental backup"

xtrabackup --backup --incremental-basedir=$topdir/full --target-dir=$topdir/inc

# remove space_id = something line from .meta file
sed -ie '/space_id/ d' $topdir/inc/test/t12.ibd.meta
vlog "Preparing backup"

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
vlog "Log applied to full backup"

# Command should fail and print error message
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --apply-log-only \
    --incremental-dir=$topdir/inc --target-dir=$topdir/full
if ! grep -q "Cannot handle DDL operation" $OUTFILE
then
	die "Error message not found!"
fi
