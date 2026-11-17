########################################################################
# --prepare keeps AUTO_INCREMENT of ordinary tables, in a full
# backup and across incremental backups
#
# --prepare merges the dynamic metadata it recovers from the redo log
# into the row of the table in mysql.innodb_dynamic_metadata (the
# buffered row) instead of replacing that row. This checks that the
# merge keeps AUTO_INCREMENT of tables without a vector index as before.
#
# A clean restart writes AUTO_INCREMENT to the buffered row and
# checkpoints past its redo records, so the backup that follows sees
# only the records written after it.
#
# Full backup window (after a restart):
#   a1  an AUTO_INCREMENT record (51)
#   a2  an AUTO_INCREMENT record (11), then a DDL lowers AUTO_INCREMENT
#       to 5: the newer table version wins
#   a3  nothing: AUTO_INCREMENT (71) is only in the buffered row
# A restart, then incremental 1 window:
#   a4  an AUTO_INCREMENT record (201)
# No restart, incremental 2 window:
#   a5  an AUTO_INCREMENT record (301)
#
# corrupted_index_debug.sh checks the corrupted index flag, the
# other dynamic metadata of an ordinary table.
########################################################################

. inc/common.sh

start_server

# Restart cleanly, so that AUTO_INCREMENT is in the buffered row, and
# keep the page cleaner from flushing the window that follows.
function restart_checkpointed()
{
  shutdown_server
  start_server
  $MYSQL $MYSQL_ARGS -Ns test -e "SET GLOBAL innodb_idle_flush_pct = 0"
}

# INSERT a row with AUTO_INCREMENT and check the id it gets.
# @param $1 table
# @param $2 expected id
function check_insert()
{
  local got
  got=$($MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO $1 (k) VALUES (99);
      SELECT LAST_INSERT_ID()")
  vlog "$1: next AUTO_INCREMENT id $got, expected $2"
  [ "$got" = "$2" ] || die "$1: the next AUTO_INCREMENT id is $got, not $2"
}

for t in a1 a2 a3 a4 a5
do
  $MYSQL $MYSQL_ARGS -Ns test -e "CREATE TABLE $t (
      id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, k INT) ENGINE=InnoDB;
    INSERT INTO $t (k) VALUES (1), (2), (3), (4)"
done
# a3: AUTO_INCREMENT 71, written to the buffered row by the restart.
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO a3 (id, k) VALUES (70, 70);
    DELETE FROM a3 WHERE id = 70"

restart_checkpointed

vlog "Full backup window"
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO a1 (id, k) VALUES (50, 50);
    DELETE FROM a1 WHERE id = 50"
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO a2 (id, k) VALUES (10, 10);
    DELETE FROM a2 WHERE id = 10;
    ALTER TABLE a2 AUTO_INCREMENT = 5"

xtrabackup --backup --target-dir=$topdir/full

restart_checkpointed

vlog "Incremental 1 window"
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO a4 (id, k) VALUES (200, 200);
    DELETE FROM a4 WHERE id = 200"

xtrabackup --backup --target-dir=$topdir/inc1 --incremental-basedir=$topdir/full

vlog "Incremental 2 window"
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO a5 (id, k) VALUES (300, 300);
    DELETE FROM a5 WHERE id = 300"

xtrabackup --backup --target-dir=$topdir/inc2 --incremental-basedir=$topdir/inc1

stop_server

vlog "Restoring the full backup"
cp -a $topdir/full $topdir/full_only
xtrabackup --prepare --target-dir=$topdir/full_only
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full_only
start_server

check_insert a1 51
check_insert a2 5
check_insert a3 71
check_insert a4 5
check_insert a5 5

stop_server

vlog "Restoring the incremental chain"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full \
  --incremental-dir=$topdir/inc1
xtrabackup --prepare --target-dir=$topdir/full --incremental-dir=$topdir/inc2
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full
start_server

check_insert a1 51
check_insert a2 5
check_insert a3 71
check_insert a4 201
check_insert a5 301
