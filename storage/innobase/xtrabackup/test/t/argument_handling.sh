. inc/common.sh

my_cnf="[mysqld]
datadir=/some/data/dir
tmpdir=/some/tmp/dir1:/some/tmp/dir2
innodb_undo_tablespaces=8
port=10001
server-conf-unknown-arg=1
[client]
port=10002
xml=1
client-conf-unknown-arg=1
"

echo "$my_cnf" >$topdir/my-arg.cnf

run_cmd_expect_failure $XB_BIN --defaults-file=$topdir/my-arg.cnf --innodb_doublewrite=0 --backup --password=foo --some-arg --target-dir=$topdir/backup 2>&1 | tee $topdir/xb.output

grep 'recognized client arguments' $topdir/xb.output > $topdir/client.args
grep 'recognized server arguments' $topdir/xb.output > $topdir/server.args
grep 'unknown argument: ' $topdir/xb.output > $topdir/unknown.args

if grep "port=10001" $topdir/server.args ; then
  echo "Recognized port as a server argument, which shouldn't happen"
  exit 1
fi

if grep "some-arg" $topdir/server.args ; then
  echo "Recognized some-arg as a server argument, which shouldn't happen"
  exit 1
fi

if grep "some-arg" $topdir/client.args ; then
  echo "Recognized some-arg as a client argument, which shouldn't happen"
  exit 1
fi

if ! grep "some-arg" $topdir/unknown.args ; then
  echo "Some-arg should be an unknown argument, and result in an error."
  exit 1
fi

if grep "server-conf-unknown-arg" $topdir/unknown.args ; then
  echo "Recognized an unknown server argument in the config file, which should have been ignored"
  exit 1
fi

if grep "client-conf-unknown-arg" $topdir/unknown.args ; then
  echo "Recognized an unknown client argument in the config file, which should have been ignored"
  exit 1
fi

if grep "xml" $topdir/client.args ; then
  echo "Recognized xml as a client argument, which shouldn't happen"
  exit 1
fi

if ! grep "port=10002" $topdir/client.args ; then
  echo "Can't find the correct port in the client arguments"
  exit 1
fi

if ! grep "password=\*" $topdir/client.args ; then
  echo "Can't find the hidden password in the client arguments"
  exit 1
fi

if ! grep "innodb_undo_tablespaces=8" $topdir/server.args ; then
  echo "Can't find the correct innodb_undo_tablespaces in the server arguments"
  exit 1
fi

if ! grep "innodb_doublewrite=0" $topdir/server.args ; then
  echo "Can't find the correct innodb_doublewrite in the server arguments"
  exit 1
fi

