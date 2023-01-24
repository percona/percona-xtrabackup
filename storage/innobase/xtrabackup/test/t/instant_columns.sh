is_server_version_higher_than 8.0.31 && die "This test should be modified after 8.0.31"
require_debug_server
require_server_version_higher_than 8.0.28
# run instant tests.
# pass 1 as first parameter if we are expected to abort the backup
function run_instant_test()
{
  expect_failure=$1
  backup_folder=${topdir}/backup
  mkdir ${backup_folder}
  start_server

  # Test 1 - corruption after INSTANT DROP COLUMN
  echo "
CREATE TABLE t1 (
col1 VARCHAR(10) NOT NULL,
col2 char(13),
col3 char(10),
col4 char(10),
col5 char(10),
col6 varchar(13),
PRIMARY KEY (col1)
) ENGINE=InnoDB;
INSERT INTO t1  VALUES ('4545', '22', '52', '53','54', '56');
ALTER TABLE t1 DROP COLUMN col2, DROP COLUMN col3, DROP COLUMN col4, DROP COLUMN col5, LOCK=DEFAULT, ALGORITHM=INSTANT;
SET GLOBAL innodb_buf_flush_list_now=ON;
SET GLOBAL innodb_log_checkpoint_now=ON;
SET GLOBAL innodb_checkpoint_disabled=true;
SET GLOBAL innodb_page_cleaner_disabled_debug=true;
UPDATE t1 SET col6 = '57' WHERE col1 = '4545';" | $MYSQL $MYSQL_ARGS -Ns test

  if [[ "${expect_failure}" -eq 1 ]];
  then
    run_cmd_expect_failure xtrabackup --backup --target-dir=${backup_folder} 2>&1 | tee $topdir/pxb.log
    grep "Found tables with row versions due to INSTANT ADD/DROP columns" $topdir/pxb.log || die "missing error message"
    rm $topdir/pxb.log
  else
    xtrabackup --backup --target-dir=${backup_folder}
    xtrabackup --prepare --target-dir=${backup_folder}
    echo "SET GLOBAL innodb_checkpoint_disabled=false;
    SET GLOBAL innodb_page_cleaner_disabled_debug=false;" | $MYSQL $MYSQL_ARGS -Ns
    checksum1=`checksum_table test t1`
    stop_server
    rm -rf ${mysql_datadir}
    xtrabackup --move-back --target-dir=${backup_folder}
    start_server
    checksum2=`checksum_table test t1`
    $MYSQL $MYSQL_ARGS -s -e "select * from t1;" test

    vlog "Old checksum: $checksum1"
    vlog "New checksum: $checksum2"

    if [ "$checksum1" != "$checksum2" ]; then
    vlog "Checksums do not match"
    exit -1
    fi
  fi
  rm -rf ${backup_folder}
  echo "DROP TABLE t1" | $MYSQL $MYSQL_ARGS -Ns test

  # Test 2 - combine ADD/DROP in single ALTER
echo "CREATE TABLE t1 (
  col1 VARCHAR(10) NOT NULL,
  col2 char(13),
  col3 char(13),
  col5 char(13),
  col6 char(13),
  col7 char(13),
  col8 char(10),
  col9 char(10),
  PRIMARY KEY (col1)
) ENGINE=InnoDB;

INSERT INTO t1  VALUES ('1', '22', '33', '55', '66', '77', '88', '99');
ALTER TABLE t1 ADD COLUMN col4 char(5) AFTER col3, DROP COLUMN col7, LOCK=DEFAULT, ALGORITHM=INSTANT;
UPDATE t1 SET col4 = '44' WHERE col1 = '1';

SET GLOBAL innodb_buf_flush_list_now=ON;
SET GLOBAL innodb_log_checkpoint_now=ON;

SET GLOBAL innodb_checkpoint_disabled=true;
SET GLOBAL innodb_page_cleaner_disabled_debug=true;
UPDATE t1 SET col4 = '4', col6='6', col8='8' WHERE col1 = '1';" | $MYSQL $MYSQL_ARGS -Ns test

if [[ "${expect_failure}" -eq 1 ]];
then
  run_cmd_expect_failure xtrabackup --backup --target-dir=${backup_folder} 2>&1 | tee $topdir/pxb.log
  grep "Found tables with row versions due to INSTANT ADD/DROP columns" $topdir/pxb.log || die "missing error message"
  rm $topdir/pxb.log
else
  xtrabackup --backup --target-dir=${backup_folder}
  xtrabackup --prepare --target-dir=${backup_folder}
  echo "SET GLOBAL innodb_checkpoint_disabled=false;
  SET GLOBAL innodb_page_cleaner_disabled_debug=false;" | $MYSQL $MYSQL_ARGS -Ns
  checksum1=`checksum_table test t1`
  stop_server
  rm -rf ${mysql_datadir}
  xtrabackup --move-back --target-dir=${backup_folder}
  start_server
  checksum2=`checksum_table test t1`
  $MYSQL $MYSQL_ARGS -s -e "select * from t1;" test

  vlog "Old checksum: $checksum1"
  vlog "New checksum: $checksum2"

  if [ "$checksum1" != "$checksum2" ]; then
  vlog "Checksums do not match"
  exit -1
  fi
fi
rm -rf ${backup_folder}
echo "DROP TABLE t1" | $MYSQL $MYSQL_ARGS -Ns test
}

vlog "XTRADB: ${XTRADB_VERSION}"
if is_xtradb;
then
  run_instant_test 0
else
  run_instant_test 1
fi
