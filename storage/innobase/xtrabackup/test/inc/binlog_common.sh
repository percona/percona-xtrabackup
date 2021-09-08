# below variables get populated at switch_server, but some tests use them
# even bofore starting the server. Adding place holder here;
vardir="${TEST_VAR_ROOT}/var1"
topdir="${vardir}"
mysql_datadir="${vardir}/data"

# Set REMOVE_INDEX_FILE_BEFORE_RESTORE to 1 to force the test
# to delete the index file from the backup before running restore.
# This is used to that restore will work with earlier versions of
# PXB that did not backup the index file.
# This will remove the INDEX_FILE and REMOVE_INDEX_FILE
REMOVE_INDEX_FILE_BEFORE_RESTORE=0

# This variable is used to indicate the index file in the backup
# directory that will be removed.  This is kept separate from
# INDEX_FILE in the case where the index file name is not as expected
# (for instance, if it's in a different directory).
REMOVE_INDEX_FILE=""

# If this variable is set to 1, then we try to get the last binlog
# suffix from the server.  This suffix is then appended to the
# BINLOG_PREFIX variable to obtain the name of the last binlog.
# If this variable is set to 0, then the BINLOG_FILENAME variable
# is used as the name of the last binlog.
GET_BINLOG_SUFFIX_FROM_SERVER=0

BINLOG_PREFIX=""
BINLOG_FILENAME=""

# The MYSQLD_LOGBIN_OPTION can be set to change the default value
# set (in the config file) used when starting the server.
# (default value: "log-bin=mysql-bin")
# MYSQLD_LOGBIN_OPTION=""


function get_last_binlog_suffix() {
  local result=$(mysql -e "SHOW BINARY LOGS" --skip-column-names -s | sort -r | head -1 | cut -f1)
  [[ -z $result ]] && return 1

  # Remove the base from the filename to get the extension
  echo "${result##*.}"
}


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

  if [[ $GET_BINLOG_SUFFIX_FROM_SERVER -eq 1 ]]; then
    BINLOG_FILENAME="${BINLOG_PREFIX}$(get_last_binlog_suffix)"
    vlog "Using $BINLOG_FILENAME as the last binlog filename (with new binlog suffix)"
  fi

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
    die "TEST: File $BINLOG_FILENAME was not found!"
  fi

  # Verify that the binlog index file is correct
  if [[ -n $INDEX_FILE ]]; then
    if ! [[ -r $INDEX_FILE ]]; then
      die "TEST: Missing binlog index file: $INDEX_FILE"
    fi
    while read file
    do
      if ! [[ -r $file ]]; then
        die "TEST: File $file in $INDEX_FILE was not found!"
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

