. inc/common.sh
# If server config file contains innodb-log-arch-dir option
# then "xtrabackup --backup" fails as it reads this option
# from server config file and it was supposed innodb-log-arch-dir
# option can be used only with --prepare for xtrabackup.
# Currently this limitation is removed and only warning outputs
# when attempt of using innodb-log-arch-dir without --prepare
# takes place.

require_xtradb
# The xtrabackup build and binary names are choosen automatically
# corresponding to server version except the case when
# build name is set explicitly with certain command line option.
# If such option is not used the below string not only checks the
# server version but if the check passed this means that the
# required xtrabackup build name is set.
require_server_version_higher_than '5.6.10'

init_server_variables 1
switch_server 1
ARCH_LOG_DIR=$MYSQLD_VARDIR/arch_log
BACKUP_DIR=$MYSQLD_VARDIR/backup
reset_server_variables 1

MYSQLD_EXTRA_MY_CNF_OPTS="innodb-log-archive=ON
			  innodb-log-arch-dir=$ARCH_LOG_DIR"
mkdir -p $ARCH_LOG_DIR $BACKUP_DIR

start_server
# If xtrabackup finished whithout error the test passed.
xtrabackup --backup --datadir=$MYSQLD_DATADIR --target-dir=$BACKUP_DIR
stop_server

rm -rf $ARCH_LOG_DIR $BACKUP_DIR

unset ARCH_LOG_DIR
unset BACKUP_DIR

exit 0
