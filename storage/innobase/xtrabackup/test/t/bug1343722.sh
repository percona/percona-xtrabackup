########################################################################
# Bug #1343722: Too easy to backup wrong datadir with multiple instances
########################################################################

start_server_with_id 1

socket=$MYSQLD_SOCKET

start_server_with_id 2

# Try to backup server 2, but use server 1's connection socket
$IB_BIN $IB_ARGS --socket=$socket --no-timestamp $topdir/backup 2>&1 |
    grep 'has different values'

if [[ ${PIPESTATUS[0]} != 0 ]]
then
    die "innobackupex failed"
fi
