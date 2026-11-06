########################################################################
# --prepare merges the dynamic metadata of a vector-indexed
# table (AUTO_INCREMENT and the vector label counter) like the server's
# crash recovery does (PS-11591), in a full backup and across
# incremental backups
#
# The server tests counter_crash_* of PS-11591 kill the server with
# checkpoints disabled, so that the redo after the last checkpoint holds
# only some kinds of metadata records. Here a backup takes the place of
# the kill: its redo starts at the same checkpoint.
#
# Full backup window (from a checkpoint, checkpoints disabled):
#   t1  label records only, below a checkpointed AUTO_INCREMENT
#   t2  AUTO_INCREMENT record, then a DDL lowers AUTO_INCREMENT, then
#       label records only at the new table version
#   t3  the buffered row is newer than the redo (DDL + eviction)
#   t4  AUTO_INCREMENT record only
#   t5  label records only
# A checkpoint, then incremental 1 window (checkpoints disabled):
#   t4  label records only
#   t5  AUTO_INCREMENT record only
#   t6  AUTO_INCREMENT record only
# No checkpoint, incremental 2 window:
#   t6  label records only
#
# --apply-log-only does not write the recovered metadata, so across an
# incremental chain it must come back from the next backup: from the
# buffered row (checkpoint between the backups) or from the redo of the
# next backup (no checkpoint in between).
#
# Needs a debug server to control checkpoints and read the label counter;
# metadata_merge.sh covers the same on a release server.
########################################################################

. inc/common.sh

require_debug_server

start_server

require_vector_index

# Hand out the next vector label of a table and print it ("id=N").
function next_label()
{
  $MYSQL $MYSQL_ARGS -Ns test -e "SET SESSION innodb_interpreter = 'vec_next_id test/$1';
     SELECT @@session.innodb_interpreter_output"
}

# Check the id the next AUTO_INCREMENT INSERT into a table gets.
# @param $1 table
# @param $2 expected id
function check_autoinc()
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

# Check the next vector label of a table.
# @param $1 table
# @param $2 expected label ("id=N")
function check_label()
{
  local got
  got=$(next_label $1)
  vlog "$1: next label $got, expected $2"
  [ "$got" = "$2" ] || die "$1: the next label is $got, not $2"
}

for t in t1 t2 t3 t4 t5 t6
do
  $MYSQL $MYSQL_ARGS -Ns test -e "CREATE TABLE $t (id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, k INT,
                      v VECTOR(4) NOT NULL,
                      VECTOR KEY vk (v) TYPE hnsw (M = 4)) ENGINE=InnoDB;
     INSERT INTO $t (k, v) VALUES
       (1, STRING_TO_VECTOR('[1,0,0,0]')), (2, STRING_TO_VECTOR('[0,1,0,0]')),
       (3, STRING_TO_VECTOR('[0,0,1,0]')), (4, STRING_TO_VECTOR('[0,0,0,1]'))"
done

# t1: AUTO_INCREMENT moves past the value a DDL stored and is
# checkpointed below.
$MYSQL $MYSQL_ARGS -Ns test -e "ALTER TABLE t1 ADD INDEX ik (k);
   INSERT INTO t1 (k, v) VALUES
     (5, STRING_TO_VECTOR('[1,1,0,0]')), (6, STRING_TO_VECTOR('[0,1,1,0]')),
     (7, STRING_TO_VECTOR('[0,0,1,1]'))"

$MYSQL $MYSQL_ARGS -Ns test -e "SET GLOBAL innodb_log_checkpoint_now = ON;
   SET GLOBAL innodb_checkpoint_disabled = ON"

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
# t3: label records at the old version; the DDL bumps the version and
# the eviction writes the buffered row at the new version.
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO t3 (k, v) VALUES (5, STRING_TO_VECTOR('[1,1,0,0]'));
   ALTER TABLE t3 AUTO_INCREMENT = 100;
   FLUSH TABLES t3;
   SET GLOBAL debug = '+d,innodb_evict_autoinc_table'"
$MYSQL $MYSQL_ARGS -Ns test -e "SET GLOBAL innodb_ft_aux_table = 'test/t3'" 2>/dev/null &&
  die "Evicting t3 did not report the expected error"
$MYSQL $MYSQL_ARGS -Ns test -e "SET GLOBAL debug = '-d,innodb_evict_autoinc_table'"
# t4: AUTO_INCREMENT record only (51).
$MYSQL $MYSQL_ARGS -Ns test -e "INSERT INTO t4 (id, k, v) VALUES (50, 50, STRING_TO_VECTOR('[5,0,0,0]'));
   DELETE FROM t4 WHERE id = 50"
# t5: label records only.
$MYSQL $MYSQL_ARGS -Ns test -e "UPDATE t5 SET v = STRING_TO_VECTOR(CONCAT('[7,', id, ',7,7]')) WHERE id <= 2"

xtrabackup --backup --target-dir=$topdir/full

# The labels a restore of the full backup must continue from. Taking
# them uses them up on the server, which the incremental backups see.
full_label_t1=$(next_label t1)
full_label_t2=$(next_label t2)
full_label_t3=$(next_label t3)
full_label_t5=$(next_label t5)

$MYSQL $MYSQL_ARGS -Ns test -e "SET GLOBAL innodb_checkpoint_disabled = OFF;
   SET GLOBAL innodb_log_checkpoint_now = ON;
   SET GLOBAL innodb_checkpoint_disabled = ON"

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
# t6: label records only; no checkpoint since incremental 1.
$MYSQL $MYSQL_ARGS -Ns test -e "UPDATE t6 SET v = STRING_TO_VECTOR(CONCAT('[7,', id, ',7,7]')) WHERE id <= 2"

xtrabackup --backup --target-dir=$topdir/inc2 --incremental-basedir=$topdir/inc1

for t in t1 t2 t3 t4 t5 t6
do
  eval "inc_label_$t=\$(next_label $t)"
done

$MYSQL $MYSQL_ARGS -Ns test -e "SET GLOBAL innodb_checkpoint_disabled = OFF"
stop_server

vlog "Restoring the full backup"
cp -a $topdir/full $topdir/full_only
xtrabackup --prepare --target-dir=$topdir/full_only
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full_only
start_server

# The labels first: the INSERTs below take labels of their own.
check_label t1 "$full_label_t1"
check_label t2 "$full_label_t2"
check_label t3 "$full_label_t3"
check_label t5 "$full_label_t5"
check_autoinc t1 8
check_autoinc t2 5
check_autoinc t3 100
check_autoinc t4 51

stop_server

vlog "Restoring the incremental chain"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full \
  --incremental-dir=$topdir/inc1
xtrabackup --prepare --target-dir=$topdir/full --incremental-dir=$topdir/inc2
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full
start_server

for t in t1 t2 t3 t4 t5 t6
do
  eval "check_label $t \"\$inc_label_$t\""
done
check_autoinc t1 8
check_autoinc t2 5
check_autoinc t3 100
check_autoinc t4 51
check_autoinc t5 201
check_autoinc t6 301

