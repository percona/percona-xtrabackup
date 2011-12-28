##########################################################################
# Bug #826632: parallel option breaks incremental backups                #
##########################################################################

. inc/common.sh

init
run_mysqld --innodb_file_per_table
load_dbase_schema sakila
load_dbase_data sakila

# Take backup
vlog "Creating the backup directory: $topdir/backup"
backup_dir="$topdir/backup"
innobackupex $topdir/full_backup --no-timestamp --parallel=8

# Make some changes for incremental backup by truncating and reloading
# tables. TRUNCATE cannot be used here, because that would be executed
# as DROP + CREATE internally for InnoDB tables, so tablespace IDs
# would change.

table_list=`$MYSQL $MYSQL_ARGS -Ns -e \
"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='sakila' \
AND TABLE_TYPE='BASE TABLE'"`

run_cmd $MYSQL $MYSQL_ARGS -s -e "SET GLOBAL foreign_key_checks=0"
for t in $table_list
do
    run_cmd $MYSQL $MYSQL_ARGS -s -e "DELETE FROM $t" sakila
done
run_cmd $MYSQL $MYSQL_ARGS -s -e "SET GLOBAL foreign_key_checks=1"

load_dbase_data sakila

# Do an incremental parallel backup
innobackupex --incremental --no-timestamp --parallel=8 \
    --incremental-basedir=$topdir/full_backup $topdir/inc_backup

stop_mysqld
# Remove datadir
rm -r $mysql_datadir

vlog "Applying log"
innobackupex --apply-log --redo-only $topdir/full_backup
innobackupex --apply-log --redo-only --incremental-dir=$topdir/inc_backup \
    $topdir/full_backup
innobackupex --apply-log $topdir/full_backup

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
innobackupex --copy-back $topdir/full_backup

run_mysqld

# Check sakila
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
