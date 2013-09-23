################################################################################
# Test innobackupex --version-check

# There's not much we can do to test full functionality in the test suite. Let's
# just test that the percona-version-check file is created
################################################################################

start_server

# VersionCheck looks for a directory where percona-version-check file is created
# in the following order:
#
#    my @vc_dirs = (
#    '/etc/percona',
#    '/etc/percona-toolkit',
#    '/tmp',
#    "$home",
# );
#
# Assume that both /etc/percona and /etc/percona-toolkit either do not exist or
# are not writable. So we'll be testing /tmp/percona-version-check. There's a
# potential race if there are concurrent tests or other tools using VC, but
# there's currently no way to avoid that.

rm -f /tmp/percona-version-check

innobackupex --no-timestamp $topdir/backup

if [ ! -f /tmp/percona-version-check ]
then
    die "/tmp/percona-version-check has not been created!"
fi

rm -f /tmp/percona-version-check

if ! grep -q "Executing a version check" $OUTFILE
then
    die "Version check has not been performed."
fi
