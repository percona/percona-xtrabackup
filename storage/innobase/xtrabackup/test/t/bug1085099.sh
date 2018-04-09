########################################################################
# Bug #1085099: innobackupex does not pass --tmpdir to xtrabackup
########################################################################

. inc/common.sh

# Lower server versions attempt to write to the tmpdir when xtrabackup
# performing specific queries. This test aims to check that xtrabackup
# doesn't access to tmpdir, not mysqld, so we'll just skip the test
# on affected server versions.
require_server_version_higher_than 5.7.0

start_server

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t(a INT);
EOF

# Create a new tmpdir
mkdir $topdir/new_tmpdir

# Make the default tmpdir inaccessible
chmod 000 $MYSQLD_TMPDIR

innobackupex --tmpdir=$topdir/new_tmpdir --no-timestamp $topdir/backup
