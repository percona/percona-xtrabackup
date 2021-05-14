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
    copyback_options="--transition-key=$transition_key"
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

  xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc2 \
       --target-dir=$topdir/backup $prepare_options

  xtrabackup --prepare --export --target-dir=$topdir/backup \
       $prepare_options

  # check that stats works
  if [ -z ${plugin_load+x} ]; then
    plugin_load_arg=""
  else
    plugin_load_arg="--plugin-load=${plugin_load}"
  fi
  xtrabackup --stats --datadir=$topdir/backup ${plugin_load_arg}

  # make sure t1.ibd is still encrypted
  vlog "-- running strings "
  strings $topdir/backup/test/t1.ibd | ( grep -vq TWO || die "t1 is not encrypted" )

  record_db_state test

  # copy-back with existing master key
  stop_server

  rm -rf $mysql_datadir

  xtrabackup --copy-back --target-dir=$topdir/backup

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

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_encrypt
innodb_undo_log_encrypt
binlog-encryption
log-bin
"

