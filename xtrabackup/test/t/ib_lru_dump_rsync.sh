########################################################################
# Bug #543134: Missing support for LRU dump/restore
########################################################################

. inc/common.sh

if ! is_xtradb || is_server_version_higher_than 5.6.0
then
    skip_test "Requires Percona Server <= 5.5"
fi

if ! which rsync > /dev/null 2>&1 ; then
    skip_test "Requires rsync to be installed"
fi

start_server

# produce ib_lru_dump
${MYSQL} ${MYSQL_ARGS} -e "select * from information_schema.XTRADB_ADMIN_COMMAND /*!XTRA_LRU_DUMP*/;"

# take a backup with rsync mode
innobackupex --rsync --no-timestamp $topdir/backup

if [ -f $topdir/backup/ib_lru_dump ] ; then
    vlog "LRU dump has been backed up"
else
    vlog "LRU dump has not been backed up"
    exit -1
fi
