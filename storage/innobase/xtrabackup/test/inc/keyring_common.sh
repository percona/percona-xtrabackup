#
# Basic test of InnoDB encryption support
#

require_server_version_higher_than 5.7.10

function test_do()
{
  table_options=$1
  transition_key=$2
  keyring_type=$3
  vlog "-- running test with: -- "
  vlog "-- table_options: ${table_options} --"
  vlog "-- transition_key: ${transition_key} --"
  vlog "-- keyring_type: ${keyring_type} --"
  if [[ "$transition_key" = "generate" ]] ; then
    backup_options="--generate-transition-key"
    prepare_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
    copyback_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
  elif [[ "$transition_key" = "none" ]] ; then
    backup_options=
    prepare_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
    copyback_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
  else
    backup_options="--transition-key=$transition_key"
    prepare_options="--transition-key=$transition_key"
    copyback_options="--transition-key=$transition_key --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
  fi

  if [[ "${KEYRING_TYPE}" = "plugin" ]]; then
    start_server
  else
    configure_server_with_component
  fi

  run_cmd $MYSQL $MYSQL_ARGS test -e "SELECT @@server_uuid"

  # PXB-1540: XB removes and recreate keyring file of 0 size
  xtrabackup --backup --target-dir=$topdir/backup0

  rm -rf $topdir/backup0

  run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1 (c1 VARCHAR(100)) ${table_options};
INSERT INTO t1 (c1) VALUES ('ONE'), ('TWO'), ('THREE');
INSERT INTO t1 (c1) VALUES ('10'), ('20'), ('30');
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
CREATE TABLE t2 (c1 VARCHAR(100)) ${table_options};
INSERT INTO t2 SELECT * FROM t1;
EOF

  # wait for InnoDB to flush all dirty pages
  innodb_wait_for_flush_all

  xtrabackup --backup --target-dir=$topdir/backup $backup_options

  cat $topdir/backup/backup-my.cnf

  run_cmd $MYSQL $MYSQL_ARGS test <<EOF
INSERT INTO t1 SELECT * FROM t1;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
INSERT INTO t1 SELECT * FROM t1;
CREATE TABLE t3 (c1 VARCHAR(100)) ${table_options};
INSERT INTO t3 SELECT * FROM t1;
DROP TABLE t2;
EOF

  # wait for InnoDB to flush all dirty pages
  innodb_wait_for_flush_all

  xtrabackup --backup --incremental-basedir=$topdir/backup \
       --target-dir=$topdir/inc1 $backup_options

  run_cmd $MYSQL $MYSQL_ARGS test <<EOF
INSERT INTO t1 SELECT * FROM t1;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
INSERT INTO t1 SELECT * FROM t1;
EOF

  # wait for InnoDB to flush all dirty pages
  innodb_wait_for_flush_all

  $MYSQL $MYSQL_ARGS -e 'START TRANSACTION; INSERT INTO t1 SELECT * FROM t1; SELECT SLEEP(200000);' test &
  uncommitted_id=$!

  sleep 3

  xtrabackup --backup --incremental-basedir=$topdir/inc1 \
       --target-dir=$topdir/inc2 $backup_options

  kill -SIGKILL $uncommitted_id

  xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup \
       $prepare_options
  xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
       --target-dir=$topdir/backup $prepare_options

  xtrabackup --prepare --incremental-dir=$topdir/inc2 \
       --target-dir=$topdir/backup $prepare_options

  xtrabackup --prepare --export --target-dir=$topdir/backup \
       $prepare_options

  # check that stats works
  if [ -z ${plugin_load+x} ]; then
    plugin_load_arg=""
  else
    plugin_load_arg="--plugin-load=${plugin_load}"
  fi
 # xtrabackup --stats --datadir=$topdir/backup ${plugin_load_arg}

  # make sure t1.ibd is still encrypted
  vlog "-- running strings "
  strings $topdir/backup/test/t1.ibd | ( grep -vq TWO || die "t1 is not encrypted" )

  record_db_state test

  # copy-back with existing master key
  stop_server

  rm -rf $mysql_datadir

  xtrabackup --copy-back --target-dir=$topdir/backup

if [[ "${KEYRING_TYPE}" = "component" ]] || [[ "${KEYRING_TYPE}" = "both" ]]; then
  cp ${instance_local_manifest}  $mysql_datadir
  cp ${keyring_component_cnf} $mysql_datadir
fi

  start_server

  verify_db_state test

  if [[ "$transition_key" != "none" ]]; then

    # copy-back with new master key
    stop_server

    rm -rf $mysql_datadir

    test "$transition_key" = "generate" || cleanup_keyring

    xtrabackup --copy-back --generate-new-master-key \
         $copyback_options \
         --target-dir=$topdir/backup

    # PXB-2874 - Check tablespace encryption uuid and key id
    FILE=$mysql_datadir/test/t1.ibd
    MASTER_KEY_ID=$(xxd -s 10393 -l4 $FILE |  awk '{print $3}')

    if [[ "${MASTER_KEY_ID}" != "0001" ]];
    then
      die "File: ${FILE} has master key id: ${MASTER_KEY_ID} while it should be 0001"
    fi

    NEW_UUID=$(xxd -p -s 10397 -l36 $FILE | xxd -r -p | tr -d '\0')
    OLD_UUID=$(xxd -p -s 10397 -l36 $topdir/backup/test/t1.ibd | xxd -r -p | tr -d '\0')

    if [[ "${NEW_UUID}" = "${OLD_UUID}" ]];
    then
      die "File: ${FILE} has same UUID as backup file: ${NEW_UUID}. PXB should have generated a new UUID"
    fi

    if grep -q "Could not find the data corresponding to Data ID: 'MySQLReplicationKey__1'" $OUTFILE
    then
      die "Cannot read uuid from backup-my.cnf file"
    fi

    if [[ "${KEYRING_TYPE}" = "component" ]] || [[ "${KEYRING_TYPE}" = "both" ]];
    then
      vlog "copying component config back to datadir"
      cp ${instance_local_manifest}  $mysql_datadir
      cp ${keyring_component_cnf} $mysql_datadir
    fi
    start_server

    verify_db_state test
  fi

  stop_server

  rm -rf $mysql_datadir
  rm -rf $topdir/{backup,inc1,inc2}
  cleanup_keyring
  vlog "-- finished test with: -- "
  vlog "-- table_options: ${table_options} --"
  vlog "-- transition_key: ${transition_key} --"
  vlog "-- keyring_type: ${keyring_type} --"
}

