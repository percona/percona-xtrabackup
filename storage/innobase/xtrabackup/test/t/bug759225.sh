########################################################################
# Bug #759225: xtrabackup does not support ALL_O_DIRECT for 
#              innodb_flush_method
########################################################################

. inc/common.sh

if test "`uname -s`" != "Linux"
then
    skip_test "Requires Linux"
fi

require_xtradb
require_server_version_lower_than 5.7.0

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_flush_method=ALL_O_DIRECT
"
start_server
load_sakila

# Take backup
mkdir -p $topdir/backup
innobackupex --stream=tar $topdir/backup > $topdir/backup/out.tar
stop_server

# Remove datadir
rm -r $mysql_datadir
# Restore sakila
vlog "Applying log"
backup_dir=$topdir/backup
cd $backup_dir
$TAR -ixvf out.tar
cd - >/dev/null 2>&1
innobackupex --apply-log $backup_dir
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
innobackupex --copy-back $backup_dir

start_server
# Check sakila
${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
