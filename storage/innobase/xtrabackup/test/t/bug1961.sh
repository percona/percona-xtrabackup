############################################################################
# Bug #1961: xtraback fails with encryption='n' with Percona server
############################################################################

. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_file_per_table
"

start_server

QUERY="show variables like 'innodb_page_cleaner_disabled_debug'";
count=`${MYSQL} ${MYSQL_ARGS} -N -e "${QUERY}"`
if [ count > 0 ]
then
   ${MYSQL} ${MYSQL_ARGS} -e "set global innodb_page_cleaner_disabled_debug=on"
fi

mysql test <<EOF
CREATE TABLE t1(a INT) ENGINE=INNODB ENCRYPTION='N';

INSERT INTO t1 VALUES(100);

EOF

record_db_state test

mkdir -p $topdir/backup

# Backup
xtrabackup --datadir=$mysql_datadir --backup --target-dir=$topdir/backup 

# Prepare
xtrabackup --datadir=$mysql_datadir --prepare --target-dir=$topdir/backup

stop_server

# Restore
rm -rf $mysql_datadir/ibdata1 $mysql_datadir/ib_logfile* \
    $mysql_datadir/test/*.ibd
cp -r $topdir/backup/* $mysql_datadir

start_server

# Verify backup
verify_db_state test
