############################################################################
# Common code for xb_stream_*.sh tests.
# Expects the following variables to be set appropriately before 
# including:
#   stream_format: passed to the --stream xtrabackup option
#   stream_extract_cmd: shell command to be used to extract the stream
#
# Optionally the following variables may be set:
#   xtrabackup_option:  additional options to be passed to xtrabackup.
#   stream_uncompress_cmd: command used to uncompress data streamed data
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

load_dbase_schema sakila
load_dbase_data sakila

xtrabackup_options=${xtrabackup_options:-""}

# Take backup
mkdir -p $topdir/backup
xtrabackup --backup --stream=$stream_format $xtrabackup_options --target-dir=$topdir/backup > $topdir/backup/out

stop_server

# Remove datadir
rm -r $mysql_datadir

# Restore sakila
vlog "#####################"
vlog "# EXTRACTING STREAM #"
vlog "#####################"
backup_dir=$topdir/backup
cd $backup_dir
run_cmd bash -c "$stream_extract_cmd out"

# PXB-1542: attempt to extract second time must fail with appropriate error code
run_cmd_expect_failure bash -c "$stream_extract_cmd out"

if [ -n "${stream_uncompress_cmd:=""}" ]; then 
  vlog "################################"
  vlog "# DECRYPTING AND DECOMPRESSING #"
  vlog "################################"
  run_cmd bash -c "$stream_uncompress_cmd";
fi
cd - >/dev/null 2>&1 
vlog "###########"
vlog "# PREPARE #"
vlog "###########"
xtrabackup --prepare --target-dir=$backup_dir
mkdir -p $mysql_datadir
vlog "###########"
vlog "# RESTORE #"
vlog "###########"
xtrabackup --copy-back --target-dir=$backup_dir

start_server
# Check sakila
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila

########################################################################
# Bug #1217426: Empty directory is not backed when stream is used
########################################################################

# Check if the 'test' database is accessible by the server
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t(a INT)" test
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT * FROM t" test
