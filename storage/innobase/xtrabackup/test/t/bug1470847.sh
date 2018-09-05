#
# Bug 1470847: starting LSN is bigger than ending LSN makes no error
#

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-log-files-in-group=4
innodb-log-file-size=4M
"
start_server

run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
	"CREATE TABLE t (a INT AUTO_INCREMENT PRIMARY KEY) ENGINE=InnoDB" test

run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
	"INSERT INTO t VALUES (), (), (), (), (), (), (), ()" test

# fill some redo log files
while [ $(innodb_lsn) -lt 30800000 ] ; do
	run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
		"INSERT INTO t SELECT NULL FROM t LIMIT 1000" test
done

# pass wrong value for innodb-log-files-in-group
run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --backup \
			--target-dir=$topdir/backup \
			--innodb-log-files-in-group=3

sleep 1

grep -q "xtrabackup: error: log block numbers mismatch" $OUTFILE
