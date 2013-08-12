########################################################################
# Bug #733651: innobackupex not stores some critical
# innodb options in backup-my.cnf
########################################################################

. inc/common.sh

options="innodb_log_files_in_group innodb_log_file_size"

# innodb_page_size is supported in XtraDB 5.1+ and InnoDB 5.6+
if is_xtradb || is_server_version_higher_than 5.6.0
then
    options="$options innodb_page_size"
fi

# innodb_fast_checksum is supported in XtraDB 5.1/5.5
if is_xtradb && is_server_version_lower_than 5.6.0
then
    options="$options innodb_fast_checksum"
fi

# innodb_log_block_size is only supported in XtraDB
if is_xtradb
then
    options="$options innodb_log_block_size"
fi

start_server

innobackupex --no-timestamp $topdir/backup

# test presence of options
for option in $options ; do

        if ! cat $topdir/backup/backup-my.cnf | grep $option
        then
                vlog "Option $option is absent"
                exit -1
        else
                vlog "Option $option is present"
        fi

done
