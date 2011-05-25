. inc/common.sh

init
run_mysqld
load_dbase_schema sakila
load_dbase_data sakila

# Take backup
mkdir -p $topdir/backup
mkdir -p $topdir/backup/stream
run_cmd ${IB_BIN} --socket=$mysql_socket --stream=tar $topdir/backup > $topdir/backup/stream/out.tar 2> $OUTFILE 

stop_mysqld
cd $topdir/backup/stream/
$TAR -ixvf out.tar

if [ -f $topdir/backup/stream/xtrabackup_binary ]
then
	vlog "File xtrabackup_binary was copied"
else
	vlog "File xtrabackup_binary was not copied"
	exit -1
fi

cd - >/dev/null 2>&1 
clean