function configure_server_with_component()
{
  if [ -f "${MYSQLD_PIDFILE}" ];
  then
    stop_server
  fi

  rm -rf $mysql_datadir
  # initialize db without keyring and extra config. In case we have options like
  # encrypt_redo_log we cannot run --initialize with local component config
  # present at datadir.
  MYSQLD_EXTRA_MY_CNF_OPTS_TMP=${MYSQLD_EXTRA_MY_CNF_OPTS}
  MYSQLD_EXTRA_MY_CNF_OPTS=""
  start_server
  stop_server
  #enable keyring component
  configure_keyring_file_component
  MYSQLD_EXTRA_MY_CNF_OPTS=${MYSQLD_EXTRA_MY_CNF_OPTS_TMP}
  start_server
  innodb_wait_for_flush_all
}

function keyring_extra_tests()
{
  #
  # PXB-2216 - Backup changing table encryption version
  #
  MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS:-""}
  innodb-buffer-pool-size=536870912
  innodb-log-file-size=536870912
  "

  XB_EXTRA_MY_CNF_OPTS="${XB_EXTRA_MY_CNF_OPTS:-""}
  xtrabackup-plugin-dir=${plugin_dir}
  "
  prepare_options="--xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}"
  if [[ "${KEYRING_TYPE}" = "plugin" ]]; then
    start_server
  else
    configure_server_with_component
  fi
  run_cmd $MYSQL $MYSQL_ARGS test -e "SELECT @@server_uuid"
  run_cmd $MYSQL $MYSQL_ARGS test -e "CREATE TABLE tb1 (ID INT PRIMARY KEY) ENCRYPTION='Y'"
  for i in $(seq 1 10);
  do
    run_cmd $MYSQL $MYSQL_ARGS test -e "INSERT INTO tb1 VALUES (${i})"
  done
  run_cmd $MYSQL $MYSQL_ARGS test -e "CREATE TABLE tb2 (ID INT PRIMARY KEY) ENCRYPTION='Y'"
  xtrabackup --backup --target-dir=$topdir/backup1
  record_db_state test
  stop_server
  rm -rf $mysql_datadir
  cp -R $topdir/backup1 $topdir/backup2

  # Write some garbage to magic section and rewrite checksum
  # Prepare must catch and fail.
  printf '\xdc\x67\xf3' | dd of=$topdir/backup2/test/tb2.ibd bs=1 seek=10390 count=3 conv=notrunc
  $MYSQL_BASEDIR/bin/innochecksum -w crc32 --no-check $topdir/backup2/test/tb2.ibd
  run_cmd_expect_failure $XB_BIN $XB_ARGS \
  --prepare --target-dir=$topdir/backup2 ${prepare_options}
  if ! grep -q "Failed to decrypt encryption information, found unexpected version of it!" $OUTFILE
  then
      die "Cannot find 'read unknown ENCRYPTION_KEY_MAGIC' message on xtrabackup log"
  fi


  xtrabackup --prepare --target-dir=$topdir/backup1 ${prepare_options}
  xtrabackup --copy-back --target-dir=$topdir/backup1
  if [[ "${KEYRING_TYPE}" = "component" ]] || [[ "${KEYRING_TYPE}" = "both" ]];
  then
    cp ${instance_local_manifest}  $mysql_datadir
    cp ${keyring_component_cnf} $mysql_datadir
  fi
  start_server
  run_cmd verify_db_state test
  stop_server
  rm -rf $mysql_datadir
  rm -rf $topdir/{backup1,backup2}
  cleanup_keyring
  # cleanup environment variables
  MYSQLD_EXTRA_MY_CNF_OPTS="
  innodb_redo_log_encrypt
  innodb_undo_log_encrypt
  binlog-encryption
  log-bin
  "
  XB_EXTRA_MY_CNF_OPTS=
}

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_encrypt
innodb_undo_log_encrypt
binlog-encryption
log-bin
"

