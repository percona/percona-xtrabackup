########################################################################
# Blueprint: Support InnoDB buffer pool dumps in MySQL 5.6
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb_buffer_pool_filename=pool/dump
"

start_server

# test presence of innodb_buffer_pool_filename
${XB_BIN} --defaults-file=$topdir/my.cnf --print-param | \
                            grep -q "innodb_buffer_pool_filename"

mkdir $mysql_datadir/pool

# take a backup
xtrabackup --backup --target-dir=$topdir/backup

# produce buffer pool dump
${MYSQL} ${MYSQL_ARGS} -e "SET GLOBAL innodb_buffer_pool_dump_now=ON;"

# incremental backup
xtrabackup --backup --incremental-basedir=$topdir/backup \
    --target-dir=$topdir/incremental

if [ -f $topdir/incremental/pool/dump ] ; then
    vlog "Buffer pool dump has been backed up"
else
    vlog "Buffer pool dump has not been backed up"
    exit -1
fi

# prepare
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup

xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/incremental \
    --target-dir=$topdir/backup

xtrabackup --prepare --target-dir=$topdir/backup

# restore from backup
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/backup

if [ -f $mysql_datadir/pool/dump ] ; then
    vlog "Buffer pool dump has been restored"
else
    vlog "Buffer pool dump has not been restored"
    exit -1
fi
