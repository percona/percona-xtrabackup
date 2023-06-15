. inc/xbcloud_common.sh
is_xbcloud_credentials_set

# call cleanup in case of error
trap 'xbcloud_cleanup' ERR

start_server --innodb_file_per_table

write_credentials

load_dbase_schema sakila
load_dbase_data sakila

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
  if grep -rq $password_string $topdir/downloaded_full/xtrabackup*
  then
     grep -r $password_string $topdir/downloaded_full/xtrabackup*
     die "found $password_string string in $topdir/downloaded_full"
  fi
done

# cleanup
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
	${full_backup_name} --parallel=4
rm -rf ${full_backup_dir}
