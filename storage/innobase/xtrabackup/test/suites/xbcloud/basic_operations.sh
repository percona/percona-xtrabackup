. inc/xbcloud_common.sh
is_xbcloud_credentials_set

start_server --innodb_file_per_table

write_credentials

load_dbase_schema sakila
load_dbase_data sakila

mysql -e "ALTER TABLE payment COMPRESSION='lz4'" sakila
mysql -e "CREATE database emptydatabase"



vlog "take full backup"

xtrabackup --backup --stream=xbstream --extra-lsndir=$full_backup_dir \
	   --target-dir=$full_backup_dir | \
    run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf put \
	    --parallel=4 \
	    ${full_backup_name}

vlog "take incremental backup"

xtrabackup --backup --incremental-basedir=$full_backup_dir \
	   --stream=xbstream --target-dir=inc_backup_dir \
	   --parallel=4 | \
    run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf put \
            --parallel=4 \
	    ${inc_backup_name}

vlog "download and prepare"

mkdir $topdir/downloaded_full
mkdir $topdir/downloaded_inc

run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf get \
        --parallel=4 \
	${full_backup_name} | \
    xbstream -xv -C $topdir/downloaded_full --parallel=4

xtrabackup --prepare --apply-log-only --target-dir=$topdir/downloaded_full

run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf get \
        --parallel=4 \
        ${inc_backup_name} | \
    xbstream -xv -C $topdir/downloaded_inc

xtrabackup --prepare --apply-log-only \
	   --target-dir=$topdir/downloaded_full \
	   --incremental-dir=$topdir/downloaded_inc

xtrabackup --prepare --target-dir=$topdir/downloaded_full

# test partial download

mkdir $topdir/partial

run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf get \
        --parallel=4 \
	${full_backup_name} ibdata1 sakila/payment.ibd \
	> $topdir/partial/partial.xbs

xbstream -xv -C $topdir/partial < $topdir/partial/partial.xbs \
	 2> $topdir/partial/partial.list

sort -o $topdir/partial/partial.list $topdir/partial/partial.list

diff -u $topdir/partial/partial.list - <<EOF
ibdata1
sakila/payment.ibd
EOF

# PXB-1832: Xbcloud does not exit when a piped command fails
xbcloud --defaults-file=$topdir/xbcloud.cnf get ${full_backup_name} 2>$topdir/pxb-1832.log | true
if [ "${PIPESTATUS[0]}" == "0" ] ; then
    die 'xbcloud did not exit with error'
fi

if ! grep -q failed $topdir/pxb-1832.log ; then
    die 'xbcloud did not exit with error'
fi

#PXB-2164 xbcloud doesn't return the error if the backup doesn't exist in s3 bucket
run_cmd_expect_failure xbcloud --defaults-file=$topdir/xbcloud.cnf get somedummyjunkbackup 2>$topdir/pxb-2164.log

if ! grep -q failed $topdir/pxb-2164.log ; then
    die 'xbcloud did not exit with error on get'
fi

#PXB-2198 xbcloud doesn't return the error on delete if the backup doesn't exist in s3 bucket
run_cmd_expect_failure xbcloud --defaults-file=$topdir/xbcloud.cnf delete somedummyjunkbackup 2>$topdir/pxb-2198.log

if ! grep -q "error: backup" $topdir/pxb-2198.log ; then
    die 'xbcloud did not exit with error on delete'
fi

#PXB-2202 Xbcloud does not display an error when xtrabackup fails to create a backup
run_cmd_expect_failure xtrabackup --backup --stream=xbstream --extra-lsndir=$full_backup_dir \
	   --target-dir=/some/unknown/dir | \
    run_cmd_expect_failure xbcloud --defaults-file=$topdir/xbcloud.cnf put \
	    --parallel=4 \
	    somedummyjunkbackup 2>$topdir/pxb-2202.log


if ! grep -q "Upload failed" $topdir/pxb-2202.log ; then
    die 'xbcloud did not exit with error on upload'
fi

#cleanup
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
	${full_backup_name} --parallel=4

run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
  ${inc_backup_name} --parallel=4
