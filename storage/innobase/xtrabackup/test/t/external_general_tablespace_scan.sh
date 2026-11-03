########################################################################
# PXB-3858: Tablespace_map::scan() reads INNODB_TABLESPACES_BRIEF and
# excludes undo/reserved spaces by space-id (dict_sys_t::is_reserved).
#
# Verifies that an out-of-datadir general tablespace is backed up and
# restored to its ORIGINAL (external) path.
########################################################################

. inc/common.sh

# external general tablespaces (innodb_directories) require 8.0.21+
require_server_version_higher_than 8.0.20

ext_dir=${TEST_VAR_ROOT}/ext_dir
mkdir $ext_dir

start_server --innodb-directories="$ext_dir"

run_cmd $MYSQL $MYSQL_ARGS <<EOF
CREATE TABLESPACE ts_ext ADD DATAFILE '$ext_dir/ts_ext.ibd' ENGINE=InnoDB;
CREATE TABLE test.t_ext (c1 INT PRIMARY KEY) TABLESPACE ts_ext;
INSERT INTO test.t_ext VALUES (1), (2), (3);

CREATE TABLE test.t_in (c1 INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO test.t_in VALUES (10), (20), (30);
EOF

record_db_state test

xtrabackup --backup  --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

stop_server
rm -rf $mysql_datadir
rm -rf $ext_dir

xtrabackup --copy-back --target-dir=$topdir/backup
start_server --innodb-directories="$ext_dir/"

# PXB-2124: the external datafile must be restored to its original path,
# not relocated into the data directory.
[ -f "$ext_dir/ts_ext.ibd" ] \
    || die "ts_ext.ibd was not restored to the external directory"
[ ! -f "$mysql_datadir/ts_ext.ibd" ] \
    || die "ts_ext.ibd was wrongly restored into the data directory (PXB-2124)"

verify_db_state test
