###############################################################################
# check if for any password store in data dir thread info or in history table
###############################################################################

###############################################################################

. inc/common.sh
password_string=secretpwd

MYSQLD_EXTRA_MY_CNF_OPTS="
secure-file-priv=$TEST_VAR_ROOT
"
vlog "Preparing server"
start_server
mysql -e "CREATE USER pxb@localhost IDENTIFIED BY '$password_string'" mysql -e "GRANT ALL ON *.* TO pxb@localhost"
### create table in MyISAM to let backup wait until there is pending update
mysql -e "CREATE TABLE t1(i int) engine=MYISAM" test
mysql -e "INSERT INTO t1 values(1)" test
mkdir -p $topdir/backup

#pass option like password -p to be run with

function grep_in_datadir_ps() {

  additional_options=$1
  ### The running update on myisam table will hold access to the table.
  ### PXB will do FLUSH TABLES on MyISAM tables which requires access
  ### to the table and waits for the running update on MyISAM to finish.
  mysql -e "SELECT SLEEP(1000) from t1 for update " test &

  $XB_BIN $XB_ARGS --backup -u pxb $additional_options\
  --transition-key=$password_string --target-dir=$topdir/backup \
  2>&1 | tee $topdir/pxb.log &
  job_pid=$!

  echo "backup pid is $job_pid"

  file=$topdir/backup/xtrabackup_logfile
  if [[ $additional_options == *"encrypt"* ]]; then
	  file+=".xbcrypt"
  fi
  echo "looking for $file"
  wait_for_file_to_generated $file
  vlog "check for $password_string string in ps with $job_pid "
  if ps -ef | grep $job_pid | grep "xtrabackup.*$password_string" | grep -v grep
  then
     die "found $password_string string in ps output"
  fi

  ###kill running mysql query on MyISAM table to let backup resume
  vlog "killing mysql query"
  kill_query_pattern "time > 0 & INFO like '%SLEEP%'"

  run_cmd wait $job_pid

  vlog "check for $password_string string in directory $topdir"
  if grep -rq $password_string $topdir
  then
     grep -r $password_string $topdir
     die "found $password_string string in $topdir "
  fi
  rm -rf $topdir/backup
}

function grep_in_history_table() {
  additional_options=$1
  run_cmd $XB_BIN $XB_ARGS --history --backup -u pxb $additional_options\
  --transition-key=$password_string --target-dir=$topdir/backup 2>&1 | tee $topdir/pxb.log

  row_count=`$MYSQL $MYSQL_ARGS -Ns -e "select count(1) from PERCONA_SCHEMA.xtrabackup_history where tool_command like '%$password_string%'" | awk {'print $1'}`
  if [ "$row_count" != 0 ]; then
    mysql -e "select * from PERCONA_SCHEMA.xtrabackup_history where tool_command like '%$password_string%'"
    die "found entry of $password_string in history table"
  fi
  rm -rf $topdir/backup
}

vlog "check for -p and  encryption string in history table"
grep_in_history_table "-p$password_string --encrypt=AES256 --encrypt-key=r______$password_string"

vlog "check for --password and encryption string in history table"
grep_in_history_table "--password=$password_string --encrypt=AES256 --encrypt-key=r______$password_string"

vlog "check for $password_string with -p option in data directory"
grep_in_datadir_ps "-p$password_string"

vlog "check for $password_string with --password option in data directory"
grep_in_datadir_ps "--password=$password_string"

vlog "check for encryption string in data directory"
grep_in_datadir_ps "--password=$password_string --encrypt=AES256 --encrypt-key=r______$password_string"
