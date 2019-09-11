#
# PXB-1913: PXB crashes in incremental backup prepare when a multi valued index is added/dropped for json data
#

start_server

cat | mysql test <<EOF
CREATE TABLE sbtest1 (
  id int(10) unsigned NOT NULL AUTO_INCREMENT,
  k int(10) unsigned NOT NULL DEFAULT '0',
  c char(120) NOT NULL DEFAULT '',
  pad char(60) NOT NULL DEFAULT '',
  PRIMARY KEY (id),
  KEY k_1 (k)
) ENGINE=InnoDB;
EOF

cat | mysql test <<EOF
ALTER TABLE sbtest1 
  ADD COLUMN b JSON AS('{"k1": "value", "k2": [10, 20]}');
CREATE INDEX jindex on sbtest1((CAST(b->'$.k2' AS UNSIGNED ARRAY)));
ALTER TABLE sbtest1
  ADD COLUMN d INT AS JSON_UNQUOTE(b->'$.k1') STORED;
EOF

for i in {1..100} ; do
    echo "INSERT INTO sbtest1 (id, k, c, pad) VALUES (0, FLOOR(RAND() * 100000), REPEAT(UUID(), 3), UUID());"
done | mysql test

record_db_state test

mysql test <<EOF &
BEGIN;
INSERT INTO sbtest1 (id, k, c, pad) SELECT 0, k, c, pad FROM sbtest1;
SELECT SLEEP(10000);
COMMIT;
EOF

while ! mysql -e 'SHOW PROCESSLIST' | grep -q 'User sleep' ; do
    sleep 1
done

xtrabackup --backup --target-dir=$topdir/backup

cp -av $topdir/backup /dev/shm/bak11

stop_server

rm -rf $mysql_datadir

xtrabackup --prepare --target-dir=$topdir/backup
xtrabackup --copy-back --target-dir=$topdir/backup

start_server

verify_db_state test

