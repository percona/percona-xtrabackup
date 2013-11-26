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
innobackupex --no-timestamp $topdir/backup

# produce buffer pool dump
${MYSQL} ${MYSQL_ARGS} -e "SET GLOBAL innodb_buffer_pool_dump_now=ON;"

# incremental backup
innobackupex --no-timestamp --incremental --incremental-basedir=$topdir/backup \
    $topdir/incremental

if [ -f $topdir/incremental/pool/dump ] ; then
    vlog "Buffer pool dump has been backed up"
else
    vlog "Buffer pool dump has not been backed up"
    exit -1
fi

# prepare
innobackupex --apply-log --redo-only $topdir/backup

innobackupex --apply-log --redo-only --incremental-dir=$topdir/incremental \
    $topdir/backup

innobackupex --apply-log $topdir/backup

# restore from backup
rm -rf $mysql_datadir/*
innobackupex --copy-back $topdir/backup

if [ -f $mysql_datadir/pool/dump ] ; then
    vlog "Buffer pool dump has been restored"
else
    vlog "Buffer pool dump has not been restored"
    exit -1
fi
