###############################################################################
# Test history-on-server feature
###############################################################################


###############################################################################
# Gets a single column value from the last history record added
function get_one_value()
{
    local column=$1
    val=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT $column FROM PERCONA_SCHEMA.xtrabackup_history ORDER BY start_time DESC LIMIT 1"`
}


###############################################################################
# Checks a single column from the last history record added for some value and
# not NULL.
function check_for_not_NULL()
{
    local column=$1
    get_one_value "$column"
    if [ -z "$val" ] || [ "$val" = "NULL" ];
    then
        vlog "Error: $column in history record invalid, expected NOT NULL, got \"$val\""
        exit 1
    fi
}


###############################################################################
# Checks a single column from the last history record added to see if is a
# specific value.
function check_for_value()
{
    local column=$1
    shift
    get_one_value "$column"
    if [ -z "$val" ] || [ "$val" != "$@" ];
    then
        vlog "Error: $column in history record invalid, got \"$val\" expected \"$@\""
        exit 1
    fi
}


###############################################################################
vlog "Prepping server"
start_server
load_dbase_schema incremental_sample
multi_row_insert incremental_sample.test \({1..100},100\)
backup_dir=$topdir/backups
mkdir $backup_dir


###############################################################################
# This tests the basic creation of a history record and that fields are
# populated with some data. It also tests specifically that
# partial, incremental, compact, compressed, encrypted and format are exactly
# the correct values after the backup.
# Missing is a test that binlog_pos is NULL, that would require restarting the
# server without the log-bin option in the .cnf file but that has been tested
# manually and doesn't seem to be something that would be required to be
# validated.
vlog "Testing basic history record"
xtrabackup --backup --history=test1 --stream=xbstream \
--target-dir=$backup_dir/`date +%s` > /dev/null

for column in uuid name tool_name tool_command tool_version ibbackup_version \
server_version start_time end_time lock_time binlog_pos innodb_from_lsn \
innodb_to_lsn
do
    check_for_not_NULL "$column"
done

for column in partial incremental compact compressed encrypted
do
    check_for_value "$column" "N"
done

check_for_value "format" "xbstream"
get_one_value "lock_time"

if [ $val -lt 0 ]
then
   vlog "Error: lock_time in history record invalid, expected > 0, got \"$val\""
   exit 1
fi

# saving for later
get_one_value "innodb_to_lsn"
first_to_lsn=$val


###############################################################################
# This tests the taking of an incremental backup based on the last record
# of a history series and validates that the lsns in the record are correct.
# It also tests that format, incremental and compact are exactly the correct
# values after the backup.
vlog "Testing incremental based on history name"

multi_row_insert incremental_sample.test \({101..200},100\)

xtrabackup --backup --history=test1 --no-lock \
--incremental-history-name=test1 --target-dir=$backup_dir/`date +%s` > /dev/null

# saving for later
get_one_value "uuid"
second_uuid=$val
get_one_value "innodb_from_lsn"
second_from_lsn=$val
get_one_value "innodb_to_lsn"
second_to_lsn=$val

check_for_value "format" "file"
check_for_value "incremental" "Y"
check_for_value "compact" "N"
check_for_value "lock_time" "0"

if [ -z "$second_from_lsn" ] || [ "$second_from_lsn" != "$first_to_lsn" ]
then
    vlog "Second backup was not properly based on the to_lsn of the first"
    exit 1
fi

multi_row_insert incremental_sample.test \({201..300},100\)

# This will be a backup based on the last incremental just done, so, its
# innodb_from_lsn (third_from_lsn) should be the same as the value in 
# second_to_lsn. This tests that we find the right record in the test1 series
# out of the two records that should be present before the backup is done.
xtrabackup --backup --history=test1 --incremental-history-name=test1 \
--target-dir=$backup_dir/`date +%s` > /dev/null

# saving for later
get_one_value "uuid"
third_uuid=$val
get_one_value "innodb_from_lsn"
third_from_lsn=$val
get_one_value "innodb_to_lsn"
third_to_lsn=$val

