############################################################################
# Bug1394493: need warning when encountering TokuDB tables
############################################################################

. inc/common.sh

require_tokudb

start_server_with_tokudb

mysql <<EOF
CREATE DATABASE testdb;
CREATE TABLE testdb.t (a INT PRIMARY KEY) ENGINE=TokuDB;
INSERT INTO testdb.t VALUES (1), (2), (3);
EOF

vlog "Checking that warning is printed out"
xtrabackup --backup --target-dir=$topdir/backup 2>&1 | tee $topdir/pxb.log
if ! grep -q 'Warning: "testdb.t" uses engine "TokuDB" and will not be backed up.' $topdir/pxb.log
then
	die "xtrabackup should print a warning."
fi

run_cmd rm -rf $topdir/backup

vlog "Suppressing warning"
xtrabackup --backup --target-dir=$topdir/backup \
	--skip-tables-compatibility-check \
	2>&1 \
	| tee $topdir/pxb.log

if grep -q 'Warning: "testdb.t" uses engine "TokuDB" and will not be backed up.' $topdir/pxb.log
then
	die "xtrabackup should NOT print a warning."
fi

run_cmd rm -rf $topdir/backup

vlog "No warning when TokuDB table is not backed up"
xtrabackup --backup \
	--target-dir=$topdir/backup \
	--databases=foo 2>&1 \
	| tee $topdir/pxb.log

if grep -q 'Warning: "testdb.t" uses engine "TokuDB" and will not be backed up.' $topdir/pxb.log
then
	die "xtrabackup should NOT print a warning."
fi

# All done, reset an $? set to 1 by grep.
vlog "Done !"
