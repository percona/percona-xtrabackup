. inc/common.sh

REMOVE_INDEX_FILE=""
GET_BINLOG_SUFFIX_FROM_SERVER=0

function get_last_binlog_suffix() {
  local result=$(mysql -e "SHOW BINARY LOGS" --skip-column-names -s | sort -r | head -1 | cut -f1)
  [[ -z $result ]] && return 1

  # Remove the base from the filename to get the extension
  echo "${result##*.}"
}

function backup() {
  mkdir $topdir/binlog-dir1
  mkdir $topdir/binlog-dir2

  start_server $*

  mysql -e "SHOW VARIABLES LIKE 'log%'";

  mysql -e "CREATE TABLE t (a INT) engine=InnoDB" test
  mysql -e "INSERT INTO t (a) VALUES (1), (2), (3)" test

  if [[ $GET_BINLOG_SUFFIX_FROM_SERVER -eq 1 ]]; then
    BINLOG_FILENAME="${BINLOG_PREFIX}$(get_last_binlog_suffix)"
    vlog "Using $BINLOG_FILENAME as the last binlog filename (with new binlog suffix)"
  fi

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

  if [[ $REMOVE_INDEX_FILE_BEFORE_RESTORE -eq 1 ]]; then
    local backup_index_file

    # To test that we work with backups using older versions of PXB
    # Remove the binlog index file
    if [[ -n $INDEX_FILE ]]; then
      backup_index_file="${topdir}/backup/$(basename $INDEX_FILE)"
      if [[ -e $backup_index_file ]]; then
        vlog "Removing $backup_index_file"
        rm "${backup_index_file}"
      fi
    fi
    if [[ -n $REMOVE_INDEX_FILE ]]; then
      backup_index_file="${topdir}/backup/$(basename $REMOVE_INDEX_FILE)"
      if [[ -e $backup_index_file ]]; then
        vlog "Removing $backup_index_file"
        rm "${backup_index_file}"
      fi
    fi
  fi

  xtrabackup --copy-back --target-dir=$topdir/backup $*

  cd $mysql_datadir

  # Verify that all the required files exist
  if ! [ -f $BINLOG_FILENAME ] ; then
    die "TEST: File $BINLOG_FILENAME is not found!"
  fi

  # Verify that the binlog index file is correct
  if [[ -n $INDEX_FILE ]]; then
    if ! [[ -r $INDEX_FILE ]]; then
      die "TEST: Missing binlog index file: $INDEX_FILE"
    fi
    while read file
    do
      if ! [[ -r $file ]]; then
        die "TEST: File $file in $INDEX_FILE cannot be found!"
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

# We run each test twice
# (1) the normal backup/restore
# (2) run backup, remove binlog index, restore
#     this simulates restoring from a backup created
#     by an earlier version of PXB 2.4 (which didn't include
#     the binlog index files in the backup).
#
# Test variables
#   INDEX_FILE
#     Specifies the location of the Binlog Index file for the server.
#     After the restore, this is checked for existence and the contents
#     are checked to see if the binlog files exist.
#   BINLOG_FILENAME
#     Specifies the location of the binlog file (in the destination).
#     This must be set if GET_BINLOG_SUFFIX_FROM_SERVER is set to 0.
#   GET_BINLOG_SUFFIX_FROM_SERVER
#     If xet to '1', then the suffix for the last binlog file will
#     will be retrieved from the server.  This will then be appended
#     to the BINLOG_PREFIX value to become the BINLOG_FILENAME value.
#     This is useful when the name of the binlog files is changed.
#   BINLOG_PREFIX
#     This is only used when GET_BINLOG_SUFFIX_FROM_SERVER is 1.
#
#   REMOVE_INDEX_FILE_BEFORE_RESTORE
#     If this is set to 1, the binlog index file (which is set
#     in the REMOVE_INDEX_FILE variable), will be removed before
#     the copy-back is performed.  This is used to simulate the
#     behavior of older PXB versions.
#   REMOVE_INDEX_FILE
#     The name of the binlog file that will be removed from the
#     backup dir.
#
for i in {1..2}; do

  if [[ $i -eq 1 ]]; then
    REMOVE_INDEX_FILE_BEFORE_RESTORE=0
  else
    REMOVE_INDEX_FILE_BEFORE_RESTORE=1
  fi

  vlog "------- TEST $i.1 -------"
  INDEX_FILE=""
  BINLOG_FILENAME=""
  GET_BINLOG_SUFFIX_FROM_SERVER=0
  backup_restore --skip-log-bin

  vlog "------- TEST $i.2 -------"
  INDEX_FILE="mysql-bin.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="mysql-bin."
  backup_restore --log-bin

  vlog "------- TEST $i.3 -------"
  INDEX_FILE="binlog123.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="binlog123."
  backup_restore --log-bin=binlog123

  vlog "------- TEST $i.4 -------"
  INDEX_FILE="binlog898.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="binlog123."
  backup_restore --log-bin=binlog123 --log-bin-index=binlog898

  vlog "------- TEST $i.5 -------"
  INDEX_FILE="binlog898.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="binlog123."
  backup_restore --log-bin=binlog123 --log-bin-index=binlog898.index

  vlog "------- TEST $i.6 -------"
  INDEX_FILE="log.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="${topdir}/binlog-dir1/bin."
  backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=log

  vlog "------- TEST $i.7 -------"
  INDEX_FILE="$topdir/binlog-dir1/idx.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="binlog-abcd."
  backup_restore --log-bin=binlog-abcd --log-bin-index=$topdir/binlog-dir1/idx

  vlog "------- TEST $i.8 -------"
  INDEX_FILE="$topdir/binlog-dir2/idx.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="${topdir}/binlog-dir1/bin."
  backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=$topdir/binlog-dir2/idx

  vlog "------- TEST $i.9 -------"
  INDEX_FILE="$topdir/binlog-dir2/idx.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="${topdir}/binlog-dir1/bin."
  backup_restore --log-bin=$topdir/binlog-dir1/bin --log-bin-index=$topdir/binlog-dir2/idx.index

  vlog "------- TEST $i.10 -------"
  INDEX_FILE="$topdir/binlog-dir1/bin.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="${topdir}/binlog-dir1/bin."
  backup_restore --log-bin=$topdir/binlog-dir1/bin

  vlog "------- TEST $i.11 -------"
  # PXB-2106 : we have an absolute path for the log-bin and it
  # is within the datadir
  INDEX_FILE="idx.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="${mysql_datadir}/bin."
  backup_restore --log-bin=$mysql_datadir/bin --log-bin-index=idx

  # do the same, but with updating my.cnf

  vlog "------- TEST $i.101 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  skip-log-bin
  "
  INDEX_FILE=""
  BINLOG_FILENAME=""
  GET_BINLOG_SUFFIX_FROM_SERVER=0
  backup_restore --skip-log-bin

  vlog "------- TEST $i.102 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin
  "
  INDEX_FILE="mysql-bin.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="./mysql-bin."
  backup_restore

  vlog "------- TEST $i.103 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=binlog123
  "
  INDEX_FILE="binlog123.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="./binlog123."
  backup_restore

  vlog "------- TEST $i.104 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=binlog123
  log-bin-index=binlog898
  "
  INDEX_FILE="binlog898.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="./binlog123."
  backup_restore

  vlog "------- TEST $i.105 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=binlog123
  log-bin-index=binlog898.index
  "
  INDEX_FILE="binlog898.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="./binlog123."
  backup_restore

  vlog "------- TEST $i.106 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=$topdir/binlog-dir1/bin
  log-bin-index=log
  "
  INDEX_FILE="log.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="$topdir/binlog-dir1/bin."
  backup_restore

  vlog "------- TEST $i.107 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=binlog-abcd
  log-bin-index=$topdir/binlog-dir1/idx
  "
  INDEX_FILE="$topdir/binlog-dir1/idx.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="binlog-abcd."
  backup_restore

  vlog "------- TEST $i.108 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=$topdir/binlog-dir1/bin
  log-bin-index=$topdir/binlog-dir2/idx
  "
  INDEX_FILE="$topdir/binlog-dir2/idx.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="$topdir/binlog-dir1/bin."
  backup_restore

  vlog "------- TEST $i.109 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=$topdir/binlog-dir1/bin
  log-bin-index=$topdir/binlog-dir2/idx.index
  "
  INDEX_FILE="$topdir/binlog-dir2/idx.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="$topdir/binlog-dir1/bin."
  backup_restore

  vlog "------- TEST $i.110 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=$topdir/binlog-dir1/bin
  "
  INDEX_FILE="$topdir/binlog-dir1/bin.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="$topdir/binlog-dir1/bin."
  backup_restore --log-bin=$topdir/binlog-dir1/bin

  vlog "------- TEST $i.111 -------"
  # PXB-2106 : we have an absolute path for the log-bin and it
  # is within the datadir
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=$mysql_datadir/bin
  log-bin-index=idx
  "
  INDEX_FILE="idx.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="$mysql_datadir/bin."
  backup_restore

  # PXB-1810 - restore binlog to the different location

  vlog "------- TEST $i.201 -------"
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=$topdir/binlog-dir1/bin
  log-bin-index=$topdir/binlog-dir1/idx
  "
  INDEX_FILE="idx.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="bin."
  backup
  restore --log-bin=bin --log-bin-index=idx

  # Only run this test if we are copying the binlog index
  # Otherwise PXB will fail the copy-back because we can't
  # find the binlog index or binlog files.
  if [[ $REMOVE_INDEX_FILE_BEFORE_RESTORE -eq 0 ]]; then
    vlog "------- TEST $i.202 -------"
    MYSQLD_EXTRA_MY_CNF_OPTS="
    log-bin=bin
    log-bin-index=idx
    "
    # There is no binlog index file, since we cannot
    # find any binlogs (due to the name difference)
    INDEX_FILE="logidx.index"
    GET_BINLOG_SUFFIX_FROM_SERVER=1
    BINLOG_PREFIX="logbin."
    # Remove this file from the backup dir
    # (to simulate restoring from an older PXB version)
    REMOVE_INDEX_FILE="idx.index"
    backup
    restore --log-bin=logbin --log-bin-index=logidx
    REMOVE_INDEX_FILE=""
    GET_BINLOG_SUFFIX_FROM_SERVER=0

    vlog "------- TEST $i.203 -------"
    MYSQLD_EXTRA_MY_CNF_OPTS="
    log-bin=$topdir/binlog-dir1/bin
    log-bin-index=$topdir/binlog-dir1/bin
    "
    # There is no binlog index file, since we cannot
    # find any binlogs (due to the name difference)
    INDEX_FILE="$topdir/binlog-dir2/bin.index"
    GET_BINLOG_SUFFIX_FROM_SERVER=1
    BINLOG_PREFIX="$topdir/binlog-dir1/logbin."
    # Remove this file from the backup dir
    # (to simulate restoring from an older PXB version)
    REMOVE_INDEX_FILE="bin.index"
    backup
    restore --log-bin=$topdir/binlog-dir1/logbin --log-bin-index=$topdir/binlog-dir2/bin.index

    GET_BINLOG_SUFFIX_FROM_SERVER=""
  fi

  # Cleanup
  MYSQLD_EXTRA_MY_CNF_OPTS=""


  # Test for binlog name with a single period
  vlog "------- TEST $i.301 -------"
  INDEX_FILE="my.index"
  GET_BINLOG_SUFFIX_FROM_SERVER=1
  BINLOG_PREFIX="my."
  backup_restore --log-bin=my.bin

  # Test for binlog name with two periods
  # Entered as a MySQL bug : https://bugs.mysql.com/bug.php?id=103404
  # MySQL creates a single unnumbered binlog file
  vlog "------- TEST $i.302 -------"
  INDEX_FILE="my.index"

  if is_server_version_higher_than 5.7.0; then
    GET_BINLOG_SUFFIX_FROM_SERVER=0
    BINLOG_FILENAME="my.log"
  else
    # If this is using a server with 5.6, then
    # we can use the normal rules.
    GET_BINLOG_SUFFIX_FROM_SERVER=1
    BINLOG_PREFIX="my."
  fi
  backup_restore --log-bin=my.log.bin


  # Test for binlog name with a two periods
  vlog "------- TEST $i.303 -------"
  INDEX_FILE="my.index"
  if is_server_version_higher_than 5.7.0; then
    GET_BINLOG_SUFFIX_FROM_SERVER=0
    BINLOG_FILENAME="my.sql"
  else
    # If this is using a server with 5.6, then
    # we can use the normal rules.
    GET_BINLOG_SUFFIX_FROM_SERVER=1
    BINLOG_PREFIX="my."
  fi
  backup_restore --log-bin=my.sql.bin


  # Test for binlog name with a two periods
  vlog "------- TEST $i.304 -------"
  INDEX_FILE="my.sql.idx"

  # The log-bin-index must also be specified in the config file
  # becauxe the server will not provide the correct value
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=my.sql.bin
  log-bin-index=my.sql.idx
  "
  if is_server_version_higher_than 5.7.0; then
    GET_BINLOG_SUFFIX_FROM_SERVER=0
    BINLOG_FILENAME="my.sql"
  else
    # If this is using a server with 5.6, then
    # we can use the normal rules.
    GET_BINLOG_SUFFIX_FROM_SERVER=1
    BINLOG_PREFIX="my."
  fi
  backup_restore --log-bin=my.sql.bin --log-bin-index=my.sql.idx
  MYSQLD_EXTRA_MY_CNF_OPTS=""


  # Test for binlog name with a two periods
  vlog "------- TEST $i.305 -------"
  INDEX_FILE="my.sql.idx.index"
  if is_server_version_higher_than 5.7.0; then
    GET_BINLOG_SUFFIX_FROM_SERVER=0
    BINLOG_FILENAME="my.sql"
  else
    # If this is using a server with 5.6, then
    # we can use the normal rules.
    GET_BINLOG_SUFFIX_FROM_SERVER=1
    BINLOG_PREFIX="my."
  fi

  # The log-bin-index must also be specified in the config file
  # becauxe the server will not provide the correct value
  MYSQLD_EXTRA_MY_CNF_OPTS="
  log-bin=my.sql.bin
  log-bin-index=my.sql.idx.index
  "
  backup_restore --log-bin=my.sql.bin --log-bin-index=my.sql.idx.index
  MYSQLD_EXTRA_MY_CNF_OPTS=""

done

vlog "-------- TEST END"
