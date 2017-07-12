########################################################################
# Test remote tablespaces in InnoDB 5.6
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

start_server --innodb_file_per_table

remote_dir=$TEST_VAR_ROOT/var1/remote_dir/subdir

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  DATA DIRECTORY = '$remote_dir' ENGINE=InnoDB;
INSERT INTO t(c) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
EOF

for ((i=0; i<12; i++))
do
    $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  DATA DIRECTORY = '$remote_dir' ENGINE=InnoDB;
EOF
done

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
rm -rf $remote_dir

for ((i=0; i<12; i++))
do
    test -f $topdir/backup/test/t${i}.ibd || die "Remote tablespace is missing from backup!"
done

innobackupex --apply-log $topdir/backup

for cmd in "--copy-back" "--move-back" ; do

    innobackupex $cmd $topdir/backup

    if [ ! -f $remote_dir/test/t.ibd ] ; then
        vlog "Tablepace $remote_dir/t.ibd is missing!"
        exit -1
    fi

    for ((i=0; i<12; i++)) ; do
        if [ ! -f $remote_dir/test/t$i.ibd ] ; then
            vlog "Tablepace $remote_dir/t$i.ibd is missing!"
            exit -1
        fi
    done

    start_server

    checksum_b=`checksum_table test t`

    vlog "Old checksum: $checksum_a"
    vlog "New checksum: $checksum_b"

    if [ "$checksum_a" != "$checksum_b"  ]
    then
        vlog "Checksums do not match"
        exit -1
    fi

    stop_server

    rm -rf $mysql_datadir/*
    rm -rf $remote_dir

done
