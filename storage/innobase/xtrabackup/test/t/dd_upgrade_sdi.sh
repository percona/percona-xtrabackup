#
# restore backup of saved backup dir that had duplicate SDI and upgraded
# ie schema of DD tables will be dd_upgrade_8030
# prepare should succeed
require_server_version_higher_than 8.0.29

start_server
stop_server

function prepare_local() {
	xtrabackup --prepare --target-dir=$topdir/dd_upgrade_sdi
}
function restore() {
  [[ -d $mysql_datadir ]] && rm -rf $mysql_datadir && mkdir -p $mysql_datadir
  xtrabackup --copy-back --target-dir=$topdir/dd_upgrade_sdi
}

function do_test() {

  MYSQLD_START_TIMEOUT=1200

  mkdir -p $topdir

  tar -xf inc/dd_upgrade_sdi.tar.xz  -C $topdir

  eval $prepare_cmd

  eval $restore_cmd

  start_server

  MYSQL_EXTRA_ARGS="-uroot -ppassword"
  $MYSQL $MYSQL_ARGS $MYSQL_EXTRA_ARGS -Ns -e "show create table t1;" test
  $MYSQL $MYSQL_ARGS $MYSQL_EXTRA_ARGS -Ns -e "show create table pt1;" test

  stop_server
}


vlog "##############################"
vlog "# restore from 8.0.30 datadir with duplicate SDI"
vlog "# and upgraded from older server version"
vlog "##############################"

export prepare_cmd=prepare_local
export restore_cmd=restore
do_test
rm -rf $topdir/dd_upgrade_sdi
