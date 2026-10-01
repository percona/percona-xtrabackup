########################################################################
# PXB-2559: --history may write its record to a server other than the
# one being backed up.
#
# suites/gr/history_host.sh covers the Group Replication scenario that
# motivates the feature. This test covers the mechanics on two plain
# servers: where the record ends up, where the incremental read path
# looks for it, when the history connection is validated, whose grants
# the privilege check consults, and whether --history-ssl-* reaches the
# connection it is meant to configure.
########################################################################
. inc/common.sh

backup_id=1
history_id=2

history_user=history_user
history_password=historyPwd123

start_server_with_id $backup_id
start_server_with_id $history_id

history_port=${SRV_MYSQLD_PORT[$history_id]}

########################################################################
vlog "Preparing the history server"

switch_server $history_id

# The least privilege the history account needs. CREATE is required even
# once the table exists, since CREATE DATABASE IF NOT EXISTS is issued on
# every run and the server checks the privilege before existence.
mysql -e "CREATE USER $history_user@'%' IDENTIFIED BY '$history_password'"
mysql -e "GRANT CREATE, ALTER, INSERT, SELECT ON PERCONA_SCHEMA.* \
    TO $history_user@'%'"
# Opening any connection reads this table, see xb::connect().
mysql -e "GRANT SELECT ON performance_schema.replication_group_members \
    TO $history_user@'%'"

########################################################################
vlog "Preparing the server to be backed up"

switch_server $backup_id

mysql -e "CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=InnoDB" test
multi_row_insert test.t1 \({1..100}\)

history_dsn="--history-host=127.0.0.1 --history-port=$history_port \
    --history-user=$history_user --history-password=$history_password"

########################################################################
vlog "The record must land on the history server"

run_cmd $XB_BIN $XB_ARGS --backup --history=full $history_dsn \
    --target-dir=$topdir/backup

switch_server $history_id

check_count PERCONA_SCHEMA xtrabackup_history 1

name=`mysql -NBe "SELECT name FROM PERCONA_SCHEMA.xtrabackup_history"`
if [ "$name" != "full" ]
then
    die "history record has name \"$name\", expected \"full\""
fi

########################################################################
vlog "And nowhere near the server that was backed up"

switch_server $backup_id

if [ -n "`mysql -NBe \"SHOW DATABASES LIKE 'PERCONA_SCHEMA'\"`" ]
then
    die "PERCONA_SCHEMA was created on the server being backed up"
fi

########################################################################
vlog "An incremental backup must read its base LSN from the history server"

switch_server $history_id
to_lsn=`mysql -NBe "SELECT innodb_to_lsn FROM PERCONA_SCHEMA.xtrabackup_history \
    WHERE name = 'full'"`

if [ -z "$to_lsn" ] || [ "$to_lsn" = "NULL" ] || [ "$to_lsn" = "0" ]
then
    die "the history record has no usable innodb_to_lsn: \"$to_lsn\""
fi

switch_server $backup_id
multi_row_insert test.t1 \({101..200}\)

run_cmd $XB_BIN $XB_ARGS --backup --incremental-history-name=full \
    $history_dsn --target-dir=$topdir/inc > $topdir/inc.log 2>&1

run_cmd grep -q "Found and using lsn: $to_lsn" $topdir/inc.log

from_lsn=`grep '^from_lsn' $topdir/inc/xtrabackup_checkpoints | \
    sed -e 's/^from_lsn[[:space:]]*=[[:space:]]*//'`

if [ "$from_lsn" != "$to_lsn" ]
then
    die "incremental started from LSN $from_lsn, expected $to_lsn as recorded \
on the history server"
fi

# Without --history the incremental must not add a record of its own.
switch_server $history_id
check_count PERCONA_SCHEMA xtrabackup_history 1

########################################################################
vlog "The history options are rejected without a reason to use them"

switch_server $backup_id

run_cmd_expect_failure $XB_BIN $XB_ARGS --backup $history_dsn \
    --target-dir=$topdir/backup_no_history

########################################################################
vlog "The history connection must be validated before the backup starts"

run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --history=bad_credentials \
    --history-host=127.0.0.1 --history-port=$history_port \
    --history-user=$history_user --history-password=wrongPassword \
    --target-dir=$topdir/backup_bad

if [ -f $topdir/backup_bad/ibdata1 ]
then
    die "the backup ran even though the history connection could not be opened"
fi

########################################################################
vlog "--check-privileges must consult the history account, not the backup one"

switch_server $history_id

# Everything the history account needs except INSERT. Before PXB-2559 the
# privilege check looked at the backup account and never checked INSERT at
# all, so this ran a full backup and then dropped the record silently.
mysql -e "CREATE USER noinsert@'%' IDENTIFIED BY '$history_password'"
mysql -e "GRANT CREATE, ALTER, SELECT ON PERCONA_SCHEMA.* TO noinsert@'%'"
mysql -e "GRANT SELECT ON performance_schema.replication_group_members \
    TO noinsert@'%'"

switch_server $backup_id

run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --check-privileges \
    --history=no_insert \
    --history-host=127.0.0.1 --history-port=$history_port \
    --history-user=noinsert --history-password=$history_password \
    --target-dir=$topdir/backup_noinsert > $topdir/noinsert.log 2>&1

run_cmd grep -q \
    "missing required privilege INSERT on PERCONA_SCHEMA.xtrabackup_history" \
    $topdir/noinsert.log

########################################################################
vlog "--history-ssl-* must configure the history connection"

switch_server $history_id

mysql -e "CREATE USER ssluser@'%' IDENTIFIED BY '$history_password' REQUIRE SSL"
mysql -e "GRANT CREATE, ALTER, INSERT, SELECT ON PERCONA_SCHEMA.* \
    TO ssluser@'%'"
mysql -e "GRANT SELECT ON performance_schema.replication_group_members \
    TO ssluser@'%'"

switch_server $backup_id

# The default ssl-mode is PREFERRED, so the history connection is encrypted
# and an account declared REQUIRE SSL is able to write the record.
run_cmd $XB_BIN $XB_ARGS --backup --history=tls \
    --history-host=127.0.0.1 --history-port=$history_port \
    --history-user=ssluser --history-password=$history_password \
    --target-dir=$topdir/backup_tls

switch_server $history_id
check_count PERCONA_SCHEMA xtrabackup_history 2

switch_server $backup_id

# Turning TLS off for the history connection alone must be honoured, which
# the server demonstrates by refusing the connection.
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --history=no_tls \
    --history-host=127.0.0.1 --history-port=$history_port \
    --history-user=ssluser --history-password=$history_password \
    --history-ssl-mode=DISABLED \
    --target-dir=$topdir/backup_notls

switch_server $history_id
check_count PERCONA_SCHEMA xtrabackup_history 2
