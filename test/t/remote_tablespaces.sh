########################################################################
# Test remote tablespaces in InnoDB 5.6
########################################################################

. inc/common.sh

if [ ${MYSQL_VERSION:0:3} != "5.6" ]
then
    echo "Requires a 5.6 server" > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

start_server --innodb_file_per_table

remote_dir=$TEST_BASEDIR/var1/remote_dir

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  DATA DIRECTORY = '$remote_dir' ENGINE=InnoDB;
INSERT INTO t(c) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
EOF

# Generate some log data, as we also want to test recovery of remote tablespaces
for ((i=0; i<12; i++))
do
    $MYSQL $MYSQL_ARGS test <<EOF
      INSERT INTO t(c) SELECT c FROM t;
EOF
done

checksum_a=`checksum_table test t`

innobackupex --no-timestamp $topdir/backup

stop_server

rm -rf $mysql_datadir/*

innobackupex --apply-log $topdir/backup
innobackupex --copy-back $topdir/backup

start_server

checksum_b=`checksum_table test t`

vlog "Old checksum: $checksum_a"
vlog "New checksum: $checksum_b"

if [ "$checksum_a" != "$checksum_b"  ]
then
    vlog "Checksums do not match"
    exit -1
fi
