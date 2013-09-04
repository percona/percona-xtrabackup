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


CREATE TABLE isam_t1(a INT) ENGINE=MyISAM;
INSERT INTO isam_t1 VALUES (1), (2), (3);

CREATE TABLE isam_t2(a INT) ENGINE=MyISAM;
INSERT INTO isam_t2 VALUES (4), (5), (6);

CREATE TABLE isam_p (
  a int
) ENGINE=MyISAM
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (100),
 PARTITION p1 VALUES LESS THAN (200),
 PARTITION p2 VALUES LESS THAN (300),
 PARTITION p3 VALUES LESS THAN (400));

INSERT INTO isam_p VALUES (1), (101), (201), (301);

EOF

# Full backup
vlog "Creating full backup"
innobackupex  --no-timestamp $topdir/full

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


DROP TABLE isam_t1;
DROP TABLE isam_t2;
CREATE TABLE isam_t2(a INT) ENGINE=MyISAM;

INSERT INTO isam_t2 VALUES (40), (50), (60);

ALTER TABLE isam_p DROP PARTITION p0;
ALTER TABLE isam_p DROP PARTITION p1;
ALTER TABLE isam_p ADD PARTITION (PARTITION p4 VALUES LESS THAN (500));
ALTER TABLE isam_p ADD PARTITION (PARTITION p5 VALUES LESS THAN (600));

INSERT INTO isam_p VALUES (401), (501);

EOF

vlog "Creating incremental backup"

innobackupex --incremental --no-timestamp \
    --incremental-basedir=$topdir/full $topdir/inc

vlog "Preparing backup"

innobackupex --apply-log --redo-only $topdir/full
vlog "Log applied to full backup"

innobackupex --apply-log --redo-only --incremental-dir=$topdir/inc \
    $topdir/full
vlog "Delta applied to full backup"

innobackupex --apply-log $topdir/full
vlog "Data prepared for restore"

ls -al $topdir/full/test/*

# we expect to see
# 5 InnoDB tablespaces
count=`ls $topdir/full/test/*.ibd | wc -l`
vlog "$count .ibd in restore, expecting 5"
test $count -eq 5

# 5 MyISAM data files
count=`ls $topdir/full/test/*.MYD | wc -l`
vlog "$count .MYD in restore, expecting 5"
test $count -eq 5

# and 10 tables overall
count=`ls $topdir/full/test/*.frm | wc -l`
vlog "$count .frm in restore, expecting 4"
test $count -eq 4
