# below variables get populated at switch_server, but some tests use them
# even bofore starting the server. Adding place holder here;
vardir="${TEST_VAR_ROOT}/var1"
topdir="${vardir}"
mysql_datadir="${vardir}/data"

function backup() {
  mkdir -p $topdir/binlog-dir1
  mkdir -p $topdir/binlog-dir2

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
}

function restore() {
  xtrabackup --copy-back --target-dir=$topdir/backup $*

  cd $mysql_datadir

  # Verify that all the required files exist
  for file in $FILES ; do
    if ! [ -f $file ] ; then
      die "File $file is not found!"
    fi
  done

  # Verify that the binlog index file is correct
  if [[ -n $INDEX_FILE ]]; then
    if ! [[ -r $INDEX_FILE ]]; then
      die "Missing binlog index file: $INDEX_FILE"
    fi
    while read file
    do
      if ! [[ -r $file ]]; then
        die "File $file in $INDEX_FILE cannot be found!"
      fi
    done < ${INDEX_FILE}
  fi

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

function backup_restore() {
    backup $*
    restore $*
}
