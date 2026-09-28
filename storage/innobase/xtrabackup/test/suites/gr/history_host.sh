########################################################################
# PXB-2559: allow --history to write its record to a different server
# than the one being backed up.
#
# --history creates PERCONA_SCHEMA.xtrabackup_history and inserts a row
# describing the backup. It used to do so over the same connection that
# takes the backup, so it needed that server to accept writes.
#
# A Group Replication SECONDARY runs with super_read_only=ON, which makes
# those statements impossible. Backing up a secondary with --history
# therefore fails after the backup itself has already been written.
#
# The history record has to be redirectable to the PRIMARY instead.
########################################################################
. inc/common.sh

primary_id=1
secondary_id=2

history_name=pxb2559
history_user=history_user
history_password=historyPwd123

########################################################################
# Wait until the given node has applied everything the primary has
# committed so far. Must be called while positioned on the primary.
########################################################################
function sync_gr_node()
{
    local node_id=$1
    local gtid_executed

    gtid_executed=`$MYSQL $MYSQL_ARGS -NBe "SELECT @@global.GTID_EXECUTED"`

    switch_server $node_id
    $MYSQL $MYSQL_ARGS -e "SELECT WAIT_FOR_EXECUTED_GTID_SET('$gtid_executed')"
}

start_group_replication_cluster 2

########################################################################
vlog "Preparing data on the primary"

switch_server $primary_id

mysql -e "CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=InnoDB" test
multi_row_insert test.t1 \({1..100}\)

# A dedicated account for the history connection, so that the test also
# covers --history-user and --history-password rather than reusing the
# credentials of the backup connection.
mysql -e "CREATE USER $history_user@'%' IDENTIFIED BY '$history_password'"
mysql -e "GRANT CREATE, ALTER, INSERT, SELECT ON *.* TO $history_user@'%'"

# Start from a known state so that the row count checked below is exact.
mysql -e "DROP DATABASE IF EXISTS PERCONA_SCHEMA"

sync_gr_node $secondary_id

########################################################################
vlog "Verifying the expected roles of the two nodes"

switch_server $primary_id
if [ "`mysql -NBe 'SELECT @@global.super_read_only'`" != "0" ]
then
    die "node $primary_id is expected to be a writable primary"
fi

switch_server $secondary_id
if [ "`mysql -NBe 'SELECT @@global.super_read_only'`" != "1" ]
then
    die "node $secondary_id is expected to be a super_read_only secondary"
fi

########################################################################
vlog "--history against the secondary alone must still fail"

# This is the behaviour that motivates the feature, and it must not
# change: with nowhere else to write the record, the backup fails.
switch_server $secondary_id

run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --history=$history_name \
    --target-dir=$topdir/backup_plain > $topdir/history_plain.log 2>&1

if ! grep -qE "1290|super-read-only" $topdir/history_plain.log
then
    cat $topdir/history_plain.log
    die "--history failed, but not because the secondary is read only"
fi

rm -rf $topdir/backup_plain

########################################################################
vlog "--history redirected to the primary must succeed"

switch_server $secondary_id

run_cmd $XB_BIN $XB_ARGS --backup --history=$history_name \
    --history-host=127.0.0.1 \
    --history-port=${SRV_MYSQLD_PORT[$primary_id]} \
    --history-user=$history_user \
    --history-password=$history_password \
    --target-dir=$topdir/backup_redirected

########################################################################
vlog "Checking the history record landed on the primary"

# Note: the record is written on the primary and then replicated back to
# the secondary, so its absence on the secondary cannot be asserted. The
# secondary cannot have produced it itself, though, since it rejects the
# CREATE DATABASE outright, as checked above.
switch_server $primary_id

check_count PERCONA_SCHEMA xtrabackup_history 1

for column in uuid name tool_name tool_command tool_version \
    ibbackup_version server_version start_time end_time innodb_to_lsn
do
    value=`mysql -NBe "SELECT $column FROM PERCONA_SCHEMA.xtrabackup_history"`
    if [ -z "$value" ] || [ "$value" = "NULL" ]
    then
        die "$column in the history record is empty, expected NOT NULL"
    fi
done

value=`mysql -NBe "SELECT name FROM PERCONA_SCHEMA.xtrabackup_history"`
if [ "$value" != "$history_name" ]
then
    die "history record has name \"$value\", expected \"$history_name\""
fi

# The record must describe the backup taken from the secondary, not some
# unrelated backup of the primary.
value=`mysql -NBe "SELECT COUNT(*) FROM PERCONA_SCHEMA.xtrabackup_history \
    WHERE tool_command LIKE '%backup_redirected%'"`
if [ "$value" != "1" ]
then
    mysql -e "SELECT tool_command FROM PERCONA_SCHEMA.xtrabackup_history"
    die "history record does not describe the backup of the secondary"
fi

rm -rf $topdir/backup_redirected
