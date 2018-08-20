##########################################################################
# Bug #826632: parallel option breaks incremental backups                #
##########################################################################

. inc/common.sh

if is_server_version_higher_than 5.6.0 && \
   is_server_version_lower_than 5.6.11
then
    skip_test "Doesn't work for server 5.6.x if x < 11, bug #1203669"
fi

start_server --innodb_file_per_table

load_dbase_schema sakila
load_dbase_data sakila

# Take backup
vlog "Creating the backup directory: $topdir/backup"
backup_dir="$topdir/backup"
xtrabackup --backup --target-dir=$topdir/full_backup --parallel=8

# Make some changes for incremental backup by truncating and reloading
# tables. TRUNCATE cannot be used here, because that would be executed
# as DROP + CREATE internally for InnoDB tables, so tablespace IDs
# would change.

table_list=`$MYSQL $MYSQL_ARGS -Ns -e \
"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='sakila' \
AND TABLE_TYPE='BASE TABLE'"`

for t in $table_list
do
    run_cmd $MYSQL $MYSQL_ARGS -s sakila <<EOF
SET foreign_key_checks=0;
DELETE FROM $t;
SET foreign_key_checks=1;
EOF
done

load_dbase_data sakila

# Do an incremental parallel backup
xtrabackup --backup --parallel=8 \
    --incremental-basedir=$topdir/full_backup --target-dir=$topdir/inc_backup

stop_server
# Remove datadir
rm -r $mysql_datadir

vlog "Applying log"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full_backup
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc_backup \
    --target-dir=$topdir/full_backup
xtrabackup --prepare --target-dir=$topdir/full_backup

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full_backup

start_server

# Check sakila
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
