########################################################################
# Bug #1180922: DBD::mysql error
########################################################################

. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="wait_timeout=5"

start_server

backup_dir=$topdir/backup

innobackupex --no-timestamp --debug-sleep-before-unlock=6 $backup_dir
