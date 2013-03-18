############################################################################
# Common code for xb_*.sh local backup tests.
# Expects the following variables to be set appropriately before
# including:
#
# Optionally the following variables may be set:
#   innobackupex_option:  additional options to be passed to innobackupex.
#   data_decrypt_cmd: command used to decrypt data
#   data_decompress_cmd: command used to decompress data
############################################################################

. inc/common.sh

start_server --innodb_file_per_table

load_dbase_schema sakila
load_dbase_data sakila

innobackupex_options=${innobackupex_options:-""}
innobackupex_options="${innobackupex_options} --no-timestamp"

# Take backup
backup_dir=${topdir}/backup
innobackupex $innobackupex_options $backup_dir
vlog "Backup created in directory $backup_dir"

stop_server
# Remove datadir
rm -r $mysql_datadir

# Restore sakila
vlog "Applying log"
cd $backup_dir
test -n "${data_decrypt_cmd:=""}" && run_cmd bash -c "$data_decrypt_cmd"
test -n "${data_decompress_cmd:-""}" && run_cmd bash -c "$data_decompress_cmd";
cd - >/dev/null 2>&1
vlog "###########"
vlog "# PREPARE #"
vlog "###########"
innobackupex --apply-log $backup_dir
vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
vlog "###########"
vlog "# RESTORE #"
vlog "###########"
innobackupex --copy-back $backup_dir

start_server
# Check sakila
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT count(*) from actor" sakila
