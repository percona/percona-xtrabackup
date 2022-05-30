###############################################################################
# check password stored in ps output during decrypt
###############################################################################

###############################################################################

require_debug_pxb_version
. inc/common.sh
password_string=secretpwd

MYSQLD_EXTRA_MY_CNF_OPTS="
secure-file-priv=$TEST_VAR_ROOT
"
vlog "Preparing server"
start_server
mysql -e "CREATE USER pxb@localhost IDENTIFIED BY '$password_string'"
### create table in MyISAM to let backup wait until there is pending update
mysql -e "GRANT ALL ON *.* TO pxb@localhost "
mysql -e "CREATE TABLE t1(i int) engine=MYISAM" test
mysql -e "INSERT INTO t1 values(1)" test
mkdir -p $topdir/backup

##grep for encryption key while decrypting
function grep_in_datadir_ps_decrypt() {

  additional_options=$1

  xtrabackup --backup $additional_options --target-dir=$topdir/backup

  $XB_BIN $XB_ARGS --decrypt=AES256 $additional_options \
  --target-dir=$topdir/backup \
           --debug-sync="decrypt_decompress_func" \
       2> >( tee $topdir/decrypt.log)&
  job_pid=$!
  pid_file=$topdir/backup/xtrabackup_debug_sync
  wait_for_xb_to_suspend $pid_file
  xb_pid=`cat $pid_file`

  echo "backup pid is $job_pid"

  vlog "check for $password_string in xbcrypt ps with $job_pid "
  ps_output=$(ps -ef | grep $job_pid | grep xtrabackup | grep -v grep)
  echo "ps output xbcrypt is ${ps_output}"
  if [[ ${ps_output} == *${password_string}* ]];  then
     die "found $password_string string in $ps_output"
  fi

  # Resume the xtrabackup process
  vlog "Resuming xtrabackup"
  kill -SIGCONT $xb_pid
  run_cmd wait $job_pid

}

vlog "check for encryption string during decryption"
grep_in_datadir_ps_decrypt " --encrypt=AES256 --encrypt-key=r______$password_string"
