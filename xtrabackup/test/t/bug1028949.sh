. inc/common.sh

[ -n "$INNODB_VERSION" ] || skip_test "Requires InnoDB plugin or XtraDB"

function test_bug_1028949()
{
  mysqld_additional_args="--innodb_file_per_table --innodb_strict_mode \
--innodb_file_format=Barracuda"
  
  start_server ${mysqld_additional_args}

  load_dbase_schema incremental_sample

  # Full backup

  # Full backup folder
  rm -rf $topdir/data/full
  mkdir -p $topdir/data/full
  # Incremental data
  rm -rf $topdir/data/delta
  mkdir -p $topdir/data/delta

  vlog "Starting backup"

  xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/data/full \
      $mysqld_additional_args

  vlog "Full backup done"

  # Changing data in sakila

  vlog "Making changes to database"

  for i in $PAGE_SIZES;
  do
      ${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t${i} (a INT(11) DEFAULT NULL, \
 number INT(11) DEFAULT NULL) ENGINE=INNODB\
 ROW_FORMAT=compressed KEY_BLOCK_SIZE=$i" incremental_sample
      ${MYSQL} ${MYSQL_ARGS} -e "INSERT INTO t${i} VALUES (1, 1)" incremental_sample
  done
  
  vlog "Changes done"

  for i in $PAGE_SIZES;
  do
      # Saving the checksum of original table
      checksum_a[$i]=`checksum_table incremental_sample t$i`

      vlog "Table 't${i}' checksum is ${checksum_a[$i]}"
  done

  vlog "Making incremental backup"

  # Incremental backup
  xtrabackup --datadir=$mysql_datadir --backup \
      --target-dir=$topdir/data/delta --incremental-basedir=$topdir/data/full \
      $mysqld_additional_args

  vlog "Incremental backup done"
  vlog "Preparing backup"

  # Prepare backup
  xtrabackup --datadir=$mysql_datadir --prepare --apply-log-only \
      --target-dir=$topdir/data/full $mysqld_additional_args
  vlog "Log applied to backup"

  xtrabackup --datadir=$mysql_datadir --prepare --apply-log-only \
      --target-dir=$topdir/data/full --incremental-dir=$topdir/data/delta \
      $mysqld_additional_args
  vlog "Delta applied to backup"

  xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/data/full \
      $mysqld_additional_args
  vlog "Data prepared for restore"

  # removing rows
  for i in $PAGE_SIZES;
  do
      ${MYSQL} ${MYSQL_ARGS} -e "delete from t$i;" incremental_sample
  done

  vlog "Tables cleared"

  # Restore backup

  stop_server

  vlog "Copying files"

  cd $topdir/data/full/
  cp -r * $mysql_datadir
  cd -

  vlog "Data restored"

  start_server ${mysqld_additional_args}

  vlog "Checking checksums"
  for i in $PAGE_SIZES
  do
      checksum_b[$i]=`checksum_table incremental_sample t$i`

      if [ "${checksum_a[$i]}" != "${checksum_b[$i]}"  ]
      then 
	  vlog "Checksums of table 't$i' are not equal"
	  exit -1
      fi
  done

  vlog "Checksums are OK"

  stop_server
}

PAGE_SIZES="1 2 4 8 16"
test_bug_1028949
