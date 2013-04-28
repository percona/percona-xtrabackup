########################################################################
# Blueprint: Support InnoDB buffer pool dumps in MySQL 5.6
########################################################################

. inc/common.sh

if ! which rsync > /dev/null 2>&1 ; then
    echo "Requires rsync to be installed" > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

if [ "${MYSQL_VERSION:0:3}" != "5.6" ]; then
    echo "Requires MySQL 5.6" > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_buffer_pool_filename=pool/dump
"

start_server

# test presence of innodb_buffer_pool_filename
${XB_BIN} --defaults-file=$topdir/my.cnf --print-param | \
                            grep -q "innodb_buffer_pool_filename"

mkdir $mysql_datadir/pool

# produce buffer pool dump
${MYSQL} ${MYSQL_ARGS} -e "SET GLOBAL innodb_buffer_pool_dump_now=ON;"

# take a backup with rsync mode
innobackupex --rsync --no-timestamp $topdir/backup

if [ -f $topdir/backup/pool/dump ] ; then
    vlog "Buffer pool dump has been backed up"
else
    vlog "Buffer pool dump has not been backed up"
    exit -1
fi
