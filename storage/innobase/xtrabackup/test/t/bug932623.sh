########################################################################
# Bug #932623: RENAME TABLE causes incremental prepare to fail
########################################################################

. inc/common.sh

start_server --innodb_file_per_table

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1_old(a INT) ENGINE=InnoDB;
INSERT INTO t1_old VALUES (1), (2), (3);

CREATE TABLE t1(a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (4), (5), (6);
EOF

# Full backup
# backup root directory
vlog "Starting backup"
xtrabackup --backup --target-dir=$topdir/full

vlog "Rotating the table"

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1_new(a int) ENGINE=InnoDB;
INSERT INTO t1_new VALUES (7), (8), (9);

CREATE DATABASE db2;
RENAME TABLE t1_old TO db2.t1;

RENAME TABLE t1 TO t1_old;
RENAME TABLE t1_new TO t1;

INSERT INTO t1_old VALUES (10), (11), (12);
INSERT INTO t1 VALUES (13), (14), (15);

EOF

vlog "Creating incremental backup"

xtrabackup --backup --incremental-basedir=$topdir/full --target-dir=$topdir/inc

vlog "Preparing backup"

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
vlog "Log applied to full backup"

xtrabackup --prepare --apply-log-only \
    --incremental-dir=$topdir/inc --target-dir=$topdir/full
vlog "Delta applied to full backup"

xtrabackup --prepare --target-dir=$topdir/full
vlog "Data prepared for restore"

checksum_t1_a=`checksum_table test t1`
checksum_t1_old_a=`checksum_table test t1_old`
checksum_t1_db2_a=`checksum_table db2 t1`

# Destroying mysql data
stop_server
rm -rf $mysql_datadir/*
vlog "Data destroyed"

# Restore backup
vlog "Copying files"

xtrabackup --copy-back --target-dir=$topdir/full
vlog "Data restored"

start_server --innodb_file_per_table

vlog "Checking checksums"
checksum_t1_b=`checksum_table test t1`
checksum_t1_old_b=`checksum_table test t1_old`
checksum_t1_db2_b=`checksum_table db2 t1`

if [ "$checksum_t1_a" != "$checksum_t1_b" -o "$checksum_t1_old_a" != "$checksum_t1_old_b" \
	-o "$checksum_t1_db2_a" != "$checksum_t1_db2_b" ]
then 
	vlog "Checksums do not match"
	exit -1
fi

vlog "Checksums are OK"
