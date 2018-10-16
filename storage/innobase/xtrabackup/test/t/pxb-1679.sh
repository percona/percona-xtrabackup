#
# PXB-1679: Crash after truncating partition when the backup is taken
#

start_server

mysql test <<EOF
CREATE TABLE test01 (id int auto_increment primary key, a TEXT, b TEXT) PARTITION BY HASH(id) PARTITIONS 4;
INSERT INTO test01 (a, b) VALUES (REPEAT('a', 1000), REPEAT('b', 1000));
INSERT INTO test01 (a, b) VALUES (REPEAT('x', 1000), REPEAT('y', 1000));
INSERT INTO test01 (a, b) VALUES (REPEAT('q', 1000), REPEAT('p', 1000));
INSERT INTO test01 (a, b) VALUES (REPEAT('l', 1000), REPEAT('c', 1000));
INSERT INTO test01 (a, b) VALUES (REPEAT('m', 1000), REPEAT('p', 1000));
INSERT INTO test01 (a, b) VALUES (REPEAT('1', 1000), REPEAT('2', 1000));
INSERT INTO test01 (a,b) SELECT a,b FROM test01;
INSERT INTO test01 (a,b) SELECT a,b FROM test01;
INSERT INTO test01 (a,b) SELECT a,b FROM test01;
INSERT INTO test01 (a,b) SELECT a,b FROM test01;
INSERT INTO test01 (a,b) SELECT a,b FROM test01;
INSERT INTO test01 (a,b) SELECT a,b FROM test01;
INSERT INTO test01 (a,b) SELECT a,b FROM test01;
INSERT INTO test01 (a,b) SELECT a,b FROM test01;
INSERT INTO test01 (a,b) SELECT a,b FROM test01;
INSERT INTO test01 (a,b) SELECT a,b FROM test01;

CREATE TABLE test02 (id int auto_increment primary key, a TEXT, b TEXT);
INSERT INTO test02 SELECT * FROM test01;
EOF

xtrabackup --backup 2>&1 --target-dir=$topdir/backup | tee /dev/stderr | \
    while read line ; do
        echo $line
        if [ $( echo $line | grep -c './test/test01#P#p3.ibd') -eq 1 ]; then
            mysql -e "ALTER TABLE test01 TRUNCATE PARTITION p3" test
        fi
        if [ $( echo $line | grep -c './test/test02.ibd') -eq 1 ]; then
            mysql -e "TRUNCATE TABLE test02" test
        fi
    done;

if ! [ ${PIPESTATUS[0]} -eq 0 ] ; then
    die "backup failed"
fi

record_db_state test

shutdown_server

xtrabackup --prepare --target-dir=$topdir/backup
rm -rf $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/backup

start_server
verify_db_state test
