########################################################################
# --prepare merges the dynamic metadata of a vector-indexed
# table (AUTO_INCREMENT and the vector label counter) like the server's
# crash recovery does (PS-11591), in a full backup and across
# incremental backups
#
# A table with a vector index and AUTO_INCREMENT keeps both counters in
# one row of mysql.innodb_dynamic_metadata. The redo records are partial:
# an UPDATE of the vector logs a new label without AUTO_INCREMENT, an
# INSERT with an explicit id logs AUTO_INCREMENT. --prepare must merge
# what it recovers into the buffered row, not replace it: a lost
# AUTO_INCREMENT makes the first INSERT after the restore fail with a
# duplicate key, a lost label counter hands out a label of the graph
# again.
#
# A clean restart writes the counters to the buffered row and
# checkpoints past their records, so the backup that follows sees only
# the records written after it.
#
# Full backup window (after a restart):
#   t1  label records only, explicit ids below AUTO_INCREMENT
#   t2  AUTO_INCREMENT record, then a DDL lowers AUTO_INCREMENT, then
#       label records only at the new table version
#   t4  AUTO_INCREMENT record only
#   t5  label records only
# A restart, then incremental 1 window:
#   t4  label records only
#   t5  AUTO_INCREMENT record only
#   t6  AUTO_INCREMENT record only
# No restart, incremental 2 window:
#   t6  label records only
#
# --apply-log-only does not write the recovered metadata, so across an
# incremental chain it must come back from the next backup: from the
# buffered row (t4, written by the restart) or from the redo of the next
# backup (t6, no checkpoint is forced in between).
#
# metadata_merge_debug.sh runs the same with checkpoints
# under control and checks the label counter exactly.
########################################################################

. inc/common.sh

start_server

require_vector_index

# Restart cleanly, so that the counters are in the buffered row, and keep
# the page cleaner from flushing the window that follows.
function restart_checkpointed()
{
  shutdown_server
  start_server
  $MYSQL $MYSQL_ARGS -Ns test -e "SET GLOBAL innodb_idle_flush_pct = 0"
}

# INSERT a row with AUTO_INCREMENT, check the id it gets and that it is
# found through the vector index. A label handed out twice fails the
# INSERT on the graph.
# @param $1 table
# @param $2 expected id
function check_insert()
{
  local got
  got=$($MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO $1 (k, v) VALUES (99, STRING_TO_VECTOR('[9,9,9,9]'));
           SELECT LAST_INSERT_ID()")
  vlog "$1: next AUTO_INCREMENT id $got, expected $2"
  [ "$got" = "$2" ] || die "$1: the next AUTO_INCREMENT id is $got, not $2"
  got=$($MYSQL $MYSQL_ARGS -Ns test -e "SELECT id FROM $1 FORCE INDEX (vk) ORDER BY
             DISTANCE(v, STRING_TO_VECTOR('[9,9,9,9]'), 'EUCLIDEAN') LIMIT 1")
  [ "$got" = "$2" ] || die "$1: the new row is not found through vk: $got"
}

for t in t1 t2 t4 t5 t6
do
  $MYSQL $MYSQL_ARGS -Ns test -e "CREATE TABLE $t (id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, k INT,
                      v VECTOR(4) NOT NULL,
                      VECTOR KEY vk (v) TYPE hnsw (M = 4)) ENGINE=InnoDB;
     INSERT INTO $t (k, v) VALUES
       (1, STRING_TO_VECTOR('[1,0,0,0]')), (2, STRING_TO_VECTOR('[0,1,0,0]')),
       (3, STRING_TO_VECTOR('[0,0,1,0]')), (4, STRING_TO_VECTOR('[0,0,0,1]'))"
done
# t1: AUTO_INCREMENT moves past the value a DDL stored.
$MYSQL $MYSQL_ARGS -Ns test -e "ALTER TABLE t1 ADD INDEX ik (k);
   INSERT INTO t1 (k, v) VALUES
     (5, STRING_TO_VECTOR('[1,1,0,0]')), (6, STRING_TO_VECTOR('[0,1,1,0]')),
     (7, STRING_TO_VECTOR('[0,0,1,1]'))"

restart_checkpointed

vlog "Full backup window"
# t1: label records only, explicit ids below AUTO_INCREMENT (8).
$MYSQL $MYSQL_ARGS -Ns test -e "DELETE FROM t1 WHERE id <= 2;
   INSERT INTO t1 (id, k, v) VALUES
     (1, 11, STRING_TO_VECTOR('[2,0,0,0]')), (2, 12, STRING_TO_VECTOR('[0,2,0,0]'))"
# t2: AUTO_INCREMENT 11 at the old version, lowered to 5 by a DDL, then
# label records only at the new version.
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO t2 (id, k, v) VALUES (10, 10, STRING_TO_VECTOR('[1,1,0,0]'));
   DELETE FROM t2 WHERE id = 10;
   ALTER TABLE t2 AUTO_INCREMENT = 5;
   DELETE FROM t2 WHERE id = 2;
   INSERT INTO t2 (id, k, v) VALUES (2, 12, STRING_TO_VECTOR('[0,2,0,0]'))"
# t4: AUTO_INCREMENT record only (51).
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO t4 (id, k, v) VALUES (50, 50, STRING_TO_VECTOR('[5,0,0,0]'));
   DELETE FROM t4 WHERE id = 50"
# t5: label records only.
$MYSQL $MYSQL_ARGS -Ns test -e "UPDATE t5 SET v = STRING_TO_VECTOR(CONCAT('[7,', id, ',7,7]')) WHERE id <= 2"

xtrabackup --backup --target-dir=$topdir/full

restart_checkpointed

vlog "Incremental 1 window"
# t4: label records only.
$MYSQL $MYSQL_ARGS -Ns test -e "UPDATE t4 SET v = STRING_TO_VECTOR(CONCAT('[7,', id, ',7,7]')) WHERE id <= 2"
# t5: AUTO_INCREMENT record only (201).
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO t5 (id, k, v) VALUES (200, 200, STRING_TO_VECTOR('[5,0,0,0]'));
   DELETE FROM t5 WHERE id = 200"
# t6: AUTO_INCREMENT record only (301).
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO t6 (id, k, v) VALUES (300, 300, STRING_TO_VECTOR('[5,0,0,0]'));
   DELETE FROM t6 WHERE id = 300"

xtrabackup --backup --target-dir=$topdir/inc1 --incremental-basedir=$topdir/full

vlog "Incremental 2 window"
# t6: label records only.
$MYSQL $MYSQL_ARGS -Ns test -e "UPDATE t6 SET v = STRING_TO_VECTOR(CONCAT('[7,', id, ',7,7]')) WHERE id <= 2"

xtrabackup --backup --target-dir=$topdir/inc2 --incremental-basedir=$topdir/inc1

stop_server

vlog "Restoring the full backup"
cp -a $topdir/full $topdir/full_only
xtrabackup --prepare --target-dir=$topdir/full_only
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full_only
start_server

check_insert t1 8
check_insert t2 5
check_insert t4 51
check_insert t5 5

stop_server

vlog "Restoring the incremental chain"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full \
  --incremental-dir=$topdir/inc1
xtrabackup --prepare --target-dir=$topdir/full --incremental-dir=$topdir/inc2
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full
start_server

check_insert t1 8
check_insert t2 5
check_insert t4 51
check_insert t5 201
check_insert t6 301
