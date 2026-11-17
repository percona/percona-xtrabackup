########################################################################
# A page-tracking incremental backup of a vector index built after the
# full backup
#
# ALTER TABLE ... ADD VECTOR KEY fills the aux table of the index without
# redo. The checkpoint does not move between the full backup and the
# incremental, so the incremental gets no changed pages from the server
# and takes the changes from its redo; it must copy the aux table in full.
########################################################################

. inc/common.sh
. inc/vector_index_rows.sh

require_debug_server

start_server
require_vector_index
mysql -e "INSTALL COMPONENT 'file://component_mysqlbackup'"

# The checkpoint passes the start of page tracking, then stays at the
# first page dirtied below: the page cleaner is stopped.
mysql -e "SELECT mysqlbackup_page_track_set(1)" >/dev/null
innodb_wait_for_flush_all
mysql -e "SET GLOBAL innodb_page_cleaner_disabled_debug = ON"

mysql test -e "CREATE TABLE t (id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
                               v VECTOR(4) NOT NULL) ENGINE=InnoDB"
insert_rows 200

xtrabackup --backup --page-tracking --target-dir=$topdir/full

mysql test -e "ALTER TABLE t ADD VECTOR KEY vk (v) TYPE hnsw"
insert_rows 5

xtrabackup --backup --page-tracking --incremental-basedir=$topdir/full \
           --target-dir=$topdir/inc
grep -q "incremental backup LSN is same as last checkpoint LSN" $OUTFILE ||
  die "The checkpoint moved after the full backup"

snapshot server

stop_server
rm -rf $mysql_datadir
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
xtrabackup --prepare --target-dir=$topdir/full --incremental-dir=$topdir/inc
xtrabackup --copy-back --target-dir=$topdir/full
start_server

snapshot restored
for f in rows index autoinc
do
  diff $topdir/server.$f $topdir/restored.$f ||
    die "The restored $f is not what the server had"
done
