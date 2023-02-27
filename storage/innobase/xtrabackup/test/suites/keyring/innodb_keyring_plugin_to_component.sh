#
# Test upgrade of keyring file plugin to component
#
skip_test "Disabled until PXB-2969 is fixed"
if is_server_version_lower_than 8.0.24
then
  skip_test "Requires server version higher than 8.0.24"
fi

. inc/keyring_common.sh

function run_insert() {
  trap "finish=true" SIGUSR1
  finish="false"
  while [ $finish != "true" ] ; do
    run_cmd $MYSQL $MYSQL_ARGS test -e "INSERT INTO tb1 VALUES (NULL)"
    sleep 0.1
  done
}

vlog "-- testing keyring upgrade from early server version --"

KEYRING_TYPE="plugin"
. inc/keyring_file.sh

# start and stop the server to create all folder structures and variables.
start_server
stop_server

# restore 8.0.23 backup and prepare it
run_cmd tar -xf inc/innodb_keyring_plugin_backup.tar.gz  -C $topdir
cp -r $topdir/backup $topdir/full
prepare_options="--xtrabackup-plugin-dir=${plugin_dir} --keyring-file-data=${topdir}/keyring_file"
xtrabackup --prepare --target-dir=$topdir/backup ${prepare_options}
rm -rf ${mysql_datadir}
xtrabackup --copy-back --target-dir=$topdir/backup
mv $topdir/keyring_file ${keyring_file_plugin}
start_server
run_insert &
job_id=$!
sleep 2
kill -USR1 $job_id
wait $job_id
xtrabackup --backup --incremental-basedir=$topdir/full \
     --target-dir=$topdir/inc1
record_db_state test
stop_server

prepare_options="--xtrabackup-plugin-dir=${plugin_dir} --keyring-file-data=${keyring_file_plugin}"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full ${prepare_options}
xtrabackup --prepare --incremental-dir=$topdir/inc1 \
     --target-dir=$topdir/full ${prepare_options}
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full
start_server
run_cmd verify_db_state test
stop_server
rm -rf $topdir/{backup,full,inc1,keyring_file}
vlog "-- DONE testing keyring upgrade from early server version --"

# use the current datadir and try to take incremental while the server
# has upgraded from keyring plugin to component
vlog "-- testing keyring upgrade from plugin to component --"
start_server
run_insert &
job_id=$!
sleep 2
xtrabackup --backup --target-dir=$topdir/full
sleep 2
xtrabackup --backup --incremental-basedir=$topdir/full \
     --target-dir=$topdir/inc1
kill -USR1 $job_id
wait $job_id
stop_server
configure_keyring_file_component
# Copy required plugin file and components
mkdir $topdir/plugin
cp $MYSQL_BASEDIR/lib/plugin/keyring_file.so $MYSQL_BASEDIR/lib/plugin/component_keyring_file.so ${MYSQLD_DATADIR}/component_keyring_file.cnf $topdir/plugin

run_cmd ${MYSQLD} --basedir=${MYSQL_BASEDIR} --keyring-migration-to-component \
--keyring-migration-source=keyring_file.so \
--keyring-migration-destination=component_keyring_file.so \
--keyring-file-data=${TEST_VAR_ROOT}/keyring_file_plugin \
--plugin-dir=$topdir/plugin


MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_redo_log_encrypt
innodb_undo_log_encrypt
binlog-encryption
log-bin
"
start_server
run_insert &
job_id=$!
sleep 2
xtrabackup --backup --incremental-basedir=$topdir/inc1 \
     --target-dir=$topdir/inc2
sleep 2
kill -USR1 $job_id
wait $job_id
xtrabackup --backup --incremental-basedir=$topdir/inc2 \
     --target-dir=$topdir/inc3
record_db_state test
stop_server


prepare_options="--xtrabackup-plugin-dir=${plugin_dir} --keyring-file-data=${keyring_file_plugin}"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full ${prepare_options}
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
     --target-dir=$topdir/full ${prepare_options}

prepare_options="--xtrabackup-plugin-dir=${plugin_dir} --component-keyring-config=${keyring_component_cnf} --keyring-file-data=${keyring_file_component}"
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc2 \
    --target-dir=$topdir/full ${prepare_options}
xtrabackup --prepare --incremental-dir=$topdir/inc3 \
   --target-dir=$topdir/full ${prepare_options}
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full
configure_keyring_file_component
start_server
run_cmd verify_db_state test
stop_server
rm -rf $mysql_datadir
rm -rf $topdir/{full,inc1,inc2,inc3}
rm -rf ${keyring_file_component} ${keyring_file_plugin} $topdir/plugin
vlog "-- DONE testing keyring upgrade from plugin to component --"
