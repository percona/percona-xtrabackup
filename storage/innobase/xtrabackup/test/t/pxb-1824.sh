#
# PXB-1824: PXB crashes in incremental backup prepare when row format is changed
#

start_server

require_server_version_higher_than 5.6.0
require_xtradb

for i in {1..10} ; do
    cat <<EOF
CREATE TABLE sbtest$i (
  id int(10) unsigned NOT NULL AUTO_INCREMENT,
  k int(10) unsigned NOT NULL DEFAULT '0',
  c char(120) NOT NULL DEFAULT '',
  pad char(60) NOT NULL DEFAULT '',
  PRIMARY KEY (id),
  KEY k_1 (k)
) ENGINE=InnoDB;
EOF
done | mysql test

for i in {1..1000} ; do
    echo "INSERT INTO sbtest1 (id, k, c, pad) VALUES (0, FLOOR(RAND() * 100000), REPEAT(UUID(), 3), UUID());"
done | mysql test

for i in {2..10} ; do
    mysql -e "INSERT INTO sbtest$i (k, c, pad) SELECT k, c, pad FROM sbtest1" test
done

while true ; do
    for i in {1..10} ; do
	mysql -e "INSERT INTO sbtest$i (k, c, pad) SELECT k, c, pad FROM sbtest$i LIMIT 20" test
    done
done >/dev/null 2>/dev/null &

while true ; do
      mysql -e "ALTER TABLE test.sbtest2 ROW_FORMAT=COMPRESSED"
      mysql -e "ALTER TABLE test.sbtest2 ROW_FORMAT=DYNAMIC"
      mysql -e "ALTER TABLE test.sbtest2 ROW_FORMAT=COMPACT"
      mysql -e "ALTER TABLE test.sbtest2 ROW_FORMAT=REDUNDANT"

      mysql -e "OPTIMIZE TABLE test.sbtest1"
      mysql -e "ALTER TABLE test.sbtest1 FORCE"
      mysql -e "TRUNCATE test.sbtest1"
done >/dev/null 2>/dev/null &

xtrabackup --lock-ddl --backup --target-dir=$topdir/backup --parallel=8
xtrabackup --lock-ddl --backup --incremental-basedir=$topdir/backup \
	   --target-dir=$topdir/inc --parallel=8
xtrabackup --lock-ddl --backup --incremental-basedir=$topdir/inc \
	   --target-dir=$topdir/inc1 --parallel=8

if ! grep -q flushed_lsn $topdir/backup/xtrabackup_checkpoints ; then
    die "flushed_lsn is missing"
fi

stop_server

# apply log shoulnd't fail

xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc \
	   --target-dir=$topdir/backup
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc1 \
	   --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

lsn1=$(grep -q flushed_lsn $topdir/backup/xtrabackup_checkpoints)
lsn2=$(grep -q flushed_lsn $topdir/inc1/xtrabackup_checkpoints)

if [ "$lsn1" != "$lsn2" ] ; then
    die "flushed_lsn is not updated"
fi

rm -rf $mysql_datadir

xtrabackup --move-back --parallel=8 --target-dir=$topdir/backup

start_server

mysql -e "CHECKSUM TABLE sbtest2 EXTENDED" test