if [ -z "$third_from_lsn" ] || [ "$third_from_lsn" != "$second_to_lsn" ]
then
    vlog "Third backup was not properly based on the to_lsn of the second"
    exit 1
fi


###############################################################################
# This tests that we can base an incremental on a specific history record
# identified by its uuid that we captured earlier from a history record or it
# could be scraped from the output of xtrabackup at some point in the past.
# It also tests specifically that incremental, compressed, encrypted and format
# are exactly the correct values after the backup.
# It tests that --history can be specified, resulting in a history record with
# no name
vlog "Testing incremental based on history uuid"
multi_row_insert incremental_sample.test \({301..400},100\)

xtrabackup --backup --history --incremental-history-uuid=$third_uuid \
--stream=xbstream --compress --encrypt=AES256 \
--encrypt-key=percona_xtrabackup_is_awesome___ --transition-key=percona \
--target-dir=$backup_dir/1 > /dev/null

get_one_value "innodb_from_lsn"
fourth_from_lsn=$val

for column in incremental compressed encrypted
do
    check_for_value "$column" "Y"
done

check_for_value "format" "xbstream"
check_for_value "name" "NULL"

# validate command tool and encrypt key scrubbibng but need to pop off first
# three arguments in the result added by test framework function xtrabackup
get_one_value "tool_command"
val=`set -- $val; shift 2; echo "$@"`
expected_val="--backup --history "\
"--incremental-history-uuid=$third_uuid --stream=xbstream --compress "\
"--encrypt=AES256 --encrypt-key=... --transition-key=... --target-dir=$backup_dir/1"

if [ -z "$val" ] || [ "$val" != "$expected_val" ]
then
  vlog "Error: tool_command in history record invalid, got \"$val\" expected \"$expected_val\""
  exit 1
fi

if [ -z "$fourth_from_lsn" ] || [ "$fourth_from_lsn" != "$third_to_lsn" ]
then
    vlog "Fourth backup was not properly based on the to_lsn of the third"
    exit 1
fi


###############################################################################
# This tests that xtrabackup fails when an invalid --incremental-history-name
# is given.
vlog "Testing bad --incremental-history-name"
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup \
--incremental-history-name=foo --stream=xbstream \
--target-dir=$backup_dir/`date +%s` > /dev/null



###############################################################################
# This tests that xtrabackup fails when an invalid --incremental-history-uuid
# is given.
vlog "Testing bad --incremental-history-uuid"
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup \
--incremental-history-uuid=foo --stream=xbstream \
--target-dir=$backup_dir/`date +%s` > /dev/null


###############################################################################
# PXB-1462 - long gtid_executed breaks --history functionality
. inc/common.sh
if is_server_version_higher_than 5.6.0
then
  stop_server
  start_server --server-id=1 --enforce-gtid-consistency --gtid-mode=ON --log-bin
  ${MYSQL} ${MYSQL_ARGS} -Ns -e "RESET MASTER"
  ${MYSQL} ${MYSQL_ARGS} -Ns -e "SET GLOBAL gtid_purged='aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaab:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaac:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaad:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaae:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaf:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa0:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa1:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa2:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa3:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa4:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa5:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa6:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa7:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa8:1,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa9:1'"
  ${MYSQL} ${MYSQL_ARGS} -Ns -e "DROP TABLE IF EXISTS PERCONA_SCHEMA.xtrabackup_history"
  xtrabackup --backup --history --target-dir=$topdir/backup0
  val=`${MYSQL} ${MYSQL_ARGS} -Ns -e "SELECT COUNT(*) FROM PERCONA_SCHEMA.xtrabackup_history"`
  if [ -z "$val" ] || [ "$val" != "1" ]
  then
      vlog "Error inserting data into xtrabackup_history table"
      exit 1
  fi
else
  vlog "Server does not support GTID"
fi
