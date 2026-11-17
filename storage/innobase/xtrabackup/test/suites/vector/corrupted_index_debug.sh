########################################################################
# --prepare keeps the corrupted flag of an index together with
# AUTO_INCREMENT
#
# A table's row in mysql.innodb_dynamic_metadata (the buffered row) holds
# both AUTO_INCREMENT and the ids of its corrupted indexes. Each redo
# record holds one of them. --prepare merges the records into the
# buffered row: a record of one kind must not drop the other kind.
#
# CHECK TABLE with dict_set_index_corrupted marks the secondary index ik
# corrupted; a read through it then fails. A clean restart writes the
# metadata to the buffered row and checkpoints past its records.
#
#   c1  ik corrupted before the restart, AUTO_INCREMENT record (51) in
#       the backup
#   c2  AUTO_INCREMENT (51) before the restart, ik corrupted in the
#       backup
#   c3  both in the backup
#   c4  AUTO_INCREMENT record (51) in the backup, ik not corrupted
########################################################################

. inc/common.sh

require_debug_server

start_server

# Restart cleanly, so that the metadata is in the buffered row, and keep
# the page cleaner from flushing the window that follows.
function restart_checkpointed()
{
  shutdown_server
  start_server
  $MYSQL $MYSQL_ARGS -Ns test -e "SET GLOBAL innodb_idle_flush_pct = 0"
}

# Mark the secondary index ik of a table corrupted.
# @param $1 table
function corrupt_index()
{
  $MYSQL $MYSQL_ARGS -Ns test -e "SET SESSION debug = '+d,dict_set_index_corrupted';
      CHECK TABLE $1;
      SET SESSION debug = '-d,dict_set_index_corrupted'" >/dev/null
}

# Move AUTO_INCREMENT of a table to 51.
# @param $1 table
function autoinc_51()
{
  $MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO $1 (id, k) VALUES (50, 50);
      DELETE FROM $1 WHERE id = 50"
}

# Check whether a read through ik fails as corrupted.
# @param $1 table
# @param $2 yes or no
function check_corrupted()
{
  local out got=no
  out=$($MYSQL $MYSQL_ARGS -Ns test -e "SELECT COUNT(*) FROM $1 FORCE INDEX (ik)
      WHERE k > 0" 2>&1) || got=yes
  vlog "$1: ik corrupted: $got ($out)"
  [ "$got" = "$2" ] || die "$1: ik corrupted is $got, not $2: $out"
  if [ "$got" = yes ]
  then
    echo "$out" | grep -qi corrupt || die "$1: the read failed otherwise: $out"
  fi
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

for t in c1 c2 c3 c4
do
  $MYSQL $MYSQL_ARGS -Ns test -e "CREATE TABLE $t (
      id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, k INT, KEY ik (k))
      ENGINE=InnoDB;
    INSERT INTO $t (k) VALUES (1), (2), (3), (4)"
done
corrupt_index c1
autoinc_51 c2

restart_checkpointed

vlog "Backup window"
autoinc_51 c1
corrupt_index c2
corrupt_index c3
autoinc_51 c3
autoinc_51 c4

for t in c1 c2 c3
do
  check_corrupted $t yes
done
check_corrupted c4 no

xtrabackup --backup --target-dir=$topdir/backup

stop_server

xtrabackup --prepare --target-dir=$topdir/backup
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup
start_server

for t in c1 c2 c3
do
  check_corrupted $t yes
  check_insert $t 51
done
check_corrupted c4 no
check_insert c4 51
