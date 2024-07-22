########################################################################
# PXB-3295 : Undo tablespaces are not tracked properly with lock-ddl=REDUCED
########################################################################

. inc/common.sh

require_debug_pxb_version
start_server

echo
echo PXB-3295 : Undo tablespaces are not tracked properly
echo
mysql -e "CREATE UNDO TABLESPACE UNDO_1 ADD DATAFILE 'undo_1.ibu'"
mysql -e "CREATE UNDO TABLESPACE UNDO_2 ADD DATAFILE 'undo_2.ibu'"
mysql -e "CREATE UNDO TABLESPACE UNDO_3 ADD DATAFILE 'undo_3.ibu'"
XB_ERROR_LOG=$topdir/backup.log
BACKUP_DIR=$topdir/backup
rm -rf $BACKUP_DIR
xtrabackup --backup --lock-ddl=REDUCED --target-dir=$BACKUP_DIR \
--debug-sync="ddl_tracker_before_lock_ddl" 2> >( tee $XB_ERROR_LOG)&

job_pid=$!
pid_file=$topdir/backup/xtrabackup_debug_sync
wait_for_xb_to_suspend $pid_file
xb_pid=`cat $pid_file`
echo "backup pid is $job_pid"

UNDO_2_BI_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space  from information_schema.innodb_tablespaces where name = 'UNDO_3'")
mysql -e "ALTER UNDO TABLESPACE UNDO_2 SET INACTIVE"
mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=1"
sleep 1
UNDO_2_BI_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space  from information_schema.innodb_tablespaces where name = 'UNDO_3'")
mysql -e "DROP UNDO TABLESPACE UNDO_2"

# before inactive
UNDO_3_BI_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space  from information_schema.innodb_tablespaces where name = 'UNDO_3'")

mysql -e "ALTER UNDO TABLESPACE UNDO_3 SET INACTIVE"
mysql -e "SET GLOBAL innodb_purge_rseg_truncate_frequency=1"
sleep 2

# after inactive
UNDO_3_AI_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space  from information_schema.innodb_tablespaces where name = 'UNDO_3'")
mysql -e "CREATE UNDO TABLESPACE UNDO_4 ADD DATAFILE 'undo_4.ibu'"

# Resume the xtrabackup process
vlog "Resuming xtrabackup"
kill -SIGCONT $xb_pid
run_cmd wait $job_pid

UNDO_1_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space from information_schema.innodb_tablespaces where name = 'UNDO_1'")
UNDO_4_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space from information_schema.innodb_tablespaces where name = 'UNDO_4'")

xtrabackup --prepare --target-dir=$BACKUP_DIR
stop_server

rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$BACKUP_DIR
start_server

UNDO_BACKUP_1_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space from information_schema.innodb_tablespaces where name = 'UNDO_1'")
UNDO_BACKUP_2_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space  from information_schema.innodb_tablespaces where name = 'UNDO_2'")
UNDO_BACKUP_3_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space  from information_schema.innodb_tablespaces where name = 'UNDO_3'")
UNDO_BACKUP_4_SPACE_ID=$($MYSQL $MYSQL_ARGS -BN -e "select space from information_schema.innodb_tablespaces where name = 'UNDO_4'")

echo "UNDO_1 space_id is $UNDO_BACKUP_1_SPACE_ID"
echo "UNDO_2 space_id is $UNDO_BACKUP_2_SPACE_ID"
echo "UNDO_3 space_id is $UNDO_BACKUP_3_SPACE_ID"
echo "UNDO_4 space_id is $UNDO_BACKUP_4_SPACE_ID"

# UNDO_1 no space_id change
if [ $UNDO_1_SPACE_ID != $UNDO_BACKUP_1_SPACE_ID ]; then
 echo "space_id of undo_01 shouldn't have changed. Before backup: undo_1 space_id $UNDO_1_SPACE_ID. After backup and restore undo_1 space_id is $UNDO_BACKUP_1_SPACE_ID"
 exit 1
fi

## UNDO_4 no space_id change. If we get this it means undo_04 is copied.
#FILE=$BACKUP_DIR/undo_4.ibu
#[ -f $FILE ] || die "$FILE did NOT exist. It should have been backuped. It is created before end of backup. Server did CREATE UNDO TABLESPACE undo_04, right before DDL lock is taken"
#
#if [ -z ${UNDO_BACKUP_4_SPACE_ID+x} ]; then
# echo "UNDO TABLESPACE 4 should have been present in backup. But it is not found."
# exit 1
#if
#
#if [ $UNDO_4_SPACE_ID != $UNDO_BACKUP_4_SPACE_ID ]; then
# echo "space_id of undo_04 shouldn't have changed. Before backup: undo_4 space_id $UNDO_4_SPACE_ID. After backup and restore undo_4 space_id is $UNDO_BACKUP_4_SPACE_ID"
# exit 1
#fi

# UND0_3 should have AI space_id and not BI space_id
if [ $UNDO_3_AI_SPACE_ID != $UNDO_BACKUP_3_SPACE_ID ]; then
 echo "space_id of undo_03 should have space_id after truncation. space_id at time of the backup: undo_3 space_id $UNDO_3_AF_SPACE_ID. After backup and restore undo_1 space_id is $UNDO_BACKUP_3_SPACE_ID"
 exit 1
fi

# UNDO_3 shouldn't have space_id before truncation
if [ $UNDO_3_BI_SPACE_ID == $UNDO_BACKUP_3_SPACE_ID ]; then
 echo "space_id of undo_03 should NOT have space_id before truncation. space_id  before_truncation undo_3 space_id $UNDO_3_BI_SPACE_ID. After backup and restore undo_1 space_id is $UNDO_BACKUP_3_SPACE_ID"
 exit 1
fi

FILE=$BACKUP_DIR/undo_2.ibu
[ ! -f $FILE ] || die "$FILE exists. It should have been deleted by prepare. Server did DROP UNDO TABLESPACE undo_02"

# UNDO_2 should be empty because it was dropped. We should also check physical presence of undo_02.ibu file
if [ ! -z "${UNDO_BACKUP_2_SPACE_ID}" ]; then
 echo "UNDO TABLESPACE 2 should have been dropped in backup. Found UNDO_2 with space_id: $UNDO_BACKUP_2_SPACE_ID"
 exit 1
fi


stop_server

rm -rf $mysql_datadir $BACKUP_DIR
rm $XB_ERROR_LOG
