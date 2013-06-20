##############################################################################
# Bug #1192347: innobackupex terminates if --galera-info specified when
#               backing up non-galera server
##############################################################################

. inc/common.sh

set +e
${MYSQLD} --basedir=$MYSQL_BASEDIR --user=$USER --help --verbose --wsrep-sst-method=rsync| grep -q wsrep
probe_result=$?
if [[ "$probe_result" == "0" ]]
    then
        echo "Server supports wsrep" > $SKIPPED_REASON
        exit $SKIPPED_EXIT_CODE
fi
set -e

start_server

innobackupex --no-timestamp --galera-info $topdir/backup

# Check that to xtrabackup_binlog_info file is created
if [ -f $topdir/backup/xtrabackup_galera_info ]
then
    vlog "$topdir/backup/xtrabackup_galera_info should not be created!"
    exit -1
fi
