########################################################################
# Bug #930062: Innobackupex doesn't add file-per-table setting for
#              table-independent backups
########################################################################

. inc/common.sh

if [ -z "$XTRADB_VERSION" ]; then
    echo "Requires XtraDB" > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

if [ ${MYSQL_VERSION:0:3} = "5.5" ]
then
    import_option="--innodb_import_table_from_xtrabackup=1"
else
    import_option="--innodb_expand_import=1"
fi

mysql_extra_args="--innodb_file_per_table $import_option"

start_server $mysql_extra_args

backup_dir=$topdir/backup

load_dbase_schema sakila
load_dbase_data sakila

checksum_1=`checksum_table sakila actor`

innobackupex --no-timestamp --include='actor' $backup_dir

stop_server

# Test that there's no innodb_file_per_table = 1 in any of configuration sources
cnt=`xtrabackup --print-param | grep -c 'innodb_file_per_table = 1' || true`
if [ "$cnt" -ne 0 ]
then
    vlog "innodb_file_per_table is enabled by default"
    exit 1
fi

innobackupex --apply-log --export $backup_dir

start_server $mysql_extra_args

run_cmd ${MYSQL} ${MYSQL_ARGS} sakila <<EOF
SET foreign_key_checks=0;
ALTER TABLE actor DISCARD TABLESPACE;
EOF

run_cmd cp $backup_dir/sakila/actor* $MYSQLD_DATADIR/sakila/
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "ALTER TABLE actor IMPORT TABLESPACE" sakila

checksum_2=`checksum_table sakila actor`

if [ "$checksum_1" != "$checksum_2" ]
then
    vlog "Table checksums mismatch."
    exit 1
fi
