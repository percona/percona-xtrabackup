#
# basic test for rocksdb backup
#

init_rocksdb

# create some tables and insert some data
load_dbase_schema sakilar
load_dbase_data sakilar

# create some more tables
mysql -e "CREATE TABLE t (a INT PRIMARY KEY AUTO_INCREMENT, b INT, KEY(b), c VARCHAR(200)) ENGINE=ROCKSDB;" test

# insert some data
for i in {1..1000} ; do
  echo "INSERT INTO t (b, c) VALUES (FLOOR(RAND() * 1000000), UUID());"
done | mysql test

# keep insering data for the rest of the test
( while true ; do
  echo "INSERT INTO t (b, c) VALUES (FLOOR(RAND() * 1000000), UUID());"
done | mysql test ) &

eval ${FULL_BACKUP_CMD}

# insert some more data
for i in {1..1000} ; do
  echo "INSERT INTO t (b, c) VALUES (FLOOR(RAND() * 1000000), UUID());"
done | mysql test

eval ${INC_BACKUP_CMD}
eval ${FULL_PREPARE_CMD}
eval ${INC_PREPARE_CMD}

stop_server

eval ${CLEANUP_CMD}

eval ${RESTORE_CMD}

start_server

mysql -e "SELECT COUNT(*) FROM t" test
