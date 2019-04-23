
function backup() {
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
}

function restore() {
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

function backup_restore() {
    backup $*
    restore $*
}

vlog "------- TEST 1 -------"
FILES=""
backup_restore --skip-log-bin

vlog "------- TEST 2 -------"
FILES="mysql-bin.index mysql-bin.000004"
backup_restore --log-bin

vlog "------- TEST 3 -------"
FILES="binlog123.index binlog123.000003"
backup_restore --log-bin=binlog123

vlog "------- TEST 4 -------"
FILES="binlog898.index binlog123.000003"
backup_restore --log-bin=binlog123 --log-bin-index=binlog898

vlog "------- TEST 5 -------"
FILES="binlog898.index binlog123.000003"
backup_restore --log-bin=binlog123 --log-bin-index=binlog898.index

vlog "------- TEST 6 -------"
FILES="log.index $topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=log

vlog "------- TEST 7 -------"
FILES="$topdir/binlog-dir1/idx.index binlog-abcd.000003"
backup_restore --log-bin=binlog-abcd --log-bin-index=$topdir/binlog-dir1/idx

vlog "------- TEST 8 -------"
FILES="$topdir/binlog-dir2/idx.index $topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=$topdir/binlog-dir2/idx

vlog "------- TEST 9 -------"
FILES="$topdir/binlog-dir2/idx.index $topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=$topdir/binlog-dir2/idx.index

vlog "------- TEST 10 -------"
FILES="$topdir/binlog-dir1/bin.index $topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin


# do the same, but with updating my.cnf

vlog "------- TEST 101 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
skip-log-bin
"
FILES=""
backup_restore

vlog "------- TEST 102 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin
"
FILES="mysql-bin.index mysql-bin.000004"
backup_restore

vlog "------- TEST 103 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog123
"
FILES="binlog123.index binlog123.000004"
backup_restore

vlog "------- TEST 104 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog123
log-bin-index=binlog898
"
FILES="binlog898.index binlog123.000004"
backup_restore

vlog "------- TEST 105 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog123
log-bin-index=binlog898.index
"
FILES="binlog898.index binlog123.000004"
backup_restore

vlog "------- TEST 106 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=log
"
FILES="log.index $topdir/binlog-dir1/bin.000004"
backup_restore

vlog "------- TEST 107 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog-abcd
log-bin-index=$topdir/binlog-dir1/idx
"
FILES="$topdir/binlog-dir1/idx.index binlog-abcd.000004"
backup_restore

vlog "------- TEST 108 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir2/idx
"
FILES="$topdir/binlog-dir2/idx.index $topdir/binlog-dir1/bin.000004"
backup_restore

vlog "------- TEST 109 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir2/idx.index
"
FILES="$topdir/binlog-dir2/idx.index $topdir/binlog-dir1/bin.000004"
backup_restore

vlog "------- TEST 110 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
"
FILES="$topdir/binlog-dir1/bin.index $topdir/binlog-dir1/bin.000004"
backup_restore --log-bin=$topdir/binlog-dir1/bin


# PXB-1810 - restore binlog to the different location

vlog "------- TEST 201 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir1/idx
"
FILES="idx.index bin.000004"
backup
restore --log-bin=bin --log-bin-index=idx

vlog "------- TEST 202 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=bin
log-bin-index=idx
"
FILES="logidx.index logbin.000004"
backup
restore --log-bin=logbin --log-bin-index=logidx

vlog "------- TEST 203 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir1/bin
"
FILES="$topdir/binlog-dir2/bin.index $topdir/binlog-dir1/logbin.000004"
backup
restore --log-bin=$topdir/binlog-dir1/logbin --log-bin-index=$topdir/binlog-dir2/bin.index
