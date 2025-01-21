########################################################################
# Test support for separate UNDO tablespace
########################################################################

. inc/common.sh

require_server_version_higher_than 8.0.20

vlog "case #1: block parallel truncation during backup"

start_server
for i in {1..100} ; do
    mysql -e "CREATE UNDO TABLESPACE UNDO_$i ADD DATAFILE 'undo_$i.ibu'"
done;

for i in {1..100} ; do
    mysql -e "ALTER UNDO TABLESPACE UNDO_$i SET INACTIVE"
done &

sleep 1s
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --lock-ddl=OFF --target-dir=$topdir/backup

