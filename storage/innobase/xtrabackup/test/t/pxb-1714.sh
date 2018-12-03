#
# Instant ALTER TABLE ADD COLUMN breaks the backup
#

. inc/common.sh

start_server

mysql test <<EOF
CREATE TABLE t1 (f1 CHAR(255)) ENGINE=InnoDB;
ALTER TABLE t1 ADD COLUMN f2 INTEGER;
START TRANSACTION;
INSERT INTO t1 (f1) VALUES ('node1_committed_during');
INSERT INTO t1 (f1) VALUES ('node1_committed_during');
INSERT INTO t1 (f1) VALUES ('node1_committed_during');
INSERT INTO t1 (f1) VALUES ('node1_committed_during');
INSERT INTO t1 (f1) VALUES ('node1_committed_during');
COMMIT;
EOF

################################################################################
# Start an uncommitted transaction pause "indefinitely" to keep the connection
# open
################################################################################
function start_uncomitted_transaction()
{
    mysql test <<EOF
START TRANSACTION;
INSERT INTO t1 (f1) VALUES ('node1_to_be_committed_after');
INSERT INTO t1 (f1) VALUES ('node1_to_be_committed_after');
INSERT INTO t1 (f1) VALUES ('node1_to_be_committed_after');
INSERT INTO t1 (f1) VALUES ('node1_to_be_committed_after');
INSERT INTO t1 (f1) VALUES ('node1_to_be_committed_after');
SELECT SLEEP(100000);
EOF
}

start_uncomitted_transaction &
trx_job=$!

while ! mysql -e 'SHOW PROCESSLIST' | grep 'SELECT SLEEP' ; do
    sleep 1;
done

xtrabackup --backup  --target-dir=$topdir/backup

kill -SIGKILL $trx_job
stop_server

xtrabackup --prepare --target-dir=$topdir/backup

rm -rf $mysql_datadir

xtrabackup --copy-back --target-dir=$topdir/backup

start_server

if mysql -e "CHECK TABLE t1" test | grep Corrupt ; then
  die "Table is corrupt"
fi
