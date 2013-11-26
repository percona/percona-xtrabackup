. inc/common.sh

if ! which rsync > /dev/null 2>&1
then
    skip_test "Requires rsync to be installed"
fi

start_server --innodb_file_per_table
load_sakila

# Bug #1211263: innobackupex unnecessarily calls 'cp' for metadata files
# Create a 'fake' cp command that will be called instead and will always 'fail'
# If any portion of innobackupex script attempts to use cp instead of rsync,
# the backup and subsequently the test will fail.
cat >$topdir/cp <<EOF
#!/bin/sh
false
EOF

chmod +x $topdir/cp

# Calling with $topdir in the path first so the 'fake' cp is used
PATH=$topdir:$PATH innobackupex --rsync --no-timestamp $topdir/backup

stop_server

run_cmd rm -r $mysql_datadir

innobackupex --apply-log $topdir/backup

run_cmd mkdir -p $mysql_datadir

innobackupex --copy-back $topdir/backup

start_server
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT COUNT(*) FROM actor" sakila

########################################################################
# Bug #1217426: Empty directory is not backed when stream is used
########################################################################
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "CREATE TABLE t(a INT)" test
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT * FROM t" test
