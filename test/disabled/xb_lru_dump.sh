. inc/common.sh

init
run_mysqld

run_cmd ${MYSQL} ${MYSQL_ARGS} -e "select * from information_schema.XTRADB_ADMIN_COMMAND /*!XTRA_LRU_DUMP*/;"

mkdir -p $topdir/backup
run_cmd ${XB_BIN} --datadir=$mysql_datadir --backup --target-dir=$topdir/backup

stop_mysqld

if [ -f $topdir/backup/ib_lru_dump ]
then
	vlog "LRU dump has been backuped"
	clean
else
	vlog "LRU dump was not backuped"
	exit -1
fi
