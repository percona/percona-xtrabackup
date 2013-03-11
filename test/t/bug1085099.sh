########################################################################
# Bug #1085099: innobackupex does not pass --tmpdir to xtrabackup
########################################################################

. inc/common.sh

start_server

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t(a INT);
EOF

# Create a new tmpdir
mkdir $topdir/new_tmpdir

# Make the default tmpdir inaccessible and make sure permissions are restored on
# failure
trap "chmod 755 $MYSQLD_TMPDIR" INT TERM EXIT
chmod 000 $MYSQLD_TMPDIR

innobackupex --tmpdir=$topdir/new_tmpdir --no-timestamp $topdir/backup
