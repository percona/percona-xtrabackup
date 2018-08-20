########################################################################
# Bug #983685: innodb_data_file_path is not written to backup-my.cnf
########################################################################

. inc/common.sh

start_server

options="innodb_data_file_path"

mkdir -p $topdir/backup
xtrabackup --backup --target-dir=$topdir/backup

# test presence of options
for option in $options ; do

	if ! grep $option $topdir/backup/backup-my.cnf ; then
		vlog "Option $option is absent"
		exit -1
	else
		vlog "Option $option is present"
	fi

done
