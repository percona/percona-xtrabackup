########################################################################
# Bug #1130627: Can't backup individual partitions
########################################################################

. inc/common.sh
. inc/ib_part.sh

MYSQLD_EXTRA_MY_CNF_OPTS="innodb-file-per-table"

start_server

require_partitioning

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t_innodb (
  i INT NOT NULL AUTO_INCREMENT,
  c CHAR(10) DEFAULT NULL,
  PRIMARY KEY (i)
) ENGINE=InnoDB
/*!50100 PARTITION BY HASH (i)
PARTITIONS 10 */;
EOF

force_checkpoint

# Test that specifying partitions with --tables works
xtrabackup --backup --tables='^test.*#p#p5' --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

file_cnt=`ls $topdir/backup/test | wc -l`

test "$file_cnt" -eq 1

test -f $topdir/backup/test/t_innodb#p#p5.ibd

rm -rf $topdir/backup

# Test that specifying partitions with --databases works

xtrabackup --backup \
    --databases='test.t_innodb#p#p5' --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

test "$file_cnt" -eq 1
test -f $topdir/backup/test/t_innodb#p#p5.ibd

rm -rf $topdir/backup

# Test that specifying partitions with --tables-file works
cat >$topdir/tables_file <<EOF
test.t_innodb#p#p5
EOF
xtrabackup --backup --tables-file=$topdir/tables_file --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

test "$file_cnt" -eq 1

rm -rf $topdir/backup
