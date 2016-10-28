#
# Bug 1415191: apply log crashes - signal 11 (ib_warn_row_too_big)
#

require_server_version_lower_than 5.7.0

start_server

( echo -n "CREATE TABLE test.foo ( "
  for i in {1..197}; do
    echo -n "text${i} TEXT"
    [[ ${i} -ne 197 ]] && echo -n ", "
  done
  echo ") ENGINE = InnoDB" ) | $MYSQL $MYSQL_ARGS test

( echo -n "INSERT INTO test.foo VALUES ("
  for i in {1..197}; do
    echo -n "'abcdef'"
    [[ ${i} -ne 197 ]] && echo -n ", "
  done
  echo ")" ) | $MYSQL $MYSQL_ARGS test

$MYSQL $MYSQL_ARGS -e "DELETE FROM test.foo WHERE text1 = 'abcdef'" test

innobackupex --no-timestamp $topdir/backup

# prepare will crash if bug is present
innobackupex --apply-log $topdir/backup
