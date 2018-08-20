#
# Bug 1647340: warning "option --ssl is deprecated" printed to stdout
#

XB_EXTRA_MY_CNF_OPTS="
ssl
"

start_server

mkdir $topdir/backup

xtrabackup --backup --target-dir=$topdir/backup --stream=xbstream > $topdir/backup/b.xbs
run_cmd xbstream -C $topdir/backup -x < $topdir/backup/b.xbs
