################################################################################
# Test xbcloud
#
# Set following environment variables to enable this test:
#     XBCLOUD_CREDENTIALS
#
# Example:
#     export XBCLOUD_CREDENTIALS="--storage=swift \
#         --swift-url=http://192.168.8.80:8080/ \
#         --swift-user=test:tester \
#         --swift-key=testing \
#         --swift-container=test_backup"
#
################################################################################

. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="
secure-file-priv=$TEST_VAR_ROOT
"
# run this test only when backing up MySQL 5.7+
# to reduce the load on swift server
require_server_version_higher_than 5.7.0
is_galera && skip_test "skipping"

[ "${XBCLOUD_CREDENTIALS:-unset}" == "unset" ] && \
    skip_test "Requires XBCLOUD_CREDENTIALS"

start_server --innodb_file_per_table

# write credentials into xbcloud.cnf
echo '[xbcloud]' > $topdir/xbcloud.cnf
echo ${XBCLOUD_CREDENTIALS} | sed 's/ *--/\'$'\n/g' >> $topdir/xbcloud.cnf

load_dbase_schema sakila
load_dbase_data sakila

mysql -e "ALTER TABLE payment COMPRESSION='lz4'" sakila
mysql -e "CREATE database emptydatabase"

now=$(date +%s)
pwdpart=($(pwd | md5sum))
full_backup_name=${now}-${pwdpart}-full_backup
inc_backup_name=${now}-${pwdpart}-inc_backup
storage_class_folder=${now}-${pwdpart}-storage_class

full_backup_dir=$topdir/${full_backup_name}
inc_backup_dir=$topdir/${inc_backup_name}

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

#PXB-2279 Xbcloud: Upload failed: backup is incomplete
vlog "take full backup to test compression"
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
	${full_backup_name} --parallel=4
rm -r $full_backup_dir
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

# PXB-2112 - Support for storage class
test_storage_class=false

if grep -q "storage=s3" $topdir/xbcloud.cnf ; then
  vlog "Testing S3 storage class"
  test_storage_class=true
  storage_class_parameter="--s3-storage-class=GLACIER"
elif grep -q "storage=google" $topdir/xbcloud.cnf ; then
  vlog "Testing GC storage class"
  test_storage_class=true
  storage_class_parameter="--google-storage-class=COLDLINE"
fi

if grep -q "minioadmin" $topdir/xbcloud.cnf ; then
  test_storage_class=false
fi

if [ "$test_storage_class" = true ]; then
  run_cmd touch $topdir/xtrabackup_tablespaces
  cd $topdir/
  xbstream -c xtrabackup_tablespaces | \
  xbcloud --defaults-file=$topdir/xbcloud.cnf ${storage_class_parameter} \
   put ${storage_class_folder} 2> $topdir/storage_class.log

 if  grep -q "expected objects using storage class" $topdir/storage_class.log ; then
    xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
     ${storage_class_folder}
     die 'xbcloud did not upload using correct storage class'
 fi
 if  grep -q "Upload failed" $topdir/storage_class.log ; then
   xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
     ${storage_class_folder}
     die 'xbcloud exit with error on upload'
 fi
 run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
  ${storage_class_folder}
fi

# cleanup
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
	${full_backup_name} --parallel=4
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
	${inc_backup_name} --parallel=4
rm -rf ${full_backup_dir}
rm -rf $topdir/downloaded_full


#check if password is stored in backup directory or in ps output

## grep for all key in secret_key
params=($(echo $XBCLOUD_CREDENTIALS | tr ";" "\n"))
secret_keys=();
for param in "${params[@]}"
do
 if [[ $param =~ "key=" ]]; then
	 secret_keys+=(`echo $param | awk -F 'key=' '{print $2}'`)
 elif [[ $param =~ "password=" ]]; then
	 secret_keys+=(`echo $param | awk -F 'password=' '{print $2}'`)
 elif [[ $param =~ "account=" ]]; then
	 secret_keys+=(`echo $param | awk -F 'account=' '{print $2}'`)
 fi
done

### create table in MyISAM to let backup wait until there is pending update
mysql -e "CREATE TABLE t1(i int) engine=MYISAM" test
mysql -e "INSERT into t1 values(100)" test

### The running update on myisam table will hold access to the table.
### PXB will do FLUSH TABLES on MyISAM tables which requires access
### to the table and waits for the running update on MyISAM to finish.
mysql -e "SELECT SLEEP(1000) from t1 for update " test &

while ! mysql -e 'SHOW PROCESSLIST' | grep 'SELECT SLEEP' ; do
    sleep 1;
done

xtrabackup --backup --stream=xbstream --extra-lsndir=$full_backup_dir \
	   --target-dir=$full_backup_dir | \
    xbcloud $XBCLOUD_CREDENTIALS put --parallel=4 \
     ${full_backup_name}  2> >( tee $topdir/pxb_2723.log)&
job_pid=$!

wait_for_file_to_generated $topdir/pxb_2723.log

#sleep is to ensure password is removed by xbcloud
while ! grep -q 'Successfully connected' $topdir/pxb_2723.log
do
	sleep 1;
done

vlog "check if password is stored in ps output"
for password_string in "${secret_keys[@]}"
do
 vlog "check for $password_string string in ps with $job_pid "
 if ps -ef | grep $job_pid | grep  "xbcloud.*$password_string" | grep -v grep
 then
     die "found $password_string string in ps output"
 fi
done

vlog "killing mysql query"
kill_query_pattern "time > 0 && INFO like '%SLEEP%'"

run_cmd wait $job_pid

vlog "check if password is stored in data directory "
mkdir -p $topdir/downloaded_full
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf get \
        --parallel=4 \
	${full_backup_name} | \
    xbstream -xv -C $topdir/downloaded_full --parallel=4
for password_string in "${secret_keys[@]}"
do
  vlog "check for $password_string in backup directory"
  if grep -rq $password_string $topdir/downloaded_full
  then
     grep -r $password_string $topdir/downloaded_full
     die "found $password_string string in $topdir/downloaded_full"
  fi
done

# cleanup
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
	${full_backup_name} --parallel=4
rm -rf ${full_backup_dir}
