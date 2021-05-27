. inc/common.sh

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

vlog "------- TEST 1 -------"
INDEX_FILE=""
FILES=""
backup_restore --skip-log-bin

vlog "------- TEST 2 -------"
INDEX_FILE="mysql-bin.index"
FILES="mysql-bin.000004"
backup_restore --log-bin

vlog "------- TEST 3 -------"
INDEX_FILE="binlog123.index"
FILES="binlog123.000003"
backup_restore --log-bin=binlog123

vlog "------- TEST 4 -------"
INDEX_FILE="binlog898.index"
FILES="binlog123.000003"
backup_restore --log-bin=binlog123 --log-bin-index=binlog898

vlog "------- TEST 5 -------"
INDEX_FILE="binlog898.index"
FILES="binlog123.000003"
backup_restore --log-bin=binlog123 --log-bin-index=binlog898.index

vlog "------- TEST 6 -------"
INDEX_FILE="log.index"
FILES="$topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=log

vlog "------- TEST 7 -------"
INDEX_FILE="$topdir/binlog-dir1/idx.index"
FILES="binlog-abcd.000003"
backup_restore --log-bin=binlog-abcd --log-bin-index=$topdir/binlog-dir1/idx

vlog "------- TEST 8 -------"
INDEX_FILE="$topdir/binlog-dir2/idx.index"
FILES="$topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=$topdir/binlog-dir2/idx

vlog "------- TEST 9 -------"
INDEX_FILE="$topdir/binlog-dir2/idx.index"
FILES="$topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=$topdir/binlog-dir2/idx.index

vlog "------- TEST 10 -------"
INDEX_FILE="$topdir/binlog-dir1/bin.index"
FILES="$topdir/binlog-dir1/bin.000003"
backup_restore --log-bin=$topdir/binlog-dir1/bin

vlog "------- TEST 11 -------"
# PXB-2106 : we have an absolute path for the log-bin and it
# is within the datadir
INDEX_FILE="idx.index"
FILES="$mysql_datadir/bin.000003"
backup_restore --log-bin=$mysql_datadir/bin --log-bin-index=idx

# do the same, but with updating my.cnf

vlog "------- TEST 101 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
skip-log-bin
"
INDEX_FILE=""
FILES=""
backup_restore

vlog "------- TEST 102 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin
"
INDEX_FILE="mysql-bin.index"
FILES="mysql-bin.000004"
backup_restore

vlog "------- TEST 103 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog123
"
INDEX_FILE="binlog123.index"
FILES="binlog123.000004"
backup_restore

vlog "------- TEST 104 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog123
log-bin-index=binlog898
"
INDEX_FILE="binlog898.index"
FILES="binlog123.000004"
backup_restore

vlog "------- TEST 105 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog123
log-bin-index=binlog898.index
"
INDEX_FILE="binlog898.index"
FILES="binlog123.000004"
backup_restore

vlog "------- TEST 106 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=log
"
INDEX_FILE="log.index"
FILES="$topdir/binlog-dir1/bin.000004"
backup_restore

vlog "------- TEST 107 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=binlog-abcd
log-bin-index=$topdir/binlog-dir1/idx
"
INDEX_FILE="$topdir/binlog-dir1/idx.index"
FILES="binlog-abcd.000004"
backup_restore

vlog "------- TEST 108 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir2/idx
"
INDEX_FILE="$topdir/binlog-dir2/idx.index"
FILES="$topdir/binlog-dir1/bin.000004"
backup_restore

vlog "------- TEST 109 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir2/idx.index
"
INDEX_FILE="$topdir/binlog-dir2/idx.index"
FILES="$topdir/binlog-dir1/bin.000004"
backup_restore

vlog "------- TEST 110 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
"
INDEX_FILE="$topdir/binlog-dir1/bin.index"
FILES="$topdir/binlog-dir1/bin.000004"
backup_restore --log-bin=$topdir/binlog-dir1/bin

vlog "------- TEST 111 -------"
# PXB-2106 : we have an absolute path for the log-bin and it
# is within the datadir
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$mysql_datadir/bin
log-bin-index=idx
"
INDEX_FILE="idx.index"
FILES="$mysql_datadir/bin.000004"
backup_restore

# PXB-1810 - restore binlog to the different location

vlog "------- TEST 201 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir1/idx
"
INDEX_FILE="idx.index"
FILES="bin.000004"
backup
restore --log-bin=bin --log-bin-index=idx

vlog "------- TEST 202 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=bin
log-bin-index=idx
"
INDEX_FILE="logidx.index"
FILES="logbin.000004"
backup
restore --log-bin=logbin --log-bin-index=logidx

vlog "------- TEST 203 -------"
MYSQLD_EXTRA_MY_CNF_OPTS="
log-bin=$topdir/binlog-dir1/bin
log-bin-index=$topdir/binlog-dir1/bin
"
INDEX_FILE="$topdir/binlog-dir2/bin.index"
FILES="$topdir/binlog-dir1/logbin.000004"
backup
restore --log-bin=$topdir/binlog-dir1/logbin --log-bin-index=$topdir/binlog-dir2/bin.index

# Cleanup
MYSQLD_EXTRA_MY_CNF_OPTS=""


# Test for binlog name with a single period
vlog "------- TEST 301 -------"
INDEX_FILE="my.index"
FILES="my.000003"
backup_restore --log-bin=my.bin

# Test for binlog name with two periods
# Entered as a MySQL bug : https://bugs.mysql.com/bug.php?id=103404
# MySQL creates a single unnumbered binlog file
vlog "------- TEST 302 -------"
INDEX_FILE="my.index"
FILES="my.log"
backup_restore --log-bin=my.log.bin
