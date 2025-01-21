. inc/common.sh

remote_dir=$TEST_VAR_ROOT/var1/remote_dir/subdir
undo_dir=$TEST_VAR_ROOT/var1/undo_dir/subdir
mkdir -p $remote_dir
mkdir -p $undo_dir

start_server --innodb_file_per_table --innodb_directories="$remote_dir;$undo_dir"

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLESPACE TS1 ADD DATAFILE 'ts1.ibd';
CREATE TABLESPACE TS2 ADD DATAFILE '$remote_dir/ts2_name_mismatch.ibd';
CREATE TABLE t(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
 DATA DIRECTORY = '$remote_dir' ENGINE=InnoDB;
INSERT INTO t(c) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
CREATE TABLE b(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS1  ENGINE=InnoDB;
INSERT INTO b(c) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
CREATE TABLE c(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS2 ENGINE=InnoDB;
INSERT INTO c(c) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
EOF

mysql -e "CREATE TABLESPACE \`TS 3\` ADD DATAFILE '$remote_dir/ts2_name_space_mismatch.ibd'" test
mysql -e "CREATE TABLE c_with_space(id INT AUTO_INCREMENT PRIMARY KEY, c INT) TABLESPACE \`TS 3\` ENGINE=InnoDB" test
mysql -e "CREATE TABLE \`c 2 with space\`(id INT AUTO_INCREMENT PRIMARY KEY, c INT) ENGINE=InnoDB" test



for ((i=0; i<12; i++))
do
    $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  DATA DIRECTORY = '$remote_dir' ENGINE=InnoDB;
CREATE TABLE b$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS1 ENGINE=InnoDB;
CREATE TABLE c$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS2 ENGINE=InnoDB;
EOF
done

for ((i=0; i<12; i++))
do
    $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE p$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  DATA DIRECTORY = '$undo_dir' ENGINE=InnoDB;
CREATE TABLE r$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS1 ENGINE=InnoDB;
CREATE TABLE s$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS2 ENGINE=InnoDB;
EOF
done
vlog "Case#1 backup with  orphan ibd in datadir "
cp inc/skip_orpah_ibds/FTS_0000000000000123_BEING_DELETED.ibd  $mysql_datadir/test

run_cmd xtrabackup --backup --target-dir=${topdir}/backup 2>&1 | tee $topdir/pxb.log

record_db_state test

vlog "Ensure just single file is missing "
if [ $(grep 'Warning.*space.*does not exist' $topdir/pxb.log | wc -l) -eq 1 ]
    then
       echo "just 1 file missing"
    else
       die "not exactly 1 tablespace missing "
fi

vlog "Case#2 backup should copy FTS in backup with lock-ddl=OFF"

rm -rf ${topdir}/backup
run_cmd xtrabackup --backup --lock-ddl=OFF  --target-dir=${topdir}/backup

vlog "Case#2 prepare should work with orphan ibd copied"

run_cmd xtrabackup --prepare --target-dir=$topdir/backup --export 2>&1 | tee $topdir/pxb_prepare.log

# After PXB-2955, we dont load SDI by default, so orphan IBD without SDI cannot be detected
# If there is a rollback on the tablespace, PXB will open them and the empty SDI is detected
# It is hard to get a running trx on this FTS table. Since the orphan is detected at export,
# (we dont parse SDI yet), the error message is as below now
grep "cannot find dictionary record of table test/FTS_0000000000000123_BEING_DELETED" $topdir/pxb_prepare.log || die "missing warning during prepare"

stop_server
rm -rf $mysql_datadir/*
rm -rf $remote_dir
rm -rf $undo_dir

xtrabackup --move-back --target-dir=${topdir}/backup
start_server

verify_db_state test
