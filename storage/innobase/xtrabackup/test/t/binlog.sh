
function backup_restore() {

  mkdir $topdir/binlog-dir1
  mkdir $topdir/binlog-dir2

  start_server $*

  mysql -e "SHOW VARIABLES LIKE 'log%'";

  mysql -e "CREATE TABLE t (a INT) engine=InnoDB" test
  mysql -e "INSERT INTO t (a) VALUES (1), (2), (3)" test

  xtrabackup --backup --target-dir=$topdir/backup
  xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup

  mysql -e "INSERT INTO t (a) VALUES (1), (2), (3)" test

  xtrabackup --backup --target-dir=$topdir/backup1 --incremental-basedir=$topdir/backup
  xtrabackup --prepare --incremental-dir=$topdir/backup1 --target-dir=$topdir/backup

  stop_server

  rm -rf $mysql_datadir
  rm -rf $topdir/binlog-dir1
  rm -rf $topdir/binlog-dir2

  xtrabackup --copy-back --target-dir=$topdir/backup $*

  cd $mysql_datadir
  for file in $FILES ; do
    if ! [ -f $file ] ; then
      die "File $file is not found!"
    fi
  done
  cd -

  start_server $*

  mysql -e "SELECT * FROM t" test

  stop_server

  rm -rf $mysql_datadir
  rm -rf $topdir/binlog-dir1
  rm -rf $topdir/binlog-dir2
  rm -rf $topdir/backup
  rm -rf $topdir/backup1
}

FILES=""
backup_restore --skip-log-bin

FILES="mysql-bin.index mysql-bin.000004"
backup_restore --log-bin

FILES="binlog123.index binlog123.000003"
backup_restore --log-bin=binlog123

FILES="binlog898.index binlog123.000003"
backup_restore --log-bin=binlog123 --log-bin-index=binlog898

FILES="binlog898.index binlog123.000003"
backup_restore --log-bin=binlog123 --log-bin-index=binlog898.index

FILES="log.index $topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=log

FILES="$topdir/binlog-dir1/idx.index binlog-abcd.000003"
backup_restore --log-bin=binlog-abcd --log-bin-index=$topdir/binlog-dir1/idx

FILES="$topdir/binlog-dir2/idx.index $topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=$topdir/binlog-dir2/idx

FILES="$topdir/binlog-dir2/idx.index $topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=$topdir/binlog-dir2/idx.index
