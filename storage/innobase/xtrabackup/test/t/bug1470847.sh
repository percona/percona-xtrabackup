#
# Bug 1470847: starting LSN is bigger than ending LSN makes no error
#

MYSQLD_EXTRA_MY_CNF_OPTS="
innodb-log-files-in-group=4
innodb-log-file-size=2M
"
start_server

run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
	"CREATE TABLE t (a INT AUTO_INCREMENT PRIMARY KEY) ENGINE=InnoDB" test

run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
	"INSERT INTO t VALUES (), (), (), (), (), (), (), ()" test

function mysql_lsn()
{
	${MYSQL} ${MYSQL_ARGS} -e "SHOW ENGINE InnoDB STATUS\G" | \
		grep "Log sequence number" | awk '{ print $4 }'
}

function mysql_flushed_lsn()
{
	${MYSQL} ${MYSQL_ARGS} -e "SHOW ENGINE InnoDB STATUS\G" | \
		grep "Log flushed up to" | awk '{ print $5 }'
}

function mysql_n_dirty_pages()
{
	result=$( $MYSQL $MYSQL_ARGS -se \
		"SHOW STATUS LIKE 'innodb_buffer_pool_pages_dirty'" | \
		awk '{ print $2 }' )
	echo "Dirty pages left $result"
	return $result
}

# fill 3 of 4 redo log files
while [ `mysql_lsn` -lt 6502000 ] ; do
	run_cmd ${MYSQL} ${MYSQL_ARGS} -e \
		"INSERT INTO t SELECT NULL FROM t LIMIT 1000" test
done

# wait for InnoDB to flush all dirty pages
while ! mysql_n_dirty_pages ; do
	sleep 1
done

# wait for InnoDB to flush redo log
while [ `mysql_flushed_lsn` != `mysql_lsn` ] ; do
	sleep 1
done

sleep 1

# pass wrong value for innodb-log-files-in-group
run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --backup \
			--target-dir=$topdir/backup \
			--innodb-log-files-in-group=3

grep -q "xtrabackup: error: last checkpoint LS" $OUTFILE
