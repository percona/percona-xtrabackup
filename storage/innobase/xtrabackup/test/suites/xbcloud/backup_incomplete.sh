. inc/xbcloud_common.sh
is_xbcloud_credentials_set

start_server --innodb_file_per_table

write_credentials

load_dbase_schema sakila
load_dbase_data sakila

#PXB-2279 Xbcloud: Upload failed: backup is incomplete
vlog "take full backup to test compression"
xtrabackup --backup --stream=xbstream --compress --extra-lsndir=$full_backup_dir \
	   --target-dir=$full_backup_dir | \
    run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf put \
	    --parallel=4 \
	    ${full_backup_name} 2> $topdir/pxb-2279.log
if  grep -q "Upload failed" $topdir/pxb-2279.log ; then
    die 'xbcloud exit with error on upload'
fi
vlog "take full backup to test encryption"
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
	${full_backup_name} --parallel=4
rm -r $full_backup_dir
xtrabackup --backup --stream=xbstream  --encrypt=AES256 \
          --encrypt-key="percona_xtrabackup_is_awesome___" --extra-lsndir=$full_backup_dir \
	   --target-dir=$full_backup_dir | \
    run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf put \
	    --parallel=4 \
	    ${full_backup_name} 2> $topdir/pxb-2279.log
if  grep -q "Upload failed" $topdir/pxb-2279.log ; then
    die 'xbcloud exit with error on upload'
fi

#cleanup
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
	${full_backup_name} --parallel=4
