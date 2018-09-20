########################################################################
# Bug #1343722: Too easy to backup wrong datadir with multiple instances
########################################################################

start_server_with_id 1

socket=$MYSQLD_SOCKET

start_server_with_id 2

# HACK: make InnoDB to write some log records so that server 2 LSN is ahead of
# the server 1 LSN and xtrabackup is able to reach the LSN obtained from
# server 1 while copying redo log from server 2.
# Otherwise backup will hang forever.
mysql -e 'CREATE TABLE t (a INT)' test
mysql -e 'INSERT INTO t (a) VALUES (1), (2), (3)' test
mysql -e 'INSERT INTO t (a) SELECT a FROM t' test
mysql -e 'INSERT INTO t (a) SELECT a FROM t' test
mysql -e 'INSERT INTO t (a) SELECT a FROM t' test

# Try to backup server 2, but use server 1's connection socket
xtrabackup --backup --socket=$socket --target-dir=$topdir/backup 2>&1 |
    grep 'has different values'

if [[ ${PIPESTATUS[0]} != 0 ]]
then
    die "xtrabackup failed"
fi
