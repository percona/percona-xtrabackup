########################################################################
# Bug #856400: RENAME TABLE causes incremental prepare to fail
########################################################################

. inc/common.sh
. inc/ib_part.sh

start_server --innodb_file_per_table

require_partitioning

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1(a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1), (2), (3);

CREATE TABLE t2(a INT) ENGINE=InnoDB;
INSERT INTO t2 VALUES (4), (5), (6);

CREATE TABLE p (
  a int
) ENGINE=InnoDB
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (100),
 PARTITION p1 VALUES LESS THAN (200),
 PARTITION p2 VALUES LESS THAN (300),
 PARTITION p3 VALUES LESS THAN (400));

INSERT INTO p VALUES (1), (101), (201), (301);

EOF

# Full backup
vlog "Creating full backup"
xtrabackup --backup --target-dir=$topdir/full

vlog "Making changes"

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

DROP TABLE t1;

DROP TABLE t2;
CREATE TABLE t2(a INT) ENGINE=InnoDB;
INSERT INTO t2 VALUES (40), (50), (60);

ALTER TABLE p DROP PARTITION p0;
ALTER TABLE p DROP PARTITION p1;
ALTER TABLE p ADD PARTITION (PARTITION p4 VALUES LESS THAN (500));
ALTER TABLE p ADD PARTITION (PARTITION p5 VALUES LESS THAN (600));

INSERT INTO p VALUES (401), (501);

EOF

vlog "Creating incremental backup"

xtrabackup --backup --incremental-basedir=$topdir/full --target-dir=$topdir/inc

vlog "Preparing backup"

xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
vlog "Log applied to full backup"

xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc \
    --target-dir=$topdir/full
vlog "Delta applied to full backup"

xtrabackup --prepare --target-dir=$topdir/full
vlog "Data prepared for restore"

ls -al $topdir/full/test/*

# we expect to see
# 5 InnoDB tablespaces
count=`ls $topdir/full/test/*.ibd | wc -l`
vlog "$count .ibd in restore, expecting 5"
test $count -eq 5
