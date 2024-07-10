########################################################################
# Test support for GTID
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

MYSQLD_EXTRA_MY_CNF_OPTS="
gtid_mode=on
log_slave_updates=on
enforce_gtid_consistency=on
"
start_server

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, c INT);
INSERT INTO t(c) VALUES (1),(2),(3),(4),(5),(6),(7),(8);
SET AUTOCOMMIT=0;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
INSERT INTO t(c) SELECT c FROM t;
COMMIT;
EOF

xtrabackup --backup --target-dir=$topdir/backup

# Check that the value of gtid_executed is in xtrabackup_binlog_info
if ! egrep -q '^mysql-bin.[0-9]+[[:space:]]+[0-9]+[[:space:]]+[a-f0-9:-]+$' \
    $topdir/backup/xtrabackup_binlog_info
then
    die "Cannot find GTID coordinates in xtrabackup_binlog_info"
fi

#
# PXB-3302 : Test if PXB logs GTID string > 8192 characters
#


sql1="SELECT UUID();"
sql2="SET GTID_NEXT=;"
sql3="BEGIN;COMMIT;"

iterations=250
for (( i=1; i<=$iterations; i++ ))
do
    uuid=`$MYSQL $MYSQL_ARGS -BN -e "$sql1"`
    sql2="SET GTID_NEXT='$uuid:1';"
    $MYSQL $MYSQL_ARGS -e "$sql2; $sql3"
done

GTID_FROM_SQL=$($MYSQL $MYSQL_ARGS -BN -e "SELECT JSON_UNQUOTE(JSON_EXTRACT(LOCAL, '$.gtid_executed')) AS gtid_executed FROM performance_schema.log_status" | tr -d '\\n')

LOGFILE=$topdir/backup.log
rm -rf $topdir/backup
xtrabackup --backup --target-dir=$topdir/backup 2>&1 | tee $LOGFILE

# Search for the line containing "GTID of the last change"
line=$(grep "GTID of the last change" "$LOGFILE")

# Check if the line was found
if [ -z "$line" ]; then
    echo "Error: 'GTID of the last change' not found in the $LOGFILE"
    exit 1
else
    # Extract the GTID without single quotes
    GTID_FROM_ERRORLOG=$(echo "$line" | awk -F"GTID of the last change '" '{print $2}' | cut -d"'" -f1)

    # Print the result
		if [ "$GTID_FROM_ERRORLOG" = "$GTID_FROM_SQL" ]; then
			echo "GTID MATCHED"
	  else
			echo "GTID MISMATCH"
			echo "GTID_FROM_SQL is $GTID_FROM_SQL"
			echo "GTID_FROM_ERRORLOG is $GTID_FROM_ERRORLOG"
			exit 1
		fi
fi
