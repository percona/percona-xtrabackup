########################################################################
# Bug #930062: Innobackupex doesn't add file-per-table setting for
#              table-independent backups
########################################################################

. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="innodb_file_per_table=0"
if [ "$MYSQL_VERSION_MAJOR" -gt 5 -o \
     "$MYSQL_VERSION_MINOR" -ge 6 ]
then
    # For server versions >= 5.6 we don't need any special server options to
    # enable table import
    true
elif [ -n "$XTRADB_VERSION" ]
then
    case "$MYSQL_VERSION_MAJOR.$MYSQL_VERSION_MINOR" in
        5.5 )
            import_option="innodb_import_table_from_xtrabackup=1";;
        5.1 | 5.2 | 5.3 )
            import_option="innodb_expand_import=1";;
        *)
            die "Unknown XtraDB version";;
    esac
    MYSQLD_EXTRA_MY_CNF_OPTS="$MYSQLD_EXTRA_MY_CNF_OPTS
$import_option"
else
    skip_test "Requires XtraDB 5.1+ or InnoDB 5.6+"
fi

start_server

backup_dir=$topdir/backup

# Enable innodb_file_per_table dynamically, so we get individual tablespaces to
# export
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SET GLOBAL innodb_file_per_table=1"

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

start_server

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
